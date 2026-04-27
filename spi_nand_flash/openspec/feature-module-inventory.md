# Feature / module inventory — `spi_nand_flash`

> **Artifact type:** OpenSpec discovery — structured inventory of existing features/modules.  
> **Source of truth:** [`baseline.md`](baseline.md) (current-state baseline).  
> **Scope:** Document the system **as-is**. No redesign. No implementation proposals.  
> **Confirmed defects (fix queue):** Documented only in [`known-bugs.md`](known-bugs.md) (baseline §11.4 mirror). They are **not** repeated in the “Known issues” column below.

---

## How to read this document

- **Confidence**
  - **Confirmed** — Stated explicitly in the baseline as verified or decided.
  - **Inferred** — Baseline marks inference (e.g. parenthetical “Inferred”) or logically follows from stated behavior without a cited verification step.
  - **Unknown** — Not stated in the baseline; would need code review, hardware, or owner input.

- **Known issues** — Operational limitations and risks from the baseline **outside** the §11.4 defect queue (e.g. §9). Deliberate non-features appear under **Constraints**, not here.

- **Open questions** — Pulled from baseline §11.2 where applicable, or marked Unknown when the baseline is silent.

---

## 1. Public page / sector API (`spi_nand_flash.h`, `src/nand.c`)

| Field | Content |
| --- | --- |
| **Name** | Public page / sector API (legacy init path) |
| **Purpose** | Initialize a Dhara-backed logical NAND device and expose read/write/copy/trim/capacity/sync/GC/chip-erase and deprecated sector aliases to applications and to sibling FatFs integration (`spi_nand_flash_fatfs`). |
| **Current behavior summary** | `spi_nand_flash_init_device` runs chip detect → vendor setup → Dhara attach → returns `spi_nand_flash_device_t*`. Page indices are **logical** (after Dhara). `spi_nand_flash_read_page` may re-program a page when corrected ECC bits exceed `ecc_data_refresh_threshold`. With `CONFIG_NAND_FLASH_ENABLE_BDL=y` (IDF ≥ 6.0), `spi_nand_flash_init_device` **returns `ESP_ERR_NOT_SUPPORTED`**; legacy API symbols remain linked but are not reachable via public init (baseline §3.4). `gc_factor == 0` is substituted with **45** before Dhara init. |
| **Inputs** | `spi_nand_flash_config_t`: non-Linux — caller-owned `spi_device_handle_t`, `gc_factor`, `io_mode`, `flags`; Linux — `nand_file_mmap_emul_config_t*`. Page/sector APIs take handle, buffers, page IDs. |
| **Outputs** | `esp_err_t`; filled buffers on read; logical capacity via getters; side effects on medium through Dhara and physical layer. |
| **Dependencies** | SPI bus/device owned by caller (§4.6). Downstream: Dhara glue, `nand_impl` / wrap, SPI ops or Linux mmap stack. Optional: `CONFIG_NAND_FLASH_VERIFY_WRITE`. |
| **Constraints** | BDL-on prevents legacy init. Half/full duplex `flags` must match SPI device configuration. Thread-safety: single mutex per handle for these entry points (§9.1). `spi_nand_flash_sync` is the durability boundary Dhara exposes (§9.4). TRIM is advisory (§9.2). Chip erase incompatible with mounted filesystem assumptions (§11.3). |
| **Related files / components** | `include/spi_nand_flash.h`, `src/nand.c`; types in `include/nand_device_types.h`. |
| **Confidence** | **Confirmed** (baseline §2, §4.1, §5.3, §9.1–§9.2). |
| **Known issues** | Long-running diagnostic-style paths documented elsewhere (not this API’s primary defect list). |
| **Open questions** | Exact mapping of `gc_factor` to Dhara reservations: baseline states it lives inside Dhara and is not re-documented (**Unknown** detail). |

---

## 2. BDL initialization and block device handles (`spi_nand_flash.h`, `esp_nand_blockdev.h`, `src/nand.c`, `src/nand_flash_blockdev.c`, `src/nand_wl_blockdev.c`)

