# Step 05 — Device state + init assigns layout and field table

**PR identifier:** `oob-layout-05`  
**Depends on:** steps **01–04**  
**Estimate:** ~200–500 LOC

## Goal

Attach OOB layout state to **`spi_nand_flash_device_t`** when Kconfig is **`y`**:

- Pointer to active `spi_nand_oob_layout_t` (default from step 03 for all chips initially).
- Field specs array: at minimum **`PAGE_USED`** length **2**, class `FREE_ECC`, `logical_offset = 0` (inside the single free region at OOB physical offset 2 — see step 03/04). This still maps physically to OOB bytes 2–3 — same on-flash bytes as today.
- **Cached IMMUTABLE region metadata only** (e.g. a small `regs[]` snapshot resolved at init by walking `layout->ops->free_region` once per relevant `class`). **No** mutable per-call state on the handle.

When Kconfig is **`n`**, **no new struct members** (use `#ifdef CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT` around fields).

## Concurrency / lifetime contract (load-bearing — proposal §2.2 + baseline §9.1)

- **No mutable xfer ctx on the handle.** The xfer context (`spi_nand_oob_xfer_ctx_t` from step 04) carries an `oob_raw` pointer plus per-call bytes; it is **stack-local per call site** in steps 06–09. The handle only holds:
  - `const spi_nand_oob_layout_t *oob_layout;` (points at static rodata)
  - `spi_nand_oob_field_spec_t oob_fields[SPI_NAND_OOB_FIELD_COUNT];` (assigned once at init, read-only after)
   - **Optionally**, small const `spi_nand_oob_region_desc_t` caches + counts for **FREE_ECC** and **FREE_NOECC** (each up to `SPI_NAND_OOB_MAX_REGIONS`, assigned once at init, read-only after) — perf cache so steps 06–09 don't re-walk `free_region` per call; default layout only populates FREE_ECC.
- Reason: raw Flash BDL paths (`src/nand_flash_blockdev.c`) call `nand_*` primitives **without taking `handle->mutex`** (baseline §9.1). A mutable cached ctx on the handle would race between two raw-BDL callers on different tasks. Stack-local ctx is lock-free by construction.
- **Non-ISR contract:** none of the layout helpers (init, scatter, gather, field read/write) are ISR-safe. They are only callable from task context, same as the rest of `nand_impl`. Document this in the header.

## Internal RAM for handle + Dhara private data ([`known-bugs.md`](../../known-bugs.md) §11.4.2 — **fix in this step**)

- Change **`nand_init_device`** (`src/nand_impl.c`) so the **`spi_nand_flash_device_t`** allocation uses **`MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT`** (not `MALLOC_CAP_DEFAULT` alone), so the handle, mutex, and embedded layout fields never land in PSRAM when the default heap is external.
- Change **`dhara_init`** (`src/dhara_glue.c`) so **`spi_nand_flash_dhara_priv_data_t`** is allocated with **`MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT`** for the same reason.
- **Keep** page/read/temp work buffers on **`MALLOC_CAP_DMA | MALLOC_CAP_8BIT`** as today.
- **Optional hardening:** `xSemaphoreCreateMutexStatic` + `StaticSemaphore_t` embedded in the handle if reviewers want the mutex control block guaranteed internal too — not required if mutex allocation follows internal handle memory policy on your IDF version; document the choice in the PR.
- **Validation:** debug build may `assert(esp_ptr_internal(handle))` after alloc (as suggested in known-bugs §11.4.2); run **`test_app`** with **`CONFIG_SPIRAM_USE_MALLOC=y`** on a PSRAM board if available.

## No *extra* heap allocations for layout-only blobs

- All **new layout-related** bytes live **inside** the existing `spi_nand_flash_device_t` allocation (same object now forced internal per above). Do **not** introduce a **second** `heap_caps_*` object just for layout tables.

## Non-goals

- Still **do not** change `nand_prog` / `nand_is_free` logic (steps 06–07).
- Per-vendor layout pointer selection — all chips use default layout accessor until vendor override exists.

## Files to touch

| File | Action |
|------|--------|
| [`priv_include/nand.h`](../../../priv_include/nand.h) | Add conditional fields to `struct spi_nand_flash_device_t` (read-only after init) |
| [`src/nand_impl.c`](../../../src/nand_impl.c) | In `nand_init_device`, after geometry known, call `nand_oob_device_layout_init(handle)` or inline guarded block |
| `priv_include/nand_oob_device.h` + `src/nand_oob_device.c` | **Optional split** — helper `nand_oob_attach_default_layout(handle)` keeps `nand_impl.c` diff small and lets step 10 reuse the same helper for Linux init |

## Implementation checklist

