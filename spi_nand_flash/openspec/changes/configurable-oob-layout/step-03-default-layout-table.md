# Step 03 — Default layout object (§1.2 equivalent)

**PR identifier:** `oob-layout-03`  
**Depends on:** steps **01**, **02**  
**Estimate:** ~200–450 LOC

## Goal

Provide **one global default** `spi_nand_oob_layout_t` + `spi_nand_ooblayout_ops_t` implementation that describes the **current** driver behavior (proposal §1.2) **using the RFC's clean BBM-vs-free split**:

- **BBM descriptor (separate from any free region):** `bbm_offset = 0`, `bbm_length = 2`, `good_pattern = {0xFF, 0xFF}`, `check_pages_mask = SPI_NAND_BBM_CHECK_FIRST_PAGE` (matches `nand_is_bad` today).
- **Single programmable user-free region:** OOB offset **2**, length **2**, `programmable = true`, `ecc_protected = true`. This is the page-used marker territory only — **BBM bytes 0–1 are NOT enumerated by `free_region`**, per RFC: *"`free()` ... must NOT return BBM or ECC parity regions"*.
- **Field placement (assigned in step 05):** `PAGE_USED` length **2**, class `FREE_ECC`, `logical_offset = 0` inside the single 2-byte free region. This still maps physically to OOB bytes 2–3 — same on-flash layout as today.

Why this matters: collapsing BBM into the free region (the previous draft) would mean the field layer could legally write PAGE_USED into BBM bytes on a non-default layout. Keeping them split now is what makes §1.2 default safe **and** what makes future per-vendor layouts safe without re-architecting.

## Non-goals

- No integration into `nand_init_device` yet (step 05).
- No scatter/gather yet (step 04).
- Per-vendor tables (different layouts) — deferred until step 12 or a future epic.

## Files to touch

| File | Action |
|------|--------|
| `src/nand_oob_layout_default.c` | **Create** — ops + const layout instance |
| [`CMakeLists.txt`](../../../CMakeLists.txt) | Append source **only if** `CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT` |

## Implementation checklist

1. Implement `free_region` enumerator returning **one** region `{offset=2, length=2, programmable=true, ecc_protected=true}`; second call returns `ESP_ERR_NOT_FOUND`.
2. Optionally stub `ecc_region` as `NULL` or return `ESP_ERR_NOT_FOUND` always.
3. Fill `spi_nand_oob_layout_t` const global `nand_oob_layout_default` (name flexible) with the BBM descriptor above and `ops` pointing at the static ops table.
4. **`oob_bytes` handling:** Acceptable patterns:
   - **Pattern A (preferred):** const layout holds `oob_bytes = 0` meaning "derive from chip at init". Step 05 reads `chip.emulated_page_oob` (or HW geometry) to fill a runtime cache.
   - **Pattern B:** runtime `spi_nand_oob_layout_runtime_t` copy on the handle (step 05).
   Pick **one** pattern, document it in this file's implementation notes, and stay consistent in step 05.
5. Expose accessor:

   ```c
   const spi_nand_oob_layout_t *nand_oob_layout_get_default(void);
   ```

   Guard declaration in header `priv_include/nand_oob_layout_default.h` or declare in `nand_oob_layout_types.h` footer — **private**.

6. Build only when Kconfig `y`.

## Testing

- Compile-only for step 03; optional **weak** unit test on host that calls `free_region` twice and asserts `{offset=2, length=2}` then `ESP_ERR_NOT_FOUND` — can merge into step 11 if size-constrained.

## Acceptance criteria

- [ ] With Kconfig **`n`**, no new object linked.
- [ ] With Kconfig **`y`**, `free_region(0)` returns exactly `{offset=2, length=2, programmable=true, ecc_protected=true}` and `free_region(1)` returns `ESP_ERR_NOT_FOUND`.
- [ ] BBM descriptor: `bbm_offset=0`, `bbm_length=2`, `good_pattern={0xFF,0xFF}`, `check_pages_mask=FIRST_PAGE` — matches current `nand_is_bad`.
- [ ] BBM bytes 0–1 are **not** enumerated by `free_region` (RFC contract).

## Risks

- Wrong `ecc_protected` flag for non-default chips bites later interleaved layouts — for the default this is safe because today's marker bytes are written under the chip's normal ECC policy alongside the data.

## Notes for implementers

- Plane selection **not** part of layout offsets — `nand_impl` continues to use `get_column_address()` (which folds the plane bit into the column based on **block index**) before applying layout-derived OOB offsets within the spare area.
- The "single region of length 2" choice is what makes the field-layer math trivial in step 04: PAGE_USED at logical_offset 0 inside the only region. No fragmentation arithmetic needed for the default.
