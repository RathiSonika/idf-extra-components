# Step 06 — `nand_is_free` uses layout path when Kconfig on

**PR identifier:** `oob-layout-06`  
**Depends on:** steps **01–05**  
**Estimate:** ~150–400 LOC

## Goal

In [`src/nand_impl.c`](../../../src/nand_impl.c) **`nand_is_free`**:

- **`CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT=n`:** Keep **exact** current implementation (read 4 bytes at `page_size` column, free iff bytes 2–3 == `0xFF, 0xFF`).
- **`y`:** Same SPI traffic as legacy (single 4-byte read at OOB base via `spi_nand_read`), but extract the marker through the layout path:
  1. Read 4 bytes into the existing **DMA-capable** buffer (see SPI-buffer-discipline rule below).
  2. Build a **stack-local** `spi_nand_oob_xfer_ctx_t` (per step 05's concurrency contract) pointing at those 4 bytes.
  3. Call `nand_oob_gather(&ctx, /*logical_off=*/0, marker, /*len=*/2)` — note **logical_offset 0 length 2** matches step 03/04 where the single free region is at physical offset 2 length 2 (so logical 0 ⇄ physical 2).
  4. `*is_free_status = (marker[0] == 0xFF && marker[1] == 0xFF)`.

**Critical:** Total SPI behavior must match legacy (same number of commands, same bytes on the wire, same column address).

## SPI-buffer-discipline rule (load-bearing — baseline §6 DMA history)

The current implementation reads markers into `handle->read_buffer` (allocated via `heap_caps_aligned_alloc(... MALLOC_CAP_DMA | MALLOC_CAP_8BIT)`, see `nand_impl.c:114, 401`). This is **not** decorative — baseline §6 documents prior regressions (0.19.0, 0.21.0) when SPI transfer buffers were not DMA-capable / aligned.

- **Do NOT** introduce a stack-local `uint8_t oob[4]` as the buffer handed to `spi_nand_read` / `spi_nand_program_load`.
- **Do** continue using `handle->read_buffer` (or `handle->temp_buffer`) as the actual SPI transfer buffer.
- The **xfer ctx's** `oob_raw` pointer can then point at `handle->read_buffer` for the duration of the call — read-only consumption, stack-local ctx, lock-free.
- Apply the same rule symmetrically in steps 07 / 08 / 09 (program-load and verify-write paths).

## Non-goals

- Changing definition of "free" — still `0xFF, 0xFF` in page-used field.
- Changing the SPI command count or sequence.

## Files to touch

| File | Action |
|------|--------|
| [`src/nand_impl.c`](../../../src/nand_impl.c) | Conditional branch |
| `priv_include/nand_oob_helpers.h` | **Optional** inline helper `nand_oob_read_page_used_marker(handle, page, &is_free)` to keep the diff small and reusable from step 08 |

## Implementation checklist

1. Extract legacy logic into `static esp_err_t nand_is_free_legacy(...)` **or** duplicate minimal `#else` branch — avoid deep nesting.
2. Kconfig `y` path:
   - Reuse existing `read_page_and_wait` + `spi_nand_read(handle, handle->read_buffer, column_addr, 4)` at OOB base (`get_column_address(handle, block, page_size)`). **Same call shape as today.**
   - Stack-local ctx: `spi_nand_oob_xfer_ctx_t ctx;` then `nand_oob_xfer_ctx_init(&ctx, handle->oob_layout, handle, SPI_NAND_OOB_CLASS_FREE_ECC, handle->read_buffer, 4);`.
   - Gather PAGE_USED field (logical offset **0**, len **2**).
   - `*is_free_status = (marker[0] == 0xFF && marker[1] == 0xFF)`.
3. Log tags unchanged where possible.
4. Verify plane math unchanged (`get_column_address` still folds plane bit via block index — see proposal §1.2 wording).

## Testing

- Legacy: all existing **target** tests with default sdkconfig (`n`).
- Local: enable `y`, run same tests — expect **identical** pass/fail.

## Acceptance criteria

- [ ] Binary-identical SPI sequence vs legacy for default layout (reviewer checklist: same `spi_nand_read` count, same column, same length).
- [ ] No new `heap_caps_*` allocations on this path.
- [ ] Buffer handed to `spi_nand_read` is `handle->read_buffer` (DMA-capable), not a stack array.
- [ ] Stack-local xfer ctx — no embedded ctx on the handle (step 05 contract).

## Risks

- Accidentally reading wrong column — double-check `column_addr = get_column_address(handle, block, handle->chip.page_size)`.
- Accidentally swapping logical/physical offsets — note that with step 03's clean BBM split, gather at logical offset **0** (not 2) is what you want, because the single free region itself starts at physical offset 2.

## Notes for implementers

- Linux parity **not** in this PR if README Linux rule applies — **must** land step **10** before release candidate; interim OK for bisect but document.
