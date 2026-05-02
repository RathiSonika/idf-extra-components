# Step 08 — `nand_is_bad` and `nand_mark_bad` use layout BBM descriptor

**PR identifier:** `oob-layout-08`  
**Depends on:** steps **01–07**  
**Estimate:** ~250–500 LOC

## Goal

Replace hardcoded BBM offsets/patterns with values from **`handle->oob_layout`** when Kconfig **`y`**:

- **`nand_is_bad`:** Currently reads first block page, 4 bytes — BBM uses bytes 0–1. With layout:
  - Confirm **which page** via `bbm.check_pages_mask` — default = first page only.
  - Read `bbm.bbm_length` bytes at OOB offset `bbm.bbm_offset` (default: 2 bytes at offset 0). For default layout this means reading 4 bytes (legacy) or 2 bytes — see "SPI delta" rule below.
  - Compare to `bbm.good_pattern` — legacy: good iff `0xFF, 0xFF`.
- **`nand_mark_bad`:** Program pattern `{0x00, 0x00, 0xFF, 0xFF}` at same positions as today — use scatter consistent with step 07 (BBM bytes from a "bad" pattern, PAGE_USED bytes left as `0xFF, 0xFF` ⇒ free).

**`n`:** Preserve exact legacy code paths byte-for-byte.

## SPI-delta-vs-legacy rule (load-bearing)

Today `nand_is_bad` reads **4 bytes** at OOB base (`nand_impl.c:231`), even though only the first 2 bytes are used. Some chips/controllers prefer this round count.

- **`y` path must minimize SPI delta vs legacy:** keep the **4-byte** read at OOB base for the default layout. Just consume the first `bbm.bbm_length` bytes (default: 2) from the buffer for the comparison. Do **not** issue a 2-byte transfer in the name of "purity" — that's a wire-level behavior change vs legacy.
- For non-default layouts that legitimately need a different read length, that part's vendor module owns the transfer-shaping decision. Out of scope here.

## SPI-buffer-discipline rule (mirror of step 06/07)

Read into `handle->read_buffer` (DMA-capable). Marker bytes for `nand_mark_bad` go through `handle->temp_buffer` for the `spi_nand_program_load`. **No stack arrays as SPI transfer buffers.**

## Non-goals

- Supporting BBM only on last page of block unless layout bitmask requests it — implement bitmask handling now **if cheap**, else document TODO for step 12.

## Files to touch

| File | Action |
|------|--------|
| [`src/nand_impl.c`](../../../src/nand_impl.c) | BBM paths |

## Implementation checklist

1. Factor helper `static bool nand_oob_bbm_good(const spi_nand_oob_layout_t *layout, const uint8_t *oob_slice)` — returns `true` iff first `bbm.bbm_length` bytes of `oob_slice` match `bbm.good_pattern`.
2. **`nand_is_bad` + `y`:**
   - Select page index within block per `bbm.check_pages_mask` (default first page only — preserve legacy).
   - Read **4 bytes** at OOB base into `handle->read_buffer` (legacy SPI shape).
   - Use the helper to evaluate good/bad.
3. **`nand_mark_bad` + `y`:**
   - Compose the 4-byte buffer in `handle->temp_buffer`: BBM bytes from a "bad" pattern (typically zero-fill of `bbm.bbm_length` bytes), remaining bytes = `0xFF, 0xFF` so the page-used slot still reads as free.
   - Issue exactly **one** `spi_nand_program_load` + **one** `program_execute_and_wait` (same as legacy).
4. **`CONFIG_NAND_FLASH_VERIFY_WRITE` parity (load-bearing):** preserve the existing `s_verify_write` call after `nand_mark_bad`'s program (`nand_impl.c:275`). The `y` path must not skip or shorten verify; the bytes being verified are the layout-composed 4 bytes, identical to legacy for default layout.

## Testing

- Bad-block tests in `test_app` / host if any; manual BBM scenario.

## Acceptance criteria

- [ ] Default layout: `is_bad` / `mark_bad` decisions match legacy golden behavior byte-for-byte.
- [ ] First-page BBM check preserved for default layout descriptor (`check_pages_mask = FIRST_PAGE`).
- [ ] Same SPI shape as legacy: 4-byte read at OOB base for `is_bad`, one `program_load` + one `program_execute` for `mark_bad`.
- [ ] `CONFIG_NAND_FLASH_VERIFY_WRITE=y` still re-reads marker bytes after `nand_mark_bad` (same `s_verify_write` call as today).
- [ ] Buffers handed to `spi_nand_read` / `spi_nand_program_load` are DMA-capable (`handle->read_buffer` / `handle->temp_buffer`), not stack.
- [ ] BBM "bad" pattern is composed from layout fields (offset, length, good_pattern → derive bad), not a hardcoded `{0x00, 0x00}` literal.

## Risks

- Partial BBM length mismatch — enforce `bbm.bbm_length == 2` assert for default table during init (in step 05's helper).
- Skipping verify on `y` is silent — verify is gated by Kconfig and by reviewer attention.

## Notes for implementers

- If multiple pages in mask, document iteration order; legacy only checks first page — default layout must keep that.
