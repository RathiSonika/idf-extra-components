# Step 02 — Private types header (`priv_include`)

**PR identifier:** `oob-layout-02`  
**Depends on:** step **01**  
**Estimate:** ~150–350 LOC (mostly typedefs + comments)

## Goal

Add **`priv_include/nand_oob_layout_types.h`** (exact filename negotiable, but **must stay private**) defining stable **internal** shapes for:

- OOB region descriptor (physical offset within spare area, length, flags).
- Layout ops (enumerate free programmable regions / optional ECC parity regions for tooling).
- BBM descriptor (offset within OOB, length, good pattern bytes, which pages to check — bitmask).
- Field ID enum + field spec struct (logical slot for PAGE_USED, etc.).
- Xfer context struct **declaration only** (implementation in step 04): cached regions, counts, `oob_raw` pointer, totals.

Use **`esp_err_t`** and ESP-IDF conventions — **not** Linux `-ERANGE` in public-facing internal APIs (proposal §3 A5). For “no more regions” use `ESP_ERR_NOT_FOUND` or a dedicated `bool`/out-parameter pattern documented in this header.

## Non-goals

- No `.c` files required (optional: compile-test unit not needed).
- No inclusion from `include/` (public API).
- Do **not** attach anything to `spi_nand_flash_device_t` yet (step 05).

## Background

Proposal §2.3 diagram lists types aligned with the RFC. This step **locks names and semantics** so later PRs do not diverge.

## Types to define (minimal)

Implementer may adjust names slightly but must preserve meaning:

1. **`spi_nand_oob_region_desc_t`**  
   - `uint16_t offset` — byte offset **within the OOB area** (0 = first spare byte at column `page_size`).  
   - `uint16_t length`  
   - `bool programmable`  
   - `bool ecc_protected` (meaning: covered by device internal ECC policy for metadata writes — match proposal/default chip behavior)

2. **`spi_nand_ooblayout_ops_t`**  
   - `esp_err_t (*free_region)(const void *chip_ctx, int section, spi_nand_oob_region_desc_t *out);`  
     - `section` 0,1,2…; return `ESP_ERR_NOT_FOUND` (or documented sentinel) when exhausted.  
   - Optional: `ecc_region` callback for parity enumeration (may be `NULL` until needed).

3. **BBM**

   - `spi_nand_bbm_check_pages_t` — bitmask enum: first page of block, last page, etc. (match RFC intent; document mapping to ONFI-style checks in comments).
   - Nested struct inside **`spi_nand_oob_layout_t`**: `bbm_offset`, `bbm_length`, `good_byte0`, `good_byte1` (or array `good_pattern[2]`), `check_pages_mask`.

4. **`spi_nand_oob_layout_t`**  
   - `uint8_t oob_bytes` — spare size per page (may duplicate geometry info later; document relationship to `chip->page_size` / datasheets).  
   - BBM struct above.  
   - `const spi_nand_ooblayout_ops_t *ops`.

5. **Fields** (naming for **init-time** assignment — **not** a per-I/O dispatch layer)

   - `typedef enum { SPI_NAND_OOB_FIELD_PAGE_USED = 0, … } spi_nand_oob_field_id_t;`
   - `typedef enum { SPI_NAND_OOB_CLASS_FREE_ECC, SPI_NAND_OOB_CLASS_FREE_NOECC } spi_nand_oob_class_t;` (names flexible)
   - `spi_nand_oob_field_spec_t`: `id`, `length`, `class`, `logical_offset` (filled at init), `assigned` bool.

   **Runtime:** steps 06–09 use **cached logical/physical offsets** + scatter/gather; Dhara does **not** pass field IDs. The enum exists so one init table documents “PAGE_USED” without magic numbers — see root [`README.md`](README.md) *Field IDs vs hot path*.

6. **`spi_nand_oob_xfer_ctx_t`**  
   - Forward-declare or full struct with:
     - `const spi_nand_oob_layout_t *layout`
     - `spi_nand_oob_class_t cls`
     - `uint8_t *oob_raw`, `uint16_t oob_size`
     - `spi_nand_oob_region_desc_t regs[SPI_NAND_OOB_MAX_REGIONS]`
     - `uint8_t reg_count`
     - `size_t total_logical_len` (sum of lengths of cached regs for bounds checks)

7. **`SPI_NAND_OOB_MAX_REGIONS`** (same cap as RFC **`MAX_REG`**)  
   - **`8`** — locked for this implementation (see root [`README.md`](README.md) *Implementation decisions*). `spi_nand_oob_xfer_ctx_t::regs[]` uses this dimension. Revisit only if a datasheet-backed layout needs more than eight disjoint free fragments.

## Files to touch

| File | Action |
|------|--------|
| `priv_include/nand_oob_layout_types.h` | **Create** |
| `priv_include/nand_impl.h` or `priv_include/nand.h` | **Optional:** forward-include only if needed for compile; prefer **no** until step 05 |

## Implementation checklist

1. Create header with SPDX header matching sibling private headers.
2. Wrap entire file or sensitive sections in `#ifdef CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT` **or** always define types but use only when config on — **prefer always define types** to simplify refactoring, as long as no static globals; alternatively ifdef-guard to avoid unused warnings on `n` builds (choose one strategy and document).
3. Add Doxygen-style comments: offset interpretation (**within OOB**), plane handling note (“column address = page_size + offset” composed in impl, not in layout ops”).
4. No linkage changes if header-only.

## Acceptance criteria

- [ ] Component builds for **esp32** + **linux** with step 01 Kconfig **`n`** and **`y`**.
- [ ] No symbols exported from `include/`.
- [ ] BBM and region semantics documented in-header so step 03–08 do not reinterpret offsets.

## Risks

- Ambiguous **offset base** (main array vs OOB-only) — comments must be explicit to avoid off-by-`page_size` bugs.

## Notes for implementers

- Use `static_assert` (C11) or compile-time checks only if IDF toolchain allows in this component; otherwise skip.
