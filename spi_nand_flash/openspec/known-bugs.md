# Known bugs / follow-ups — `spi_nand_flash`

> **Artifact type:** OpenSpec discovery — confirmed defects and intended fix directions.  
> **Source of truth:** [`baseline.md`](baseline.md) §11.4. This file mirrors that section so the [feature inventory](feature-module-inventory.md) stays focused on behavior and operational constraints (e.g. §9) without duplicating defect tracking.  
> **Scope:** These issues are **not** part of the stable behavior that changes must preserve; they are explicitly queued to be fixed. When §11.4 changes in `baseline.md`, update this file to match.

---

## 11.4 Known bugs / follow-ups (queued for next release)

These are issues confirmed against the current tree (commit at the time
this baseline was written). They are **not** part of the stable behavior
that future OpenSpec changes need to preserve — they are explicitly
queued to be fixed. Each entry lists what is broken, where, and what an
acceptable fix shape looks like, so the next change can pick them up
directly.

### 11.4.1 `nand_emul_get_stats` is declared in the public header but never defined

- **Surface:** `include/nand_linux_mmap_emul.h` (line ~133, gated on
  `CONFIG_NAND_ENABLE_STATS`) declares:
  ```c
  void nand_emul_get_stats(spi_nand_flash_device_t *handle,
                           size_t *read_ops, size_t *write_ops,
                           size_t *erase_ops, size_t *read_bytes,
                           size_t *write_bytes);
  ```
- **Defect:** No definition exists in `src/nand_linux_mmap_emul.c` (only
  `nand_emul_clear_stats` is implemented there). Any consumer that calls
  the symbol with `CONFIG_NAND_ENABLE_STATS=y` will fail to link with an
  unresolved external. Today no in-tree caller exercises it, so the gap
  is dormant.
- **Why it matters:** the header is part of the **stable public API on
  the Linux target** per §4.0, so an out-of-tree host-test author who
  enables the Kconfig is justified in expecting the symbol to be
  callable. The current state silently breaks that contract.
- **Acceptable fix shape (any one of these):**
  1. Implement `nand_emul_get_stats` in `src/nand_linux_mmap_emul.c`,
     reading the counters from `nand_mmap_emul_handle_t::stats` (which
     already exist behind `CONFIG_NAND_ENABLE_STATS`) and writing them
     through the `size_t*` out-parameters.
  2. Or remove the declaration from `include/nand_linux_mmap_emul.h`
     and document that only `nand_emul_clear_stats` is exposed.
- **Validation:** add a host-test case under `host_test/main/` that
  performs a known number of `nand_emul_read` / `_write` / `_erase_block`
  calls and asserts the counters match.

### 11.4.2 PSRAM / DMA-capable heap placement of NAND state is not enforced

- **Surface:**
  - `src/nand_impl.c::nand_init_device` allocates the device handle
    (`spi_nand_flash_device_t`) with `heap_caps_calloc(...,
    MALLOC_CAP_DEFAULT)`. The handle owns the FreeRTOS mutex created by
    `xSemaphoreCreateMutex()` (which itself goes through the default
    allocator) plus the cached `spi_nand_flash_config_t`.
  - `src/dhara_glue.c::dhara_init` allocates
    `spi_nand_flash_dhara_priv_data_t` (Dhara map + Dhara nand context)
    with `heap_caps_calloc(..., MALLOC_CAP_DEFAULT)`.
  - Only the per-page work / read / temp buffers in
    `src/nand_impl.c::nand_init_device` use
    `MALLOC_CAP_DMA | MALLOC_CAP_8BIT` via `heap_caps_aligned_alloc`.
- **Defect:** On targets where the default heap is routed to PSRAM
  (e.g. `CONFIG_SPIRAM_USE_MALLOC=y` with PSRAM as primary), the
  device handle, the Dhara private struct, and the FreeRTOS mutex
  control block can all land in PSRAM. FreeRTOS sync primitives and
  hot-path control structures are conventionally expected to live in
  internal RAM; placing them in PSRAM has not been validated on
  hardware in this component, and is a likely source of bring-up
  failures (cache thrash, ISR-context access patterns, etc.).
- **Why it matters:** the component declares no Kconfig or runtime
  check guarding against this, so the failure mode is silent — the
  driver will appear to work in non-stress smoke tests and can fail
  later under load or under unrelated PSRAM contention.
- **Acceptable fix shape:**
  1. Switch the device-handle and Dhara-private allocations from
     `MALLOC_CAP_DEFAULT` to `MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT`,
     so they are always pulled from internal RAM regardless of PSRAM
     configuration. Keep the page buffers on `MALLOC_CAP_DMA |
     MALLOC_CAP_8BIT` as today.
  2. Audit the FreeRTOS mutex placement: if needed, allocate the mutex
     control block via `xSemaphoreCreateMutexStatic` with a
     statically-located `StaticSemaphore_t` carried inside the device
     handle (which is itself in internal RAM after fix #1).
- **Validation:**
  - Build the on-target test app with `CONFIG_SPIRAM_USE_MALLOC=y` and
    PSRAM as the default allocator on a target that has PSRAM (e.g.
    ESP32-S3, ESP32-P4) and run the existing legacy-mode and BDL-mode
    test suites.
  - Add a debug-only assertion in `nand_init_device` /
    `dhara_init` that the returned pointers are in internal RAM
    (`esp_ptr_internal()`), enabled under `assert()` in debug builds.

---

## Document history

| Version | Note |
| --- | --- |
| 1.0 | Split from feature inventory; content aligned with `baseline.md` §11.4. |