| Field | Content |
| --- | --- |
| **Name** | Block Device Layer (BDL) — Flash and wear-leveling adapters |
| **Purpose** | On ESP-IDF ≥ 6.0 with `CONFIG_NAND_FLASH_ENABLE_BDL=y`, expose raw flash and Dhara-managed flash as `esp_blockdev_t` handles; provide NAND-specific IOCTLs per baseline matrix (§4.2). |
| **Current behavior summary** | `spi_nand_flash_init_with_layers` builds Flash BDL then WL BDL and returns WL handle. `nand_flash_get_blockdev` / `spi_nand_flash_wl_get_blockdev` allow layered construction. IOCTL routing: Flash BDL handles bad-block, mark bad, free page, page ECC, copy page; both Flash and WL forward some info/stats IOCTLs; WL handles `MARK_DELETED` (TRIM). BDL sources compile only when Kconfig and CMake IDF version guards pass. |
| **Inputs** | Same `spi_nand_flash_config_t` as legacy (minus successful legacy init when BDL on). Block addresses/lengths for read/write/erase; IOCTL command + typed args. |
| **Outputs** | `esp_blockdev_handle_t`; read/write data; IOCTL output structs; release tears down layers per baseline §5.4. |
| **Dependencies** | `esp_blockdev` (IDF ≥ 6.0). Underlying `nand_impl` + SPI or emulator. Dhara for WL BDL. |
| **Constraints** | Raw Flash BDL does **not** take `handle->mutex`; caller must serialize concurrent raw access (§9.1). Mixing Dhara-managed use with concurrent raw access on same medium is unsupported (§9.1). Raw-flash erase ignores bad-block map (§9.3). FatFs-on-BDL not shipped in 1.0.0; sibling FatFs requires BDL off (§6, §10). |
| **Related files / components** | `include/esp_nand_blockdev.h`, `include/spi_nand_flash.h`, `src/nand_flash_blockdev.c`, `src/nand_wl_blockdev.c`, `src/nand.c`; safety checks per §8.3. |
| **Confidence** | **Confirmed** (baseline §3.4, §4.2, §5.2, §5.4, §9.1). |
| **Known issues** | `GET_ECC_STATS` full-device scan may trip task WDT on large parts (§9.3). |
| **Open questions** | **Unknown** — Which IOCTL combinations are exercised in CI vs manual (baseline §11.2 mentions chip-on-bench gaps only at high level). |

---

## 3. Dhara FTL glue (`src/dhara_glue.c`, `priv_include/nand.h`)

| Field | Content |
| --- | --- |
| **Name** | Dhara wear-leveling / FTL glue |
| **Purpose** | Bridge external Dhara library to physical NAND operations; own wear-leveling state for logical sector/page abstraction, TRIM, GC. |
| **Current behavior summary** | Attaches Dhara to `nand_impl` callbacks; implements logical-to-physical translation used by legacy API and WL BDL. Power-loss behavior is **exactly Dhara’s** (§9.4). Chip erase path clears Dhara map after physical erase (baseline §11.3 references `dhara_map_clear` / deinit sequencing). |
| **Inputs** | Device geometry and ops from init; page IDs and buffers from upper layers. |
| **Outputs** | Physical page/block ops invoked through glue; metadata on flash per Dhara. |
| **Dependencies** | Component dependency `espressif/dhara` `0.1.*` (override path for monorepo) (§4.5). Unwrapped `nand_*` from `nand_impl` inside locked regions (§9.1). |
| **Constraints** | No additional durability guarantees beyond Dhara (§9.4). `gc_factor` defaulting when zero (§6). |
| **Related files / components** | `src/dhara_glue.c`, `priv_include/nand.h`; `idf_component.yml`. |
| **Confidence** | **Confirmed** for role and guarantees; **Unknown** for internal Dhara algorithms/capacity math details not duplicated in baseline. |
| **Known issues** | — |
| **Open questions** | Same as §11.2 performance envelope (**Unknown**). |

---

## 4. NAND implementation — mutex wrap (`src/nand_impl_wrap.c`, `include/nand_private/nand_impl_wrap.h`)

