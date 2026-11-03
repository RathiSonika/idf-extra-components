# SPI NAND Flash: Pluggable GPSPI / MSPI Transport

**Date:** 2026-11-03  
**Status:** Draft for review  
**Component:** `spi_nand_flash`  
**Approach:** Thin NAND transport ops (Approach 1)

## Goal

Reuse one NAND driver for two product shapes without duplicating chip/protocol logic in ESP-IDF:

1. **Independent GPSPI product** — external SPI NAND; application initializes through **WL BDL**.
2. **Primary MSPI product** — NAND holds the app image (and optionally FS data); NOR boots; loader copies app from NAND into **PSRAM** for execution. System/IDF brings up **Raw NAND BDL** on MSPI.

The transport seam lives **inside** `spi_nand_flash`. Below it, ESP-IDF GPSPI (`driver/spi_master`) and MSPI/flash-host (`spi_flash` / memspi / `common_command`) are used **as-is**.

## Non-goals

- Boot ROM / second-bootloader that performs NAND → PSRAM copy and starts execution.
- Final partition / image layout on NAND (driver supplies raw access only).
- True XIP / execute-in-place from SPI NAND.
- Forking or replacing ESP-IDF SPI / flash-host components.
- Hard-coding a single MSPI pin/CS topology (kept flexible until hardware is fixed).

## Requirements (agreed)

| ID | Requirement |
|----|-------------|
| R1 | Same NAND core for GPSPI external and MSPI primary use. |
| R2 | GPSPI apps keep an independent path via WL BDL. |
| R3 | When MSPI is enabled for primary chips, Raw NAND BDL uses the MSPI path. |
| R4 | Loader uses **raw physical** reads (no Dhara/WL). FS may use WL BDL later. |
| R5 | MSPI is not an application-facing “transport menu”; apps do not select `SPI_NAND_TRANSPORT_MSPI`. |
| R6 | Hybrid Kconfig: backends can be stripped; if both built, wiring depends on entry point. |
| R7 | Scope = transport abstraction + raw read support for the loader (not full boot integration). |

## Architecture

```text
GPSPI product (app owns init)          MSPI / primary (system / IDF owns init)
─────────────────────────────          ──────────────────────────────────────
App                                    Bootloader / IDF / board bring-up
  │                                      │
  ▼                                      ▼
WL BDL  (logical pages, Dhara)         Raw NAND BDL
  │                                      │
  ▼                                      │
Raw NAND BDL  ◄──────────────────────────┘
  │
  ▼
NAND core (chip detect, ECC, page/block commands)
  │
  ▼
spi_nand_transport_t   ← owned by spi_nand_flash
  ├─ GPSPI backend  →  esp-idf spi_master (spi_device_transmit)
  └─ MSPI backend   →  esp-idf memspi / flash host (common_command)
```

### Layer ownership

| Layer | Responsibility | Consumers |
|-------|----------------|-----------|
| WL BDL | Wear-leveled logical storage | GPSPI apps (FatFS, etc.); optional later FS on primary |
| Raw NAND BDL | Physical page/block blockdev | Under WL always; MSPI/primary entry + loader |
| NAND core | Protocol, chip support, ECC | Shared — avoid duplicating in IDF |
| Transport | Bus only | GPSPI vs MSPI backends |

### Rules

- Above the transport: NAND protocol only — no `spi_device_*`, no `SPI_TRANS_*`.
- Transport backends call ESP-IDF APIs; they do not reimplement SPI controllers.
- GPSPI remains fully supported for external NAND.
- MSPI is how Raw NAND BDL is brought up for primary products when that backend is compiled in.

## Entry points and public API

### GPSPI / independent product (app-driven)

1. Application: `spi_bus_initialize(HOST_ID, …)` + `spi_bus_add_device(…)` → `spi_device_handle_t`.
2. Application initializes **WL BDL** (existing layered init path), passing the device handle.
3. Stack wires **GPSPI transport** under the shared NAND core and Raw NAND BDL.

`HOST_ID` stays outside the driver (user-chosen GPSPI host). The driver only receives the device handle.

### MSPI / primary product (system-driven)

1. When MSPI support is built in, bootloader / IDF / board code initializes **Raw NAND BDL** with an MSPI attach config.
2. That path wires **MSPI transport** under the same NAND core.
3. Loader uses raw physical reads.
4. Application may later mount WL BDL on the same raw device for FS — not required for load-to-PSRAM.

MSPI is **not** exposed as an app-level transport enum. Normal applications do not “choose MSPI”; they either:

- use the GPSPI WL path for external NAND, or
- consume a primary device already brought up by the system (raw BDL / get-primary style API).

### MSPI attach config (flexible)

Topology is not fixed yet. Config fields should allow later binding without API churn:

| Field | Role |
|-------|------|
| `host_id` | Typically `SPI1_HOST`; not assumed forever |
| `cs_io_num` / `cs_id` | NAND chip-select |
| `freq_mhz` | Clock |
| `io_mode` | SIO / DIO / QIO as supported |
| `input_delay_ns` | Optional timing compensation |

