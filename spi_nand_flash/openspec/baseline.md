# Baseline — `spi_nand_flash` component

> **Artifact type:** Discovery / current-state baseline (not a change proposal).
> **Purpose:** Document the existing system as it stands today so future OpenSpec
> changes can be planned against a shared, accurate picture.
> **Scope rule:** This document describes *what exists today*. It does **not**
> propose new features. Anything that looks forward-looking is confined to
> [§10 “Explicitly out of scope for this baseline”](#10-explicitly-out-of-scope-for-this-baseline)
> (deliberate non-features) or [§11.4 “Known bugs / follow-ups (queued for
> next release)”](#114-known-bugs--follow-ups-queued-for-next-release)
> (defects already confirmed against the tree).

---

## Table of contents

1. [One-paragraph summary](#1-one-paragraph-summary)
2. [What the component does today](#2-what-the-component-does-today)
3. [Module & feature inventory](#3-module--feature-inventory)
4. [External interfaces](#4-external-interfaces)
5. [Architecture & data flow](#5-architecture--data-flow)
6. [Build, configuration & dependency constraints](#6-build-configuration--dependency-constraints)
7. [Supported hardware & targets](#7-supported-hardware--targets)
8. [Testing & validation surface](#8-testing--validation-surface)
9. [Known constraints, invariants & edge cases](#9-known-constraints-invariants--edge-cases)
10. [Explicitly out of scope for this baseline](#10-explicitly-out-of-scope-for-this-baseline)
11. [Unknowns and assumptions](#11-unknowns-and-assumptions)
12. [References](#12-references)

---

## 1. One-paragraph summary

`spi_nand_flash` is an ESP-IDF component that drives SPI-attached NAND flash
chips from multiple vendors and exposes them to applications as logical
read/write/trim pages with wear leveling and bad-block management built in.
The component wraps the external **Dhara** flash translation layer (FTL) to
provide a sector/page abstraction on top of raw NAND, supports SIO/DOUT/DIO/
QOUT/QIO SPI modes, and — starting with component version **1.0.0** on
**ESP-IDF ≥ 6.0** — can additionally expose its raw-flash and wear-leveling
layers as standard `esp_blockdev_t` block devices via an opt-in Block Device
Layer (BDL). A Linux host build substitutes a memory-mapped file emulator so
the same stack can be unit-tested off target.

---

## 2. What the component does today

At a functional level, the component is responsible for:

| # | Responsibility | Where it lives |
|---|---|---|
| 1 | Detect the attached SPI NAND chip via JEDEC manufacturer/device IDs and dispatch to a vendor-specific init routine | `src/nand_impl.c`, `src/devices/nand_<vendor>.c`, `priv_include/nand_flash_devices.h` |
| 2 | Drive the SPI controller to issue NAND command sequences (read, program load, program execute, erase, get feature, set feature, read ID, reset) in SIO/DOUT/DIO/QOUT/QIO modes | `src/spi_nand_oper.c`, `priv_include/spi_nand_oper.h` |
| 3 | Physical-page/block operations on the NAND die (page read, page program, block erase, bad-block check/mark, free-page check, ECC status read) | `src/nand_impl.c`, `priv_include/nand_impl.h`, wrapped by `src/nand_impl_wrap.c` |
| 4 | Logical-sector abstraction with wear leveling, bad-block remapping, TRIM, and garbage collection, implemented by integrating the external Dhara library | `src/dhara_glue.c`, `priv_include/nand.h` |
| 5 | Public C API: init/deinit, page read/write/copy/trim/sync/GC, capacity queries, chip erase | `include/spi_nand_flash.h`, `src/nand.c` |
| 6 | Backward-compatible “sector” API aliases for the page API | `include/spi_nand_flash.h`, `src/nand.c` |
| 7 | Optional `esp_blockdev_t` adapters over both the raw-flash layer and the wear-leveling layer, plus NAND-specific IOCTL extensions (bad-block, ECC stats, page-copy, free-page, flash info) | `include/esp_nand_blockdev.h`, `src/nand_flash_blockdev.c`, `src/nand_wl_blockdev.c` — **compiled only when `CONFIG_NAND_FLASH_ENABLE_BDL=y` and IDF ≥ 6.0** |
| 8 | Diagnostic helpers: bad-block count, ECC error statistics (total / uncorrected / exceeding-threshold) | `include/nand_diag_api.h`, `src/nand_diag_api.c` |
| 9 | Linux host emulation of a NAND chip via a memory-mapped file, used by host tests | `src/nand_impl_linux.c`, `src/nand_linux_mmap_emul.c`, `include/nand_linux_mmap_emul.h` |
| 10 | Optional post-program read-back verification (debug aid) | gated by `CONFIG_NAND_FLASH_VERIFY_WRITE` |
| 11 | Shared test fixtures/helpers reused by both the on-target test app and the Linux host test | `include/spi_nand_flash_test_helpers.h`, `src/spi_nand_flash_test_helpers.c` |
| 12 | Automatic data refresh on soft ECC errors: `spi_nand_flash_read_page` re-programs a page if the corrected-bits count exceeds `chip.ecc_data.ecc_data_refresh_threshold` | `src/nand.c::spi_nand_flash_read_page` (uses `nand_ecc_exceeds_data_refresh_threshold`) |

Explicitly **not** provided by this component today:

- **Filesystem integration.** FATFS support was removed in 1.0.0 and moved to a
  separate sibling component `spi_nand_flash_fatfs`. No filesystem code lives
  in this component anymore.
- **Drivers for parallel/ONFI NAND.** Only SPI NAND is handled.
- **A CLI/host tool.** There is no user-facing utility beyond the test apps.

---

## 3. Module & feature inventory

### 3.1 Public headers (`include/`)

| Header | Always available? | Purpose |
|---|---|---|
| `spi_nand_flash.h` | Yes | Main public API (page + sector + legacy init; BDL init behind `#ifdef CONFIG_NAND_FLASH_ENABLE_BDL`). |
| `nand_device_types.h` | Yes | Shared types: `nand_ecc_status_t`, `nand_ecc_data_t`, `nand_flash_geometry_t`, `nand_device_info_t`. |
| `nand_diag_api.h` | Yes (target builds) | `nand_get_bad_block_stats`, `nand_get_ecc_stats`. |
| `nand_linux_mmap_emul.h` | Linux target only | `nand_file_mmap_emul_config_t` for host emulation. |
| `spi_nand_flash_test_helpers.h` | Yes | Helpers shared by test apps. Today exposes deterministic buffer-pattern utilities: `spi_nand_flash_fill_buffer` / `spi_nand_flash_check_buffer` (fixed pattern) and `spi_nand_flash_fill_buffer_seeded` / `spi_nand_flash_check_buffer_seeded` (caller-supplied seed for distinct data per overwrite round). **Stable public API** per §4.0. |
| `esp_nand_blockdev.h` | **Only when BDL is enabled** | Flash BDL / WL BDL factories + NAND-specific IOCTL commands and argument structs. |
| `nand_private/nand_impl_wrap.h` | Yes | Wrapper API over low-level impl ops. Despite the `nand_private/` path, per §4.0 it is exported from `include/` and is part of the **stable public API** (callers using it are responsible for their own concurrency — see §9.1). |

### 3.2 Private headers (`priv_include/`)

| Header | Purpose |
|---|---|
| `nand.h` | Internal `spi_nand_flash_device_t` structure, Dhara attach/detach hooks. |
| `nand_impl.h` | Low-level page/block operations, bad-block and ECC primitives. |
| `nand_flash_devices.h` | JEDEC manufacturer/device IDs and per-vendor init prototypes. |
| `spi_nand_oper.h` | ESP-side SPI transaction and register helpers (not present on Linux target). |

### 3.3 Source modules (`src/`)

| Source | Always compiled? | Role |
|---|---|---|
| `nand.c` | Yes | Implements the public API and the BDL-only `spi_nand_flash_init_with_layers`. |
| `dhara_glue.c` | Yes | Bridges Dhara’s NAND interface to `nand_impl` and owns the WL layer state. |
| `nand_impl_wrap.c` | Yes | Mutex-protected wrappers around `nand_impl` operations. |
| `nand_impl.c` | Target (non-Linux) only | Real implementation of physical page/block/ECC/bad-block ops and plane selection. |
| `spi_nand_oper.c` | Target (non-Linux) only | SPI transaction layer (SIO/DOUT/DIO/QOUT/QIO). |
| `devices/nand_winbond.c`, `nand_gigadevice.c`, `nand_alliance.c`, `nand_micron.c`, `nand_zetta.c`, `nand_xtx.c` | Target only | Per-vendor feature-register setup and chip-specific quirks. |
| `nand_diag_api.c` | Target only | Diagnostic API implementation. |
| `nand_impl_linux.c` | Linux target only | Linux counterpart to `nand_impl.c` driven by the mmap emulator. |
| `nand_linux_mmap_emul.c` | Linux target only | Memory-mapped file emulation of a NAND device. |
| `spi_nand_flash_test_helpers.c` | Yes | Shared test-support code. |
| `nand_flash_blockdev.c` | **BDL + IDF ≥ 6.0** | `esp_blockdev_t` adapter over the raw-flash layer. |
| `nand_wl_blockdev.c` | **BDL + IDF ≥ 6.0** | `esp_blockdev_t` adapter over the wear-leveling layer. |

### 3.4 Feature matrix

| Feature | Legacy mode (default) | BDL mode (`CONFIG_NAND_FLASH_ENABLE_BDL=y`, IDF ≥ 6.0) |
|---|---|---|
| `spi_nand_flash_init_device()` | Works | **Returns `ESP_ERR_NOT_SUPPORTED`** |
| `spi_nand_flash_init_with_layers()` | Not declared | Available |
| Page API (`read_page`/`write_page`/`copy_page`/`trim`/`get_page_*`) | Yes | Linked, but unreachable from app code [†] |
| Sector API aliases (`read_sector`/… — deprecated) | Yes | Linked, but unreachable from app code [†] |
| `sync`, `gc`, `erase_chip`, `get_block_size`, `get_block_num` | Yes | Linked, but unreachable from app code [†] |
| Raw-flash `esp_blockdev_t` via `nand_flash_get_blockdev` | No | Yes |
| WL `esp_blockdev_t` via `spi_nand_flash_wl_get_blockdev` | No | Yes |
| NAND IOCTLs (`IS_BAD_BLOCK`, `MARK_BAD_BLOCK`, `IS_FREE_PAGE`, `GET_PAGE_ECC_STATUS`, `COPY_PAGE`) | No | Yes (Flash BDL only) |
| NAND IOCTLs (`GET_BAD_BLOCKS_COUNT`, `GET_ECC_STATS`, `GET_NAND_FLASH_INFO`) | No | Yes (Flash BDL **and** WL BDL — see §4.2) |
| NAND IOCTL `MARK_DELETED` (TRIM) | No | Yes (WL BDL only) |
| `nand_diag_api.h` (bad-block count, ECC stats) | Yes | Linked, but unreachable from app code [†] — use `GET_BAD_BLOCKS_COUNT` / `GET_ECC_STATS` IOCTLs instead |
| FATFS integration | **Not in this component** — use sibling `spi_nand_flash_fatfs`, which requires BDL **off** | Not available (FatFs-on-BDL for SPI NAND is deliberately not shipped in 1.0.0) |
| Write verification | Opt-in via `CONFIG_NAND_FLASH_VERIFY_WRITE` | Opt-in via `CONFIG_NAND_FLASH_VERIFY_WRITE` |
| Host statistics gathering | Linux-only, opt-in via `CONFIG_NAND_ENABLE_STATS` | Linux-only, opt-in via `CONFIG_NAND_ENABLE_STATS` |

[†] In BDL mode, `spi_nand_flash_init_device()` deliberately returns
`ESP_ERR_NOT_SUPPORTED`, so an application has no way to obtain a
`spi_nand_flash_device_t*` through the public API. Symbols that take
`spi_nand_flash_device_t*` (the legacy page/sector API, `sync`/`gc`/
`erase_chip`, the diag API) are still **linked into the binary** but are
**not callable from BDL-only consumers** without reaching into the
non-public `bdl_handle->ctx`, which is outside the stability contract.

---

## 4. External interfaces

### 4.0 Stability contract & supported consumers (decided)

- **Primary consumers the baseline must protect, in parallel:**
  1. Application code calling the **legacy page / sector API** in
     `spi_nand_flash.h` directly.
  2. The sibling `spi_nand_flash_fatfs` component, which sits on the legacy
     `spi_nand_flash_device_t` path with `CONFIG_NAND_FLASH_ENABLE_BDL=n`.
  3. New consumers of the **BDL API** (`esp_nand_blockdev.h` +
     `spi_nand_flash_init_with_layers` / `nand_flash_get_blockdev` /
     `spi_nand_flash_wl_get_blockdev`) when
     `CONFIG_NAND_FLASH_ENABLE_BDL=y` on IDF ≥ 6.0.
- **Stability of exported symbols:** everything exported from `include/` is
  treated as **stable public API** for the purposes of this baseline. That
  includes:
  - `spi_nand_flash.h` (legacy + page + BDL-init)
  - `esp_nand_blockdev.h` (BDL-conditional)
  - `nand_device_types.h`
  - `nand_diag_api.h`
  - `nand_linux_mmap_emul.h` (Linux target)
  - `spi_nand_flash_test_helpers.h`
  - `nand_private/nand_impl_wrap.h`

  Headers under `priv_include/` remain internal and **are not** part of the
  stability contract. Any future OpenSpec change that removes, renames, or
  breaks the signature of a symbol in `include/` needs to be treated as a
  breaking change and flagged accordingly.
- **Supported IDF version window:** exactly what `idf_component.yml`
  declares today (`idf >= 5.0`, open-ended). No tighter internal LTS pin is
  being adopted as part of this baseline.



### 4.1 Public C API (legacy + page)

Declared in `include/spi_nand_flash.h`. Signatures (abridged):

```c
esp_err_t spi_nand_flash_init_device(spi_nand_flash_config_t *config,
                                     spi_nand_flash_device_t **handle);
esp_err_t spi_nand_flash_deinit_device(spi_nand_flash_device_t *handle);

esp_err_t spi_nand_flash_read_page (spi_nand_flash_device_t *h, uint8_t *buf,       uint32_t page_id);
esp_err_t spi_nand_flash_write_page(spi_nand_flash_device_t *h, const uint8_t *buf, uint32_t page_id);
esp_err_t spi_nand_flash_copy_page (spi_nand_flash_device_t *h, uint32_t src, uint32_t dst);
esp_err_t spi_nand_flash_trim      (spi_nand_flash_device_t *h, uint32_t page_id);

esp_err_t spi_nand_flash_get_page_count(spi_nand_flash_device_t *h, uint32_t *out);
esp_err_t spi_nand_flash_get_page_size (spi_nand_flash_device_t *h, uint32_t *out);
esp_err_t spi_nand_flash_get_block_size(spi_nand_flash_device_t *h, uint32_t *out);
esp_err_t spi_nand_flash_get_block_num (spi_nand_flash_device_t *h, uint32_t *out);

esp_err_t spi_nand_flash_sync (spi_nand_flash_device_t *h);
esp_err_t spi_nand_flash_gc   (spi_nand_flash_device_t *h);
esp_err_t spi_nand_erase_chip (spi_nand_flash_device_t *h);

/* Deprecated sector-named aliases kept for source-level back-compat. */
esp_err_t spi_nand_flash_read_sector  (...);
esp_err_t spi_nand_flash_write_sector (...);
esp_err_t spi_nand_flash_copy_sector  (...);
esp_err_t spi_nand_flash_get_capacity (...);
esp_err_t spi_nand_flash_get_sector_size(...);
```

Key config type:

```c
struct spi_nand_flash_config_t {
#ifndef CONFIG_IDF_TARGET_LINUX
    spi_device_handle_t device_handle;   /* caller-owned SPI device */
#else
    nand_file_mmap_emul_config_t *emul_conf;
#endif
    uint8_t gc_factor;                   /* trades space vs. GC frequency */
    spi_nand_flash_io_mode_t io_mode;    /* SIO / DOUT / DIO / QOUT / QIO */
    uint8_t flags;                       /* must match SPI device flags, e.g. SPI_DEVICE_HALFDUPLEX */
};
```

### 4.2 Public BDL API (conditional)

Declared in `include/spi_nand_flash.h` and `include/esp_nand_blockdev.h`:

```c
/* Simplified one-shot: creates Flash BDL + WL BDL and returns the WL handle. */
esp_err_t spi_nand_flash_init_with_layers(spi_nand_flash_config_t *config,
                                          esp_blockdev_handle_t   *wl_bdl);

/* Advanced, layer-by-layer: */
esp_err_t nand_flash_get_blockdev       (spi_nand_flash_config_t *config,
                                         esp_blockdev_handle_t   *flash_bdl);
esp_err_t spi_nand_flash_wl_get_blockdev(esp_blockdev_handle_t    flash_bdl,
                                         esp_blockdev_handle_t   *wl_bdl);
```

NAND-specific IOCTL commands. The matrix below shows which layer's `ioctl`
handler currently services each command (verified against
`src/nand_flash_blockdev.c` and `src/nand_wl_blockdev.c`). On the WL BDL,
`GET_NAND_FLASH_INFO`, `GET_BAD_BLOCKS_COUNT`, and `GET_ECC_STATS` are
forwarded to the underlying Flash BDL — they work on **either** handle.

| IOCTL | Flash BDL | WL BDL | Arg type |
|---|---|---|---|
| `ESP_BLOCKDEV_CMD_IS_BAD_BLOCK` | Yes | — | `esp_blockdev_cmd_arg_status_t*` |
| `ESP_BLOCKDEV_CMD_MARK_BAD_BLOCK` | Yes | — | `uint32_t*` |
| `ESP_BLOCKDEV_CMD_IS_FREE_PAGE` | Yes | — | `esp_blockdev_cmd_arg_status_t*` |
| `ESP_BLOCKDEV_CMD_GET_PAGE_ECC_STATUS` | Yes | — | `esp_blockdev_cmd_arg_ecc_status_t*` |
| `ESP_BLOCKDEV_CMD_COPY_PAGE` | Yes | — | `esp_blockdev_cmd_arg_copy_page_t*` |
| `ESP_BLOCKDEV_CMD_GET_BAD_BLOCKS_COUNT` | Yes | Yes (forwarded) | `uint32_t*` |
| `ESP_BLOCKDEV_CMD_GET_ECC_STATS` | Yes | Yes (forwarded) | `esp_blockdev_cmd_arg_ecc_stats_t*` |
| `ESP_BLOCKDEV_CMD_GET_NAND_FLASH_INFO` | Yes | Yes (forwarded) | `esp_blockdev_cmd_arg_nand_flash_info_t*` |
| `ESP_BLOCKDEV_CMD_MARK_DELETED` (TRIM) | — | Yes | `esp_blockdev_cmd_arg_erase_t*` |

### 4.3 Diagnostic API

```c
esp_err_t nand_get_bad_block_stats(spi_nand_flash_device_t *flash, uint32_t *bad_block_count);
esp_err_t nand_get_ecc_stats      (spi_nand_flash_device_t *flash);
```

### 4.4 Kconfig options

From `Kconfig`:

| Option | Default | Availability | Effect |
|---|---|---|---|
| `NAND_FLASH_VERIFY_WRITE` | `n` | All builds | After each page program, read the page back and compare; logs an error on mismatch. |
| `NAND_FLASH_ENABLE_BDL` | `n` | **Only when `IDF_INIT_VERSION >= 6.0`** | Compiles the BDL adapters and disables the legacy `spi_nand_flash_init_device()` path (which now returns `ESP_ERR_NOT_SUPPORTED`). |
| `NAND_ENABLE_STATS` | `n` | Linux target only | Enables host-side wear-leveling / statistics gathering in the emulator. |

### 4.5 Component & build dependencies

From `idf_component.yml` and `CMakeLists.txt`:

| Dependency | Kind | Condition |
|---|---|---|
| ESP-IDF | Public | `>= 5.0` |
| `espressif/dhara` | Public | `0.1.*` (overridable via `override_path: "../dhara"` for monorepo builds) |
| `esp_blockdev` | Public (REQUIRES) | IDF `>= 6.0` |
| `esp_driver_spi` | Public (REQUIRES) | IDF `> 5.3`, non-Linux target |
| `driver` (legacy SPI driver component) | Public (REQUIRES) | IDF `<= 5.3`, non-Linux target |
| `esp_mm` | Private (PRIV_REQUIRES) | Non-Linux target |

### 4.6 Expected pin/wiring interface

The component does **not** own the SPI bus or chip-select configuration. The
caller is expected to:

1. Initialize the chosen SPI host and attach the NAND chip with
   `spi_bus_initialize` / `spi_bus_add_device`.
2. Pass the resulting `spi_device_handle_t` (plus the matching `flags` value
   and chosen `io_mode`) into `spi_nand_flash_config_t`.

All SPI HAL concerns (pin muxing, clock speed, DMA channel, host selection)
are therefore outside this component’s responsibility.

### 4.7 Linux emulator configuration (host-test surface)

`include/nand_linux_mmap_emul.h` is part of the stable public API on the
`linux` target (see §4.0). The user-facing config type is:

```c
typedef struct {
    char   flash_file_name[256];
    size_t flash_file_size;
    bool   keep_dump;
} nand_file_mmap_emul_config_t;
```

Concretely:

- **`flash_file_name`:** if non-empty, used verbatim as the
  backing file path (`O_RDWR | O_CREAT`). If empty, the emulator falls back
  to a `mkstemp("/tmp/idf-nand-XXXXXX")` temporary file, and the resolved
  path is stored back into the handle’s copy of the field.
- **`flash_file_size`:** must be a non-zero multiple of the
  chip’s user-visible block size (`page_size * pages_per_block`). If zero,
  defaults to `EMULATED_NAND_SIZE = 128 MiB`. The on-disk file is allocated
  as `pages_per_block * (page_size + oob_size)` per block, so the actual
  byte size of the file is larger than `flash_file_size` and the user-visible
  capacity is `num_blocks * block_size` (slightly less than
  `flash_file_size` due to OOB overhead). Effective number of usable blocks
  is `flash_file_size / (pages_per_block * (page_size + oob_size))`.
- **`keep_dump`:** if `true`, the backing file is **not** removed
  on `nand_emul_deinit`. (Inferred) However, every `nand_emul_init` call
  re-`memset`s the mapped region to `0xFF`, so the file’s previous contents
  do not survive across init cycles even with `keep_dump = true`. Persisting
  data across runs would require additional plumbing not present today.
- **Synthetic chip identity (Confirmed):** on Linux, `nand_init_device`
  populates `device_info` with `manufacturer_id = 0xEF`,
  `device_id = 0xE100`, and `chip_name = "Linux NAND mmap emul"` so BDL
  consumers calling `ESP_BLOCKDEV_CMD_GET_NAND_FLASH_INFO` see non-zero
  IDs and a non-empty name in host tests. These values are not real JEDEC
  IDs and must not be interpreted as such.
- **Synthetic geometry / timings:** `log2_page_size = 11`
  (2048 B), `log2_ppb = 6` (64 pages/block), `num_planes = 1`, OOB size
  16 / 64 / 128 B for 512 / 2048 / 4096 B pages, `erase_block_delay_us =
  3000`, `program_page_delay_us = 630`, `read_page_delay_us = 60`,
  `ecc_status_reg_len_in_bits = 2`, `ecc_data_refresh_threshold = 4`. These
  are emulator constants, not values read from any vendor table.

Functions exported by this header (`nand_emul_init`, `nand_emul_deinit`,
`nand_emul_read`, `nand_emul_write`, `nand_emul_erase_block`, plus
`nand_emul_get_stats` / `nand_emul_clear_stats` behind
`CONFIG_NAND_ENABLE_STATS`) are intended for the component’s own
`nand_impl_linux.c` and the host test app.
---

## 5. Architecture & data flow

### 5.1 Layered view (legacy mode, default)

```
                                   ┌───────────────────────────────┐
                                   │ Application                   │
                                   └───────────────┬───────────────┘
                                                   │  page/sector API
                                                   ▼
                                   ┌───────────────────────────────┐
                                   │ Public API   (spi_nand_flash.h│
                                   │              / src/nand.c)    │
                                   └───────────────┬───────────────┘
                                                   │
                                                   ▼
                                   ┌───────────────────────────────┐
                                   │ Wear-Leveling layer           │
                                   │  Dhara FTL glue               │
                                   │  (src/dhara_glue.c)           │
                                   └───────────────┬───────────────┘
                                                   │  physical page/block ops
                                                   ▼
                                   ┌───────────────────────────────┐
                                   │ Flash layer                   │
                                   │  (src/nand_impl.c,            │
                                   │   src/nand_impl_wrap.c)       │
                                   │  + per-vendor init            │
                                   │    (src/devices/*.c)          │
                                   └───────────────┬───────────────┘
                                                   │  SPI commands / mmap I/O
                                                   ▼
                         ┌───────────────────────┐   ┌───────────────────────┐
                         │ SPI ops               │   │ Linux emulator        │
                         │ (src/spi_nand_oper.c) │   │ (nand_linux_mmap_…)   │
                         └───────────┬───────────┘   └───────────┬───────────┘
                                     │                           │
                                     ▼                           ▼
                               ┌──────────┐               ┌────────────┐
                               │ HW SPI   │               │ mmap file  │
                               │ + NAND   │               │ on disk    │
                               └──────────┘               └────────────┘
```

### 5.2 Layered view (BDL mode)

```
     Application / filesystem                Application calling ioctls
             │  esp_blockdev_t                        │
             ▼                                        ▼
    ┌──────────────────────────┐       ┌──────────────────────────┐
    │ WL BDL                   │       │ Flash BDL                │
    │ (nand_wl_blockdev.c)     │──────▶│ (nand_flash_blockdev.c)  │
    │ read / write / trim      │       │ read / write / erase /   │
    │ → ioctl MARK_DELETED     │       │  ioctl: bad-block, ECC,  │
    └────────────┬─────────────┘       │  copy-page, flash-info   │
                 │                      └────────────┬─────────────┘
                 ▼                                    ▼
          Dhara FTL  (dhara_glue.c)          nand_impl (+ vendor)
                                                   │
                                                   ▼
                                            spi_nand_oper / emul
```

### 5.3 Init sequence (legacy)

1. Caller prepares `spi_device_handle_t` + `spi_nand_flash_config_t`.
2. `spi_nand_flash_init_device()` → `nand_init_device()` in `nand_impl.c`.
3. `nand_impl` reads JEDEC ID, dispatches to `devices/nand_<vendor>.c`
   to populate `nand_flash_geometry_t` (page size, block size, ECC threshold,
   QE-bit position, plane count, timings, flags).
4. Dhara is attached via `nand_wl_attach_ops()` in `dhara_glue.c`, which
   constructs the wear-leveling state.
5. Handle is returned; subsequent API calls route page_id → Dhara → physical
   page.

### 5.4 Init sequence (BDL)

1. `spi_nand_flash_init_with_layers(config, &wl_bdl)` internally:
   a. Calls `nand_flash_get_blockdev(config, &flash_bdl)` — runs the same
      chip-detect path as legacy init and wraps it in `esp_blockdev_t` ops.
   b. Calls `spi_nand_flash_wl_get_blockdev(flash_bdl, &wl_bdl)` — attaches
      Dhara on top and exposes another `esp_blockdev_t`.
2. Releasing `wl_bdl` tears down both layers.

---

## 6. Build, configuration & dependency constraints

- **IDF version gating** is enforced both in Kconfig (`NAND_FLASH_ENABLE_BDL`
  depends on `IDF_INIT_VERSION >= "6.0"`) and in `CMakeLists.txt`
  (`IDF_VERSION_MAJOR.IDF_VERSION_MINOR >= 6.0` guards inclusion of BDL sources
  and the `esp_blockdev` requirement).
- **SPI driver selection** switches at build time:
  `esp_driver_spi` for IDF > 5.3, else the legacy `driver` component.
- **Target gating**: on `linux` target, the SPI / vendor / diag sources are
  excluded and replaced by `nand_impl_linux.c` + `nand_linux_mmap_emul.c`.
  `esp_mm` is a private requirement only on non-Linux targets.
- **Linux emulator file-size constraint :** on the `linux` target,
  `nand_init_device` rejects (`ESP_ERR_INVALID_SIZE`) any
  `flash_file_size` that is not a multiple of the user-visible
  `chip.block_size` (`page_size * pages_per_block`). The actual on-disk file
  is sized using `file_bytes_per_block = pages_per_block *
  (page_size + oob_size)`, so the user-visible capacity reported via the
  page API is `num_blocks * block_size`, slightly less than
  `flash_file_size` (the trailing OOB overhead is not user-addressable).
  Default size when `flash_file_size == 0` is `EMULATED_NAND_SIZE`
  (128 MiB).
- **Mutual exclusion with FatFs-on-NAND**: the README and CHANGELOG call out
  that `spi_nand_flash_fatfs` only works when `CONFIG_NAND_FLASH_ENABLE_BDL`
  is **off**. There is no FatFs-on-BDL path in 1.0.0 — this is an intentional
  constraint, not an unfinished feature of this component.
- **DMA / alignment**: prior releases (0.19.0, 0.21.0) fixed failures when
  buffers handed to `spi_nand_program_load` / `spi_nand_read` were not
  DMA-aligned. Today buffers and work areas are handled internally; callers
  still pass arbitrary user buffers for page read/write through the public
  API.
- **Write verification** (`CONFIG_NAND_FLASH_VERIFY_WRITE`) is a debug aid,
  not a guarantee; it doubles every program with a read-back compare.
- **`gc_factor`** (from `spi_nand_flash_config_t`) trades usable capacity for
  GC headroom; the README documents “lower values reduce available space but
  increase performance” — the exact mapping to block reservations lives inside
  Dhara and is not (currently) re-documented here. If the caller passes **0**,
  both `spi_nand_flash_init_device()` and `spi_nand_flash_init_with_layers()`
  substitute **45** before Dhara is initialized (`src/nand.c`).
- **Backward-compat guarantee**: the sector-named API is a set of deprecated
  aliases over the page API; both paths take the same code path internally.

---

## 7. Supported hardware & targets

### 7.1 MCU targets

| Target family | Status | Notes |
|---|---|---|
| All ESP chipsets supported by the configured IDF version | Supported | Component makes no target-specific assumptions beyond the two SPI driver options above. |
| `linux` | Supported (emulation only) | Uses mmap file; no real NAND I/O. The emulator advertises a synthetic identity (mfr `0xEF`, dev `0xE100`, name `"Linux NAND mmap emul"`) — these are placeholder values for host tests, **not** real JEDEC IDs. |

### 7.2 SPI NAND chips (compiled-in vendor modules)

| Vendor | File | Representative parts listed in README |
|---|---|---|
| Winbond | `src/devices/nand_winbond.c` | W25N01GVxxxG/T/R, W25N512GVxIG/IT, W25N512GWxxR/T, W25N01JWxxxG/T, W25N02KVxxIR/U, W25N04KVxxIR/U |
| GigaDevice | `src/devices/nand_gigadevice.c` | GD5F1GQ5UExxG, GD5F1GQ5RExxG, GD5F2GQ5UExxG, GD5F2GQ5RExxG, GD5F2GM7xExxG, GD5F4GQ6UExxG, GD5F4GQ6RExxG, GD5F4GM8xExxG, GD5F1GM7xExxG |
| Alliance | `src/devices/nand_alliance.c` | AS5F31G04SND-08LIN, AS5F32G04SND-08LIN, AS5F12G04SND-10LIN, AS5F34G04SND-08LIN, AS5F14G04SND-10LIN, AS5F38G04SND-08LIN, AS5F18G04SND-10LIN |
| Micron | `src/devices/nand_micron.c` | MT29F4G01ABAFDWB, MT29F1G01ABAFDSF-AAT:F, MT29F2G01ABAGDWB-IT:G |
| Zetta | `src/devices/nand_zetta.c` | ZD35Q1GC |
| XTX | `src/devices/nand_xtx.c` | XT26G08D |

Chip detection is automatic via JEDEC ID; unknown IDs cause init to fail
(`ESP_ERR_NOT_FOUND` path through the BDL factory).

### 7.3 SPI I/O modes

`spi_nand_flash_io_mode_t` covers SIO, DOUT, DIO, QOUT, QIO. DIO / DOUT
require `SPI_DEVICE_HALFDUPLEX`. Whether a given chip actually supports QIO
(and the quad-enable-bit position) is reported by the vendor init into
`nand_flash_geometry_t::has_quad_enable_bit` / `quad_enable_bit_pos`.

---

## 8. Testing & validation surface

### 8.1 On-target test application (`test_app/`)

- `main/test_app_main.c` boots the test runner.
- `main/test_spi_nand_flash.c` holds the legacy-mode test cases.
- `main/test_spi_nand_flash_bdl.c` holds the BDL-mode test cases.
- Two CI sdkconfig presets are shipped: `sdkconfig.ci.default` (legacy) and
  `sdkconfig.ci.bdl` (BDL).
- `pytest_spi_nand_flash.py` drives the flashed target from a host runner.

### 8.2 Linux host test (`host_test/`)

- `main/test_app_main.cpp` + `main/test_nand_flash.cpp` +
  `main/test_nand_flash_bdl.cpp` + `main/test_nand_flash_ftl.cpp` exercise
  the stack against the mmap emulator. `test_nand_flash_ftl.cpp` carries the
  Dhara-FTL-focused cases (capacity, GC, erase-chip-then-reuse) that are
  shared between legacy and BDL modes via shared fixtures.
- `pytest_nand_flash_linux.py` orchestrates host runs.
- Build via `idf.py --preview set-target linux && idf.py build monitor`.

### 8.3 Safety checks baked into the BDL adapters

- Zero-divisor guards on page-size / block-size / plane modulo math.
- Alignment checks for address/length on read / write / erase paths.
- Function-pointer validation on every BDL ops table used as input
  (`read`, `write`, `erase`, `ioctl`, `release` must be non-NULL where
  required).

---

## 9. Known constraints, invariants & edge cases

### 9.1 Interface invariants

- A `spi_nand_flash_device_t*` is non-relocatable; callers must own the
  handle’s lifetime.
- **Concurrency contract (load-bearing):**
  - **There is exactly one mutex per device handle:** `handle->mutex`,
    created with `xSemaphoreCreateMutex()` in `nand_init_device`. All
    serialization is built on top of this single mutex.
  - **Dhara-managed paths are internally thread-safe per handle.** Any thread
    may call the public page/sector API (`spi_nand_flash_read_page`,
    `write_page`, `copy_page`, `trim`, `sync`, `gc`, `erase_chip`, capacity
    queries) or the **WL BDL** (`spi_nand_flash_wl_get_blockdev` ops and
    IOCTLs) on the same handle concurrently. Serialization is provided by
    `handle->mutex`, taken at the public-API entry points in `src/nand.c`
    (the WL BDL ops in `src/nand_wl_blockdev.c` re-enter through these same
    public-API functions, so they inherit the same lock). Inside the
    locked region, Dhara's callbacks call the unwrapped low-level
    `nand_*` functions directly (no double-take).
  - **`nand_private/nand_impl_wrap.h` is a separate, parallel entry point**
    into the same `handle->mutex`. The wrap functions (`nand_wrap_read`,
    `nand_wrap_prog`, `nand_wrap_is_bad`, …) take `handle->mutex` around
    a single low-level op so that direct `nand_impl_wrap.h` callers and
    `nand_diag_api.c` are safe against concurrent use of the public API
    on the same handle. The wrap layer is **not** what protects the
    legacy public API — that one locks itself.
  - **Raw-flash paths are NOT internally synchronized across callers.** The
    Flash BDL ops (`src/nand_flash_blockdev.c`) call the unwrapped
    `nand_*` primitives directly and **do not** take `handle->mutex`. If an
    application uses the **raw Flash BDL** (`nand_flash_get_blockdev`)
    standalone, the caller is responsible for concurrency.
  - Mixing a Dhara-managed handle (legacy API or WL BDL) with concurrent
    raw-flash access (Flash BDL or `nand_impl_wrap.h`) on the **same
    underlying device** is unsupported.
- `spi_device_handle_t` passed into config must be configured with a `flags`
  value matching what the caller passes in `spi_nand_flash_config_t::flags`
  (half/full duplex). Mismatch is a programmer error.
- On BDL builds, legacy `spi_nand_flash_init_device()` **deliberately** returns
  `ESP_ERR_NOT_SUPPORTED`. This is a compile-and-runtime contract to keep the
  FatFs-on-NAND sibling from silently mis-initializing on top of BDL.

### 9.2 Behavioural invariants

- `spi_nand_erase_chip()` now skips factory-marked bad blocks (regression
  fixed in 1.0.0). Before 1.0.0 it erased every block unconditionally.
- All public page-API indices are **logical** (after Dhara remapping). Raw
  flash indices are only reachable through the Flash BDL.
- TRIM / `spi_nand_flash_trim` is advisory — Dhara uses it to release a page
  for GC; the actual physical erase happens later on a GC cycle or block
  reclaim.
- `spi_nand_flash_sync()` is the point at which buffered Dhara state is
  pushed to the medium; callers relying on durability after a write sequence
  are expected to call it.
- `spi_nand_erase_chip()` is a **destructive media wipe** (physical erase,
  Dhara map cleared afterward). It serializes with other API calls on the
  same handle via the device mutex, but it must not be used while a
  filesystem or other consumer assumes the volume is mounted with intact
  metadata. See §11.3 for details.

### 9.3 Operational edge cases called out in code / docs

- `ESP_BLOCKDEV_CMD_GET_ECC_STATS` scans the whole part and may run long
  enough to trip the task watchdog on large devices; the header explicitly
  warns against calling it from an ISR and recommends adjusting WDT settings.
- Raw-flash BDL `erase` ignores the bad-block map. Callers must pre-check
  with `ESP_BLOCKDEV_CMD_IS_BAD_BLOCK` or use the WL BDL for managed access.
- `nand_get_ecc_stats()` (diagnostic) iterates pages and is similarly meant
  for offline/maintenance use, not the hot I/O path.
- Write verification doubles every program and is expected to noticeably
  reduce write throughput — it is off by default for this reason.

### 9.4 Power-loss / unclean-shutdown guarantees

The supported failure model today is **exactly what the Dhara FTL provides —
nothing more, nothing less**. In practice that means:

- Dhara’s own journal/metadata recovery on next mount is what protects the
  logical sector view after an unclean shutdown.
- This component adds **no additional** durability guarantees on top of
  Dhara. In particular, `spi_nand_flash_sync()` does what Dhara exposes; no
  higher-level transactional boundary is defined.
- Raw-flash paths (Flash BDL, `nand_private/nand_impl_wrap.h`) have **no**
  power-loss protection beyond what the physical NAND part guarantees for an
  in-flight page program or block erase.
- Stronger guarantees (e.g. atomic multi-page writes, integrity scrub,
  journaling on top of Dhara) are explicitly **out of scope** — see §10.

---

## 10. Explicitly out of scope for this baseline

These items are intentionally excluded from this document because it is a
current-state baseline, not a roadmap. Listed here only so future OpenSpec
changes know where the seams are:

- **FatFs-on-BDL for SPI NAND.** Not implemented in 1.0.0; deliberately absent.
- **New vendor / new part support.** Additions follow the
  `src/devices/nand_<vendor>.c` pattern; scoping such work is a future
  proposal.
- **Alternative FTLs.** Dhara is currently the only wear-leveling option.
- **Power-loss / unclean-shutdown hardening beyond Dhara’s built-in
  guarantees.** Whatever Dhara provides is what is available today; anything
  beyond that (journaling, atomic multi-page writes, integrity scrub) is a
  future concern.
- **Encryption-at-rest / authenticated storage.** No such layer exists today.
- **Runtime hot-plug / multi-device coordination.** Each `init_*` call
  produces an independent handle; the component has no global registry.

---

## 11. Unknowns and assumptions

Remaining gaps are listed under §11.2; everything else in this section has
been checked against the tree.

### 11.1 Policy assumptions (still worth stating explicitly)

1. **Vendor modules:** All six `src/devices/nand_*.c` modules remain
   first-class unless the maintainers explicitly deprecate one in CHANGELOG /
   README.
2. **No third FTL / API mode:** Wear leveling stays on Dhara; the only
   product-visible configuration split for the API is legacy vs BDL (plus the
   `linux` **target**, which swaps the backend for mmap emulation — not a
   third application-facing mode).

### 11.2 Remaining unknowns (need owner / lab input)

These cannot be closed from the repo alone:

- Exact **performance envelope** (MB/s, IOPS, GC pause distribution) on a
  chosen reference part — no benchmark suite or numbers are checked in.
- Which vendor/part combinations are exercised by **automated CI hardware**
  vs manual benches (CI presets only pin legacy vs BDL **mode**, not the chip
  on the bench).

### 11.3 Resolved decisions (locked)

Recorded here so the resolutions are not lost:

- **Consumer set:** legacy page/sector API, `spi_nand_flash_fatfs` (legacy
  path, BDL off), and the BDL API are **all** load-bearing in parallel. See
  §4.0.
- **Stability contract:** everything exported under `include/` — including
  `spi_nand_flash_test_helpers.h` and `nand_private/nand_impl_wrap.h` — is
  stable public API. `priv_include/` remains internal. See §4.0.
- **IDF version window:** as declared in `idf_component.yml` today
  (`idf >= 5.0`, open-ended). See §4.0.
- **Concurrency contract:** Dhara-managed paths are internally thread-safe
  per handle; raw-flash paths require caller-side serialization. See §9.1.
- **Power-loss guarantee:** exactly what Dhara provides, nothing more. See
  §9.4.
- **Legacy vs BDL as the API axis:** Kconfig + CMake expose only
  BDL on/off (when IDF ≥ 6.0); there is no additional parallel init mode.
  `linux` is a **target** that uses the mmap stack instead of SPI — see §7.1
  and host tests.
- **Vendor drivers:** Non-Linux builds compile all six vendor
  files unconditionally (`CMakeLists.txt`); chip selection is JEDEC-based at
  runtime.
- **Dhara dependency:** `idf_component.yml` pins
  `espressif/dhara` at `0.1.*` with `override_path: "../dhara"` for this
  monorepo layout.
- **`gc_factor` default:** If the caller passes `gc_factor == 0`,
  both `spi_nand_flash_init_device()` and `spi_nand_flash_init_with_layers()`
  set it to **45** before attaching Dhara (`src/nand.c`). Host tests document
  “0 selects driver default.” There is still **no** Kconfig override for this
  value.
- **`spi_nand_flash_config_t::flags`:** Documented in
  `spi_nand_flash.h` as `SPI_DEVICE_HALFDUPLEX` for half duplex or **0** for
  full duplex; `spi_nand_execute_transaction()` branches on
  `SPI_DEVICE_HALFDUPLEX` and adjusts transaction lengths for full-duplex mode.
  DIO/DOUT still require half duplex per the same header note.
- **`spi_nand_erase_chip()` usage:** The call
  takes the per-handle mutex for the whole operation (other threads block on
  the public API until it finishes). It performs a **physical** full-media
  erase (skipping factory bad blocks), then invokes the Dhara layer’s
  `deinit` path (`dhara_map_clear` after re-init sizing — see
  `dhara_deinit()` in `src/dhara_glue.c`). Host FTL tests assert that
  read/write still succeeds **after** a successful erase on a Dhara-managed
  handle. **It is still not appropriate while a filesystem is mounted on
  that logical device:** it destroys on-flash metadata and invalidates any
  higher-level format; use only for intentional media wipe (typically with
  nothing else treating the volume as mounted). Not applicable to BDL-only
  workflows that never hold a `spi_nand_flash_device_t*` — those should use
  chip erase only via an explicitly designed path if one is added later.

### 11.4 Known bugs / follow-ups (queued for next release)

The two items that were previously documented here (**§11.4.1**
`nand_emul_get_stats`, **§11.4.2** internal-RAM placement for the device
handle and Dhara private data) are **fixed in-tree** as part of the
configurable OOB layout work (OpenSpec steps **10** and **05**
respectively). Details and history: [`known-bugs.md`](known-bugs.md) §11.4.

No additional defects are queued in this subsection at the time of that sync.

---

## 12. References

- `README.md` — component overview, supported parts, verification Kconfig.
- `layered_architecture.md` — the detailed architecture + migration guide.
- `CHANGELOG.md` — historical feature additions and 1.0.0 breaking changes.
- `Kconfig` — component options (verify write, BDL, host stats, experimental OOB layout).
- `CMakeLists.txt` — conditional source / dependency selection by target and
  IDF version.
- `idf_component.yml` — declared dependencies and versions.
- `include/spi_nand_flash.h` — the canonical public API.
- `include/esp_nand_blockdev.h` — the BDL-only API and IOCTL commands.
- `include/nand_device_types.h` — shared geometry / ECC / identification
  types.
- `src/devices/*.c` — per-vendor init code.
- `test_app/` and `host_test/` — target and host test entry points.