| Field | Content |
| --- | --- |
| **Name** | `nand_impl` wrapper (parallel entry point) |
| **Purpose** | Serialize individual low-level NAND operations with `handle->mutex` for callers using the stable wrap API (e.g. diagnostics). |
| **Current behavior summary** | Wrap functions take mutex around one low-level op; coexistence contract with legacy API and `nand_diag_api` documented in §9.1. |
| **Inputs** | `spi_nand_flash_device_t*` and operation-specific args via wrap functions (signatures in exported header). |
| **Outputs** | Physical read/program/erase/status results per op. |
| **Dependencies** | `nand_impl` primitives. |
| **Constraints** | Part of **stable public API** under `include/` (§4.0). Caller responsible for not mixing unsafely with raw Flash BDL on same device (§9.1). |
| **Related files / components** | `src/nand_impl_wrap.c`, `include/nand_private/nand_impl_wrap.h`. |
| **Confidence** | **Confirmed** (§4.0, §9.1). |
| **Known issues** | — |
| **Open questions** | — |

---

## 5. NAND implementation — physical layer (`src/nand_impl.c`, `priv_include/nand_impl.h`) — target only

| Field | Content |
| --- | --- |
| **Name** | Physical NAND page/block/ECC/bad-block implementation |
| **Purpose** | JEDEC ID read; dispatch to vendor init; physical page read/program, block erase, bad-block check/mark, free-page check, ECC status; plane selection; chip erase skipping factory bad blocks (§9.2). |
| **Current behavior summary** | Real hardware path for non-Linux targets. Init allocates DMA-capable work buffers per baseline §6. |
| **Inputs** | SPI device from caller config; addresses/page indices at physical layer when invoked from glue or BDL. |
| **Outputs** | Data buffers; ECC status; medium mutations. |
| **Dependencies** | `spi_nand_oper.c`; vendor `src/devices/*.c`; `esp_mm` PRIV_REQUIRES non-Linux (§4.5). |
| **Constraints** | Unknown JEDEC ID → init failure (`ESP_ERR_NOT_FOUND` via BDL factory note §7.2). DMA/alignment history noted in §6. |
| **Related files / components** | `src/nand_impl.c`, `priv_include/nand_impl.h`, `priv_include/nand_flash_devices.h`. |
| **Confidence** | **Confirmed** for responsibilities and file mapping; **Unknown** for full register-level behavior per chip. |
| **Known issues** | — |
| **Open questions** | Automated CI coverage per vendor/part (**Unknown**, §11.2). |

---

## 6. SPI transaction layer (`src/spi_nand_oper.c`, `priv_include/spi_nand_oper.h`) — target only

| Field | Content |
| --- | --- |
| **Name** | SPI NAND command / transaction layer |
| **Purpose** | Issue NAND command sequences over ESP SPI controller in SIO/DOUT/DIO/QOUT/QIO modes. |
| **Current behavior summary** | Drives read, program load/execute, erase, get/set feature, read ID, reset sequences per baseline §2. Not built on Linux target. |
| **Inputs** | SPI device handle and transaction parameters from `nand_impl`. |
| **Outputs** | Data/OOB/STATUS on wire; errors surfaced as operation failures. |
| **Dependencies** | `esp_driver_spi` (IDF > 5.3) or legacy `driver` SPI (IDF ≤ 5.3), non-Linux (§4.5). |
| **Constraints** | Half vs full duplex affects transaction shaping (`spi_nand_flash_config_t::flags`, §6, §11.3). |
| **Related files / components** | `src/spi_nand_oper.c`, `priv_include/spi_nand_oper.h`. |
| **Confidence** | **Confirmed** (baseline §2–§3). |
| **Known issues** | — |
| **Open questions** | — |

---

## 7. Vendor-specific chip support (`src/devices/nand_*.c`, `priv_include/nand_flash_devices.h`)

