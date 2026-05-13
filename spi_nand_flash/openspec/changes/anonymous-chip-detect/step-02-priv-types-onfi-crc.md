# Step 02 — Private ONFI types and CRC helper (headers + optional pure C)

**PR identifier:** `anonymous-chip-02`  
**Depends on:** step **01**  
**Estimate:** ~120–350 LOC

## Goal

Introduce **`priv_include/`**-only definitions for the ONFI parameter page layout, signature/CRC constants, and a **pure** CRC validation function usable from **host tests** (proposal §5.2 integrity, §7 `nand_parameter_page_t` visibility, §10.A).

## Scope

| Area | Action |
|------|--------|
| `priv_include/` | New header(s), e.g. `nand_onfi_param_page.h` (name negotiable): packed `struct` mirroring ONFI parameter page fields needed for v1 (minimum set proposal §5.2 table). |
| `src/` (optional) | Small `nand_onfi_crc.c` **or** `static inline` in header if acceptable to host test linkage — prefer one compile unit if it keeps `host_test` linking simple. |

## Out of scope

- SPI transactions, `spi_nand_oper` calls, `nand_init_device` changes.
- Publishing any packed layout type in `include/` (proposal §7).
- Parameter page **read** implementation (step **03**).
- Parsing into `nand_flash_geometry_t` (step **04**).

## Background

Proposal §5.2: bytes **0–3** must equal ASCII **`ONFI`**; CRC over documented range (verify polynomial/init against ONFI 1.0 / vendor errata in implementation — step doc requires **documenting** the chosen polynomial in code comment or README fragment).

## Implementation checklist

1. Define **`nand_parameter_page_t`** (or equivalent name) with:
   - Correct packing / endianness documentation for fields actually read in step **04** (`data_bytes_per_page`, `pages_per_block`, `blocks_per_lun`, `num_luns`, timing fields, `ecc_correctability`, manufacturer/model strings as needed).
   - Comments cite proposal §5.2 field table, not external spec URLs (per component comment rules).

2. **`bool` / `esp_err_t` CRC API** (choose one style consistent with repo):
   - Input: 256-byte buffer; validate bytes **0–253** vs CRC in **254–255** per ONFI convention assumed in proposal §5.2.
   - Return values usable from unit tests (clear invalid vs valid).

3. **Include guards** and **no** inclusion from `include/spi_nand_flash.h`.

4. If a `.c` file is added: append to CMake **only** when `CONFIG_NAND_FLASH_ANONYMOUS_DETECT=y` **or** compile CRC unconditionally behind a neutral name — **prefer** compiling CRC helper whenever `host_test` needs it without SPI: see step **09** for linking story; step 02 may use a **always-built** tiny file if footprint is negligible **or** gate with `CONFIG_NAND_FLASH_ANONYMOUS_DETECT` and mirror tests — **lock choice in PR** so step 09 does not guess.

## Acceptance criteria

- [ ] No public API changes under `include/`.
- [ ] `nand_parameter_page_t` (or chosen name) lives only under `priv_include/` (proposal §7).
- [ ] CRC function has a **deterministic** contract documented in the header (length, endianness of CRC field).
- [ ] Target + Linux **default** builds still succeed (step 01 gates; no init hooks yet).

## Verification

```bash
cd test_app && idf.py set-target esp32 && idf.py build
cd ../host_test && idf.py --preview set-target linux && idf.py build
```

(Host unit tests for CRC land in step **09**; step 02 may add a **temporary** `TEST_CASE` only if step 09 is far behind — prefer deferring tests to step **09**.)

## Risks / notes

- **Endianness:** ONFI multi-byte integers are little-endian per ONFI; document field access.
- **Host vs target:** If CRC lives in a gated `.c`, `host_test` must still link it for §10.A — coordinate with step **09** CMake.

## References

- [`../../anonymous_chip_detect_proposal.md`](../../anonymous_chip_detect_proposal.md) §5.2, §5.2a, §7, §10.A, §12 (POC gap: CRC gate).
