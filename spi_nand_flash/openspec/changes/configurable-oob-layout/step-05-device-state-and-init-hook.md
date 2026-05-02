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
  - **Optionally**, a small const `spi_nand_oob_region_desc_t cached_regs[SPI_NAND_OOB_MAX_REGIONS]` + `uint8_t cached_reg_count` per class (assigned once at init, read-only after) — purely a perf cache so steps 06–09 don't re-walk `free_region` callbacks per call.
- Reason: raw Flash BDL paths (`src/nand_flash_blockdev.c`) call `nand_*` primitives **without taking `handle->mutex`** (baseline §9.1). A mutable cached ctx on the handle would race between two raw-BDL callers on different tasks. Stack-local ctx is lock-free by construction.
- **Non-ISR contract:** none of the layout helpers (init, scatter, gather, field read/write) are ISR-safe. They are only callable from task context, same as the rest of `nand_impl`. Document this in the header.

## No new heap allocations (load-bearing — known-bugs.md §11.4.2)

- All new state lives **inside** the existing `spi_nand_flash_device_t` allocation. Do **not** introduce separate `heap_caps_*` calls for layout state.
- Reason: the device handle already has a known PSRAM-placement issue (baseline §11.4.2). Folding layout state into the same allocation means when that bug is fixed (move handle to `MALLOC_CAP_INTERNAL`), the layout state moves with it for free. A separate allocation here would compound the problem.

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
   - **Optional perf cache:** `spi_nand_oob_region_desc_t oob_cached_regs_free_ecc[SPI_NAND_OOB_MAX_REGIONS]; uint8_t oob_cached_reg_count_free_ecc;` — populated once at init by walking `free_region` for `FREE_ECC`. Reused as a read-only source by stack-local xfer ctx in steps 06–09.
   - **Do NOT** add `spi_nand_oob_xfer_ctx_t` as an embedded field. That struct holds the per-call `oob_raw` pointer and is mutable per call; embedding it would break the concurrency contract above.
2. Init function responsibilities:
   - Set `oob_layout = nand_oob_layout_get_default()`.
   - If layout uses `oob_bytes == 0` pattern (step 03 Pattern A), fill the runtime cache from `chip.emulated_page_oob` (Linux) or the chip's spare size (target).
   - Initialize field specs: `PAGE_USED` `length=2`, `class=FREE_ECC`, `logical_offset=0`, `assigned=true`.
   - If using the perf cache: walk `layout->ops->free_region(handle, section, &desc)` once per class; copy descriptors into `oob_cached_regs_*[]`; record count.
3. Deinit / fail paths: no leaks — no heap if embedded-only design (which is the requirement above).
4. **Mutex:** Init runs single-threaded before mutex use; no locking change here. After init, all new fields are read-only, so no lock needed for reads on hot paths.

## Testing

- Boot init only; existing tests unchanged (Kconfig `n`).
- Optional: enable `y` in local sdkconfig and confirm init returns `ESP_OK` and that `handle->oob_layout != NULL` and `handle->oob_fields[PAGE_USED].assigned == true`.

## Acceptance criteria

- [ ] Kconfig `n`: struct size/layout unchanged vs baseline (no new members).
- [ ] Kconfig `y`: after `nand_init_device`, handle has non-NULL `oob_layout` and valid field spec for PAGE_USED (`length=2`, `logical_offset=0`, `class=FREE_ECC`, `assigned=true`).
- [ ] Kconfig `y`: **no** mutable xfer ctx embedded in the handle (review checklist).
- [ ] Kconfig `y`: **no** new `heap_caps_*` calls — all new state inside the existing handle allocation (review checklist; PR description must enumerate added bytes).
- [ ] No functional change to read/program yet (behavior identical).

## Risks

- RAM growth per handle when `y` — document bytes added in PR description. The `cached_regs[]` perf cache is the largest contributor; if `SPI_NAND_OOB_MAX_REGIONS` is set high (step 02 Q3), reconsider whether the cache is worth it for the default 1-region layout.

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