| Field | Content |
| --- | --- |
| **Name** | Vendor modules (Winbond, GigaDevice, Alliance, Micron, Zetta, XTX) |
| **Purpose** | Per-vendor feature registers, geometry, QE bit, planes, timings, quirks after JEDEC match. |
| **Current behavior summary** | All six sources compiled unconditionally on non-Linux builds; runtime selection by JEDEC ID (§11.3). Representative part lists in README (§7.2). |
| **Inputs** | JEDEC manufacturer/device ID from hardware. |
| **Outputs** | Populated `nand_flash_geometry_t` / device capabilities used by impl + Dhara sizing. |
| **Dependencies** | Called from `nand_impl` init path. |
| **Constraints** | Parallel/ONFI NAND unsupported (§2). QIO support varies by part; geometry fields convey quad-enable (§7.3). |
| **Related files / components** | `src/devices/nand_winbond.c`, `nand_gigadevice.c`, `nand_alliance.c`, `nand_micron.c`, `nand_zetta.c`, `nand_xtx.c`, `priv_include/nand_flash_devices.h`. |
| **Confidence** | **Confirmed** for inventory and selection model. |
| **Known issues** | — |
| **Open questions** | Which parts are validated on hardware vs datasheet-only (**Unknown**, §11.2). |

---

## 8. Diagnostic API (`include/nand_diag_api.h`, `src/nand_diag_api.c`) — target only

| Field | Content |
| --- | --- |
| **Name** | NAND diagnostics (bad-block count, ECC stats) |
| **Purpose** | Query bad-block statistics and ECC statistics for a `spi_nand_flash_device_t*`. |
| **Current behavior summary** | Implemented on target builds. In BDL-only apps, baseline states diag API is linked but not reachable without legacy handle; BDL consumers should use IOCTLs `GET_BAD_BLOCKS_COUNT` / `GET_ECC_STATS` (§3.4). |
| **Inputs** | Device handle; outputs via pointer or internal aggregation per signatures §4.3. |
| **Outputs** | Bad block count; ECC stats (implementation detail **Unknown** beyond baseline naming). |
| **Dependencies** | Legacy device handle; uses wrap path for threading (§9.1). |
| **Constraints** | Intended for offline/maintenance — scans can be expensive (§9.3). |
| **Related files / components** | `include/nand_diag_api.h`, `src/nand_diag_api.c`. |
| **Confidence** | **Confirmed** for role and BDL reachability caveat. |
| **Known issues** | Full-device iteration concerns (§9.3). |
| **Open questions** | — |

---

## 9. Linux mmap emulator (`include/nand_linux_mmap_emul.h`, `src/nand_linux_mmap_emul.c`)

| Field | Content |
| --- | --- |
| **Name** | Memory-mapped file NAND emulator |
| **Purpose** | Back the Linux-target stack with a file-backed virtual NAND for host tests. |
| **Current behavior summary** | Configurable path/size/`keep_dump`; empty name uses temp file; size multiple constraints and default 128 MiB (§4.7). Each `nand_emul_init` memset region to `0xFF`. Exposes read/write/erase and optional stats API when `CONFIG_NAND_ENABLE_STATS`. |
| **Inputs** | `nand_file_mmap_emul_config_t`; emulator operation calls from `nand_impl_linux`. |
| **Outputs** | Emulated NAND behavior; optional statistics helpers when `CONFIG_NAND_ENABLE_STATS` (baseline §4.7). |
| **Dependencies** | Linux target build only. |
| **Constraints** | Synthetic JEDEC identity and geometry constants — not real silicon (§4.7). `flash_file_size` validation vs block multiple (§6). Persisting data across inits with `keep_dump`: baseline notes prior contents do not survive init cycle (**Inferred** limitation paragraph §4.7). |
| **Related files / components** | `include/nand_linux_mmap_emul.h`, `src/nand_linux_mmap_emul.c`. |
| **Confidence** | **Confirmed** for most behaviors; **Inferred** for `keep_dump` persistence nuance as labeled in baseline. |
| **Known issues** | — |
| **Open questions** | — |

---

## 10. Linux NAND implementation (`src/nand_impl_linux.c`)