1. Extend device struct (ifdef guarded; **all read-only after init**):
   - `const spi_nand_oob_layout_t *oob_layout;`
   - `spi_nand_oob_field_spec_t oob_fields[SPI_NAND_OOB_FIELD_COUNT];` (small fixed count)
   - **Optional perf cache:** `oob_cached_regs_free_ecc[]` / `oob_cached_reg_count_free_ecc` and symmetric `oob_cached_regs_free_no_ecc[]` / `oob_cached_reg_count_free_no_ecc` — populated once at init from each `free_region` descriptor (`programmable && ecc_protected` → ECC cache, `programmable && !ecc_protected` → NOECC cache). Reused read-only by stack-local xfer ctx in steps 06–09.
   - **Do NOT** add `spi_nand_oob_xfer_ctx_t` as an embedded field. That struct holds the per-call `oob_raw` pointer and is mutable per call; embedding it would break the concurrency contract above.
2. Init function responsibilities:
   - **ECC / feature register read order (load-bearing — proposal §2.1a):** Run vendor/chip-specific init that **enables or selects internal ECC** (and any related configuration) **first**, so subsequent **Get Feature** / configuration reads reflect the **same** ECC mode the driver uses for I/O. Only **after** that, read the ECC-related bits used as the **ECC mode key** for layout lookup `(MI, DI, ECC mode)` and attach the matching layout (today: still default layout for all supported parts until vendor tables land).
   - Set `oob_layout = nand_oob_layout_get_default()` (or table lookup in a later step).
   - If layout uses `oob_bytes == 0` pattern (step 03 Pattern A), fill the runtime cache from `chip.emulated_page_oob` (Linux) or the chip's spare size (target).
   - Initialize field specs: `PAGE_USED` `length=2`, `class=FREE_ECC`, `logical_offset=0`, `assigned=true`.
   - If using the perf cache: walk `layout->ops->free_region(handle, section, &desc)` once; split programmable regions into `oob_cached_regs_free_ecc` vs `oob_cached_regs_free_no_ecc` by `ecc_protected`; record counts.
3. Deinit / fail paths: no leaks — no heap if embedded-only design (which is the requirement above).
4. **Mutex:** Init runs single-threaded before mutex use; no locking change here. After init, all new fields are read-only, so no lock needed for reads on hot paths.

## Testing

- Boot init only; existing tests unchanged (Kconfig `n`).
- Optional: enable `y` in local sdkconfig and confirm init returns `ESP_OK` and that `handle->oob_layout != NULL` and `handle->oob_fields[PAGE_USED].assigned == true`.

## Acceptance criteria

- [ ] **§11.4.2:** Device handle and Dhara private struct allocate from **internal** RAM (`MALLOC_CAP_INTERNAL`); evidence in PR (assert log or `idf.py size` + config note).
- [ ] Kconfig `n`: **no new `#ifdef` OOB layout fields** on the handle vs baseline. §11.4.2 allocator changes may apply to **all** builds — note in PR / CHANGELOG (step 12) if user-visible (e.g. slightly different heap usage).
- [ ] Kconfig `y`: after `nand_init_device`, handle has non-NULL `oob_layout` and valid field spec for PAGE_USED (`length=2`, `logical_offset=0`, `class=FREE_ECC`, `assigned=true`).
- [ ] Kconfig `y`: **no** mutable xfer ctx embedded in the handle (review checklist).
- [ ] Kconfig `y`: layout state lives in the **same** handle object as §11.4.2 — no separate heap object for layout metadata.
- [ ] ECC-sensitive register reads for layout happen **after** chip ECC configuration is applied (review order vs vendor `init`).
- [ ] No functional change to read/program yet (behavior identical).

## Risks

- RAM growth per handle when `y` — document bytes added in PR description. The `cached_regs[]` perf cache is the largest contributor; **`SPI_NAND_OOB_MAX_REGIONS` is 8** (step 02 / root [`README.md`](README.md)) — for the default single-region layout the cache stays small.

## Notes for implementers

- **`chip_ctx` for layout ops:** Pass `handle` consistently (or `&handle->chip` if step 02's ops signature uses chip context). Document the choice in the helper header so steps 06–09 cannot pass the wrong pointer.
- The "stack-local xfer ctx" rule means each of steps 06–09 will do something like:
  ```c
  spi_nand_oob_xfer_ctx_t ctx;
  uint8_t oob[/* small fixed */];
  nand_oob_xfer_ctx_init(&ctx, handle->oob_layout, handle, SPI_NAND_OOB_CLASS_FREE_ECC, oob, sizeof(oob));
  /* ... scatter/gather ... */
  ```
  No allocation, no lock, no per-handle mutation. That is the design intent.
