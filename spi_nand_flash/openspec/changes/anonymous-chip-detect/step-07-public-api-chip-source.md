# Step 07 — Public API: `spi_nand_chip_source_t` and `spi_nand_get_chip_source`

**PR identifier:** `anonymous-chip-07`  
**Depends on:** step **06**  
**Estimate:** ~80–200 LOC

## Goal

Add the **stable** public surface from proposal §7:

- `typedef enum { SPI_NAND_CHIP_SOURCE_DATABASE, SPI_NAND_CHIP_SOURCE_ONFI, SPI_NAND_CHIP_SOURCE_MANUAL } spi_nand_chip_source_t;` (names may add `SPI_NAND_` prefix for consistency — **do not** publish `SPI_NAND_CHIP_FLAG_ANONYMOUS` via `include/`).

- `esp_err_t spi_nand_get_chip_source(spi_nand_flash_device_t *handle, spi_nand_chip_source_t *out);`

Implement in [`../../../src/nand.c`](../../../src/nand.c) with mutex rules consistent with other getters (baseline §9.1 — use same locking pattern as `spi_nand_flash_get_page_size` etc.).

## Scope

| File | Action |
|------|--------|
| [`../../../include/spi_nand_flash.h`](../../../include/spi_nand_flash.h) | Enum + declaration + Doxygen-style brief if repo uses it |
| [`../../../src/nand.c`](../../../src/nand.c) | Implementation |

## Out of scope

- Exposing `nand_parameter_page_t` or CRC helpers (proposal §7).
- Changing `nand_flash_geometry_t::flags` for anonymous (proposal §7).

## Background

Baseline §4.0: symbols under `include/` are **stable**; choose enum values explicitly (fixed integers) if ABI stability is a concern, or document that enum order is API.

## Implementation checklist

1. **NULL checks:** return `ESP_ERR_INVALID_ARG` for NULL `handle` / `out`.

2. **Init ordering:** If called before init completes, return `ESP_ERR_INVALID_STATE` (or document “only valid after successful init” — match repo patterns).

3. Map internal private enum to public enum in one place to avoid drift.

4. **BDL mode:** If legacy `spi_nand_flash_device_t*` is unreachable in BDL-only apps, still document behavior for tests that hold a handle (baseline §4.2).

## Acceptance criteria

- [ ] Public header compiles from C and C++ consumers if the repo already guarantees that for `spi_nand_flash.h`.
- [ ] Getter returns last successful tier source after init (proposal §7).
- [ ] No leak of internal-only flags through geometry bitmask (proposal §7).

## Verification

```bash
cd test_app && idf.py set-target esp32 && idf.py build
cd ../host_test && idf.py --preview set-target linux && idf.py build
```

(Add assertion in host test in step **09** if not done here.)

## Risks / notes

- **Stable enum:** Adding values in the future should not reorder existing ones; reserve or pad comment.

## References

- [`../../anonymous_chip_detect_proposal.md`](../../anonymous_chip_detect_proposal.md) §7.
- [`../../baseline.md`](../../baseline.md) §4.0, §9.1.