| Field | Content |
| --- | --- |
| **Name** | Linux counterpart to `nand_impl` |
| **Purpose** | Drive logical NAND operations via mmap emulator instead of SPI. |
| **Current behavior summary** | Swapped in for `linux` target per §3.3 / §5.1. Uses synthetic device identity for BDL info IOCTL consumers on host (§4.7). |
| **Inputs** | Emulator config through init chain; same upper-layer ops as target impl from Dhara/glue perspective (**Unknown** at per-function granularity in baseline). |
| **Outputs** | Emulated flash results. |
| **Dependencies** | `nand_linux_mmap_emul.c`. |
| **Constraints** | Same validation rules for file size multiple (§6). |
| **Related files / components** | `src/nand_impl_linux.c`. |
| **Confidence** | **Confirmed** for role; **Unknown** for full API mapping vs `nand_impl.c`. |
| **Known issues** | — |
| **Open questions** | — |

---

## 11. Post-program read-back verification (`CONFIG_NAND_FLASH_VERIFY_WRITE`)

| Field | Content |
| --- | --- |
| **Name** | Write verification (Kconfig-gated) |
| **Purpose** | Debug aid: after each page program, read back and compare; log mismatch. |
| **Current behavior summary** | Doubles programs; reduces throughput; not a correctness guarantee for production (§6, §9.3). Applies in both legacy and BDL builds when enabled (§3.4). |
| **Inputs** | Written page data; Kconfig on. |
| **Outputs** | Logs on mismatch; operational latency increase. |
| **Dependencies** | Physical read/program path. |
| **Constraints** | Off by default. |
| **Related files / components** | **Unknown** exact translation units from baseline (Kconfig only names option). |
| **Confidence** | **Confirmed** for behavior description; **Unknown** for precise code location (baseline references option, not file). |
| **Known issues** | Throughput impact (§9.3). |
| **Open questions** | — |

---

## 12. Automatic data refresh on soft ECC (`spi_nand_flash_read_page`)

| Field | Content |
| --- | --- |
| **Name** | ECC threshold-driven page refresh |
| **Purpose** | When read shows corrected bits above `chip.ecc_data.ecc_data_refresh_threshold`, re-program the page to refresh marginal data. |
| **Current behavior summary** | Implemented in `src/nand.c::spi_nand_flash_read_page` using `nand_ecc_exceeds_data_refresh_threshold` (baseline §2 item 12). |
| **Inputs** | ECC status from read path; threshold from chip ECC data. |
| **Outputs** | Possible implicit reprogram of page. |
| **Dependencies** | ECC reporting from `nand_impl`; write path. |
| **Constraints** | Threshold comes from vendor/geometry setup (**Unknown** per-chip tuning from baseline). |
| **Related files / components** | `src/nand.c` (baseline citation only). |
| **Confidence** | **Confirmed** for existence and location reference. |
| **Known issues** | — |
| **Open questions** | Interaction with host emulator ECC constants (**Unknown** beyond emulator constants listed §4.7). |

---

## 13. Host WL statistics (`CONFIG_NAND_ENABLE_STATS`, Linux only)

| Field | Content |
| --- | --- |
| **Name** | Host-side statistics gathering (emulator) |
| **Purpose** | Optional counters for wear-leveling / ops on Linux host (baseline §3.4, §4.4). |
| **Current behavior summary** | When enabled, augments emulator; `nand_emul_clear_stats` implemented per baseline §4.7. |
| **Inputs** | Kconfig; emulator operations. |
| **Outputs** | Statistics buffers (**Unknown** exact counter semantics beyond baseline naming). |
| **Dependencies** | Linux mmap emulator. |
| **Constraints** | Linux-only Kconfig (§4.4). |
| **Related files / components** | **Partially Unknown** — baseline references header and `nand_linux_mmap_emul.c` for clear stats. |
| **Confidence** | **Confirmed** for intent and Linux-only gate; **Unknown** for full stats schema. |
| **Known issues** | — |
| **Open questions** | — |

---

## 14. Shared test helpers (`include/spi_nand_flash_test_helpers.h`, `src/spi_nand_flash_test_helpers.c`)

| Field | Content |
| --- | --- |
| **Name** | Test fixture helpers |
| **Purpose** | Deterministic buffer fill/check helpers (fixed pattern and seeded variants) shared by on-target and host tests. |
| **Current behavior summary** | Stable public API per §4.0. |
| **Inputs** | Buffers, lengths, optional seed. |
| **Outputs** | Filled buffers; boolean/check results per API. |
| **Dependencies** | None stated beyond standard library/component link. |
| **Constraints** | Exported from `include/` — stability commitment (§4.0). |
| **Related files / components** | `include/spi_nand_flash_test_helpers.h`, `src/spi_nand_flash_test_helpers.c`. |
| **Confidence** | **Confirmed**. |
| **Known issues** | — |
| **Open questions** | — |

