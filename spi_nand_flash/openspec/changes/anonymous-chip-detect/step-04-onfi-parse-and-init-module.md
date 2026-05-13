# Step 04 — ONFI parse, validation, and chip setup (Tier 2 module)

**PR identifier:** `anonymous-chip-04`  
**Depends on:** step **03**  
**Estimate:** ~350–650 LOC

## Goal

From a **256-byte** parameter page buffer: verify **`ONFI`** signature (bytes **0–3**), run **CRC** (step **02**), enforce **`num_luns == 1`**, extract fields per proposal §5.2 table, and populate **`nand_flash_geometry_t`**, delays, and **`nand_ecc_data_t`** consistently with baseline vendor-less expectations (§5.5). Set **internal** state: **`SPI_NAND_CHIP_FLAG_ANONYMOUS`** and driver-private **`chip_source == ONFI`** for the in-flight init context (proposal §5.6, §7).

## Scope

| Area | Action |
|------|--------|
| `src/nand_onfi.c` (example name) | `nand_onfi_try_init(...)` or similar: input `spi_nand_flash_device_t*` partial + SPI ops, output filled geometry / chip flags |
| `priv_include/nand.h` (or device struct header) | Fields for `chip_source` (enum in **private** header) and anonymous flag storage if not already present |

## Out of scope

- Calling this function from `nand_init_device` (step **05**).
- Public `spi_nand_get_chip_source` (step **07**).
- **`spare_bytes_per_page`** driving geometry or `oob_size` (proposal §5.2 table row “ignore”; optional `ESP_LOGI` dump).
- QE / quad enable (proposal §5.6: **force SIO** for anonymous ONFI success path inside this module or document that upper layer downgrades before SPI traffic — **single ownership** for downgrade: implement here or in step **05**, not both conflicting).

## Background

Proposal §5.2a (multi-LUN reject); §5.4–§5.5 (markers at `page_size`, no new `oob_size`); §5.6 (SIO, warn on QOUT/QIO config — warn may fire in step **05** when `io_mode` is visible; at minimum this step must **not** set QE bits).

## Implementation checklist

1. Reject parameter page if signature or CRC fails; return **`ESP_ERR_NOT_FOUND`** or **`ESP_ERR_INVALID_CRC`** / existing baseline-consistent code — **document** mapping; final aggregate failure in step **05** still normalizes to **`ESP_ERR_NOT_FOUND`** when anonymous is on (proposal §5.1).

2. Map `data_bytes_per_page` → `log2_page_size` / `page_size` with validation (powers of two supported by existing `nand_impl`).

3. `pages_per_block`, `blocks_per_lun` → block geometry; **`num_blocks`** when `num_luns == 1` (proposal §5.2).

4. Timing: `t_r`, `t_prog`, `t_bers` → `read_page_delay_us`, `program_page_delay_us`, `erase_block_delay_us` (names per `nand_flash_geometry_t` — match existing vendor modules).

5. `ecc_correctability`: log and/or set conservative `nand_ecc_data_t` fields per proposal §5.5 (no new ECC layout engine).

6. **Device info strings:** fill manufacturer/model from parameter page where ASCII fields exist (proposal §5.2 note on strings).

## Acceptance criteria

- [ ] **`num_luns != 1`** ⇒ function returns error without mutating chip to “success” (proposal §5.2a).
- [ ] Does **not** enable quad / QE for anonymous path (proposal §5.6).
- [ ] Does **not** add `oob_size` to public geometry type.
- [ ] Tier 2 success marks internal anonymous flag and records ONFI provenance for later `spi_nand_get_chip_source` (proposal §7).

## Verification

- Build: `test_app` with `CONFIG_NAND_FLASH_ANONYMOUS_DETECT=y` (module linked; not yet called from init if step 05 separate — if step 04 lands without caller, use `__attribute__((unused))` static wrapper or temporary unit test in step **09** only; **prefer** landing step **04**+**05** in close succession to avoid dead code warnings).

## Risks / notes

- **Incorrect ECC defaults** can cause silent read reliability issues; align defaults with simplest vendor init for similar geometry or document assumption (proposal §5.5).
- Parser must tolerate reserved fields without over-reading buffer.

## References

- [`../../anonymous_chip_detect_proposal.md`](../../anonymous_chip_detect_proposal.md) §5.2, §5.2a, §5.4–§5.6, §7, §12.