Documented default expectation: SPI1/MSPI; concrete CS sharing with boot NOR is decided when hardware is known.

### Compatibility

- Existing GPSPI + WL/BDL applications keep working.
- Prefer additive APIs and Kconfig; avoid breaking `device_handle`-based init without a migration path.

## Transport interface

Owned by `spi_nand_flash`, used only by NAND oper / core:

```c
typedef struct {
    esp_err_t (*transmit)(void *ctx, spi_nand_transaction_t *t);
    size_t    (*max_transfer)(void *ctx);  /* MSPI may be ~64B; GPSPI larger */
    esp_err_t (*lock)(void *ctx);          /* no-op or SPI1/flash bus lock */
    esp_err_t (*unlock)(void *ctx);
    void      (*deinit)(void *ctx);
    void *ctx;
} spi_nand_transport_t;
```

### GPSPI backend

- `ctx`: `spi_device_handle_t` plus half-duplex / DMA alignment behavior as today.
- `transmit`: map `spi_nand_transaction_t` → `spi_transaction_ext_t` → `spi_device_transmit`.
- `max_transfer`: current GPSPI/DMA limits.
- `lock` / `unlock`: typically no-op (spi_master serializes per device).

### MSPI backend

- `ctx`: IDF memspi / flash-host instance from MSPI attach config.
- `transmit`: map to `spi_flash_trans_t` → `host->driver->common_command`.
- Must **chunk** when length exceeds `max_transfer`.
- `lock` / `unlock`: SPI1 / flash bus lock when sharing with main NOR.
- Uses ESP-IDF MSPI/flash-host APIs as-is.

### Kconfig (hybrid)

- `CONFIG_NAND_FLASH_TRANSPORT_GPSPI` — compile GPSPI backend.
- `CONFIG_NAND_FLASH_TRANSPORT_MSPI` — compile MSPI backend.
- At least one enabled.
- GPSPI-only and MSPI-only products can strip the unused backend.

Wiring is by **entry point** (WL+GPSPI handle vs Raw BDL+MSPI attach), not by an application transport enum.

## Raw loader API and early boot

### API intent

Provide physical read access for copying an app image into PSRAM:

- `spi_nand_flash_read_physical_page(dev, page, buf)` and/or
- `spi_nand_flash_read_physical(dev, page, offset, len, buf)`

Semantics:

- **Physical** addressing (fixed image region), not logical WL pages.
- Path: transport → NAND page-read sequence → ECC/status handling as on today’s raw path.
- No Dhara map, no GC, no trim.
- Raw NAND BDL `read()` may wrap the same core for `esp_blockdev_t` consumers.

### Early-boot constraints (MSPI primary)

- Usable from bootloader / pre-app context (limited heap; possibly no full FreeRTOS).
- MSPI transmit must follow cache-disabled / flash-bus lock rules expected by IDF hosts.
- Prefer static / internal RAM for host context and small bounce buffers; avoid large dynamic allocation on the loader path.
- Loader iterates physical pages (e.g. 2 KiB); does not rely on one giant SPI transfer.

### Error handling

- Propagate `esp_err_t` from transport and NAND status (timeout, ECC fail, program/erase fail).
- Retry / skip / abort-boot policy stays **outside** this driver.
- Raw/loader path must not silently remap bad blocks.

## Testing and validation

- GPSPI regression: existing WL BDL / FatFS / page tests keep behavior.
- Host test: NAND oper against a mock transport (command layer independent of hardware SPI).
- MSPI (when hardware available): raw BDL init; physical page read/write/erase; bus-lock coexistence with main NOR if shared SPI1.
- Loader scenario: read a known physical region into DRAM/PSRAM stand-in and verify contents.
- Build matrix: GPSPI-only, MSPI-only, both enabled.

## Risks

| Risk | Mitigation |
|------|------------|
| MSPI small max transfer | Transport `max_transfer` + mandatory chunking in oper layer |
| SPI1 shared with NOR | Explicit lock/unlock in MSPI backend |
| Early-boot alloc / cache | Minimal primary init; internal RAM; document constraints |
| API confusion (apps selecting MSPI) | No public transport enum; MSPI only via system raw-BDL bring-up |

## Implementation sketch (not yet planned in detail)

1. Introduce `spi_nand_transport_t` and move current `spi_device_transmit` path into GPSPI backend.
2. Make `spi_nand_oper` / NAND core call only the transport seam; remove GPSPI types from that layer.
3. Add MSPI backend wrapping IDF `common_command` + chunking + lock.
4. Wire WL BDL init → GPSPI transport; Raw BDL primary init → MSPI transport.
5. Add physical read API for loader; cover with host/mock tests then hardware.

## Open items

- Concrete MSPI topology (SPI1 + second CS vs dedicated MSPI) — deferred (choice C).
- Exact header split for system/primary APIs vs app-facing WL APIs.
- NAND image / partition layout for app region (owned outside this driver; needed by loader).
- Whether app-stage FS on primary always remounts WL or reuses a handle created in bootloader.