---

## 15. Shared types (`include/nand_device_types.h`)

| Field | Content |
| --- | --- |
| **Name** | NAND device types |
| **Purpose** | Shared structs/enums: ECC status/data, geometry, device info for public and internal boundaries. |
| **Current behavior summary** | Always available header (§3.1). |
| **Inputs** | — |
| **Outputs** | Types for consumers. |
| **Dependencies** | — |
| **Constraints** | Stable public API (§4.0). |
| **Related files / components** | `include/nand_device_types.h`. |
| **Confidence** | **Confirmed**. |
| **Known issues** | — |
| **Open questions** | — |

---

## 16. Build / packaging metadata (`CMakeLists.txt`, `Kconfig`, `idf_component.yml`)

| Field | Content |
| --- | --- |
| **Name** | Build system and declared dependencies |
| **Purpose** | Select sources by target and IDF version; expose Kconfig options; declare component dependencies and Dhara version. |
| **Current behavior summary** | BDL gated IDF ≥ 6.0 in Kconfig and CMake; Linux swaps sources; SPI driver component selection by IDF minor (§6, §4.5). |
| **Inputs** | sdkconfig; IDF version; target. |
| **Outputs** | Linked component object code and transitive requirements. |
| **Dependencies** | ESP-IDF core; conditional packages above. |
| **Constraints** | IDF window `>= 5.0` declared in manifest (§4.0). |
| **Related files / components** | `CMakeLists.txt`, `Kconfig`, `idf_component.yml`. |
| **Confidence** | **Confirmed**. |
| **Known issues** | — |
| **Open questions** | — |

---

## 17. Test applications (`test_app/`, `host_test/`)

| Field | Content |
| --- | --- |
| **Name** | On-target and Linux host tests |
| **Purpose** | Validate legacy and BDL modes; host exercises mmap + FTL scenarios; CI sdkconfig presets and pytest drivers per baseline §8. |
| **Current behavior summary** | Split test files for legacy vs BDL; host includes FTL-focused cases; pytest wrappers named in baseline. |
| **Inputs** | Built firmware / host binary; pytest invocation. |
| **Outputs** | Pass/fail signals for CI/local runs. |
| **Dependencies** | Full component stack under test configurations. |
| **Constraints** | CI presets pin mode, not specific chip on bench (**Unknown** chip, §11.2). |
| **Related files / components** | `test_app/main/test_app_main.c`, `test_spi_nand_flash.c`, `test_spi_nand_flash_bdl.c`, `sdkconfig.ci.default`, `sdkconfig.ci.bdl`, `pytest_spi_nand_flash.py`; `host_test/main/...`, `pytest_nand_flash_linux.py` (§8). |
| **Confidence** | **Confirmed** for layout references. |
| **Known issues** | — |
| **Open questions** | Hardware bench coverage (**Unknown**, §11.2). |

---

## Cross-cutting inventory notes

| Topic | Baseline reference | Note |
| --- | --- | --- |
| Stability contract | §4.0 | All `include/` exports stable; `priv_include/` internal. |
| Concurrency | §9.1 | Single mutex per handle; raw Flash BDL unsynchronized; mixing raw + Dhara unsupported. |
| Power loss | §9.4 | Dhara-only guarantees for WL path; none added by component. |
| Explicit non-features | §10 | e.g. FatFs-on-BDL, encryption, multi-device registry. |
| Performance numbers | §11.2 | **Unknown** in baseline. |
| Confirmed defect queue | §11.4 | [`known-bugs.md`](known-bugs.md) — not duplicated in module rows. |

---

## Document history

| Version | Note |
| --- | --- |
| 1.0 | Initial inventory derived from `baseline.md` only. |
| 1.1 | §11.4 defects moved to [`known-bugs.md`](known-bugs.md); inventory cross-links only. |
