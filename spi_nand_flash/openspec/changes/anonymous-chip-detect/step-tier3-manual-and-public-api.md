# Tier 3 — Manual fallback + public provenance API

**PR identifier:** `anonymous-chip-tier3`  
**Depends on:** [step-tier2-onfi.md](step-tier2-onfi.md)  
**Estimate:** ~280–700 LOC

## Goal

1. **Tier 3 — Manual:** When **`CONFIG_NAND_FLASH_ANONYMOUS_MANUAL=y`** (requires master **`y`**), read user-supplied page geometry and delays from Kconfig; validate; on invalid combination return **`ESP_ERR_INVALID_ARG`** with clear log (proposal §5.3). Success sets **`chip_source = MANUAL`**, internal **`SPI_NAND_CHIP_FLAG_ANONYMOUS`**, **`ESP_LOGW`** (§9), **SIO-only** (§5.6). Wire **after Tier 2 fails** (§5.1).
2. **Public API (proposal §7):** `spi_nand_chip_source_t` + **`spi_nand_get_chip_source`** in [`../../../include/spi_nand_flash.h`](../../../include/spi_nand_flash.h); implement in [`../../../src/nand.c`](../../../src/nand.c) with mutex rules matching other getters (baseline §9.1). **Do not** publish `SPI_NAND_CHIP_FLAG_ANONYMOUS` or `nand_parameter_page_t` via `include/`.

## Scope

| Area | Action |
|------|--------|
| [`../../../Kconfig`](../../../Kconfig) | Manual geometry + delay prompts (prefix e.g. `NAND_FLASH_ANONYMOUS_MANUAL_*` per §6 naming); symbols beyond **Tier 1** placeholders if any were deferred. |
| `src/nand_impl.c` (or `src/nand_anonymous_manual.c`) | Tier 3 branch after Tier 2 failure; shared minimal register setup with Tier 2 where practical (§5.5). |
| [`../../../include/spi_nand_flash.h`](../../../include/spi_nand_flash.h) | `typedef enum { … DATABASE, … ONFI, … MANUAL } spi_nand_chip_source_t;` + `spi_nand_get_chip_source` declaration + brief docs if repo style requires. |
| [`../../../src/nand.c`](../../../src/nand.c) | Getter implementation; map internal enum to public enum in one place. |

## Out of scope

- ONFI read/parse changes (**Tier 2**).
- **`sdkconfig.ci.*`** / pytest / host tests (**Tests** milestone).
- Component root README / CHANGELOG (**Docs** milestone).
- Spare-size / per-region OOB Kconfig (§5.4 — no spare-size Kconfig v1).

## Background

Proposal §6.1–§6.2 (manual opt-in, help text); §7 (stable public surface); baseline §4.0 (`include/` stability).

## Implementation checklist

### Manual tier (merged from legacy step 06)

1. Kconfig: `page_size` (or `log2`), `pages_per_block`, `num_blocks`, `t_r`, `t_prog`, `t_bers` — align names with `nand_flash_geometry_t`.
2. Validation mirrors vendor inits (power-of-two where required).
3. Tier 3 only when master + manual both **`y`** and Tier 2 failed.
4. **QE:** do not enable; **`ESP_LOGW`** if QOUT/QIO (§5.6).
5. Help text: values must match datasheet; optional “reject placeholder defaults” — document rule in Kconfig help if implemented (§6.2).

### Public API (merged from legacy step 07)

1. **NULL checks:** `ESP_ERR_INVALID_ARG` for NULL `handle` / `out`.
2. **Init ordering:** match repo pattern if called before init completes (`ESP_ERR_INVALID_STATE` or documented).
3. **Stable enum:** fixed integer values or documented ordering for future extension (baseline §4.0).
4. **BDL / tests:** document behavior for handles after successful init (baseline §4.2).

## Acceptance criteria

- [ ] **`CONFIG_NAND_FLASH_ANONYMOUS_MANUAL=n`:** no Tier 3 code path (§6.1).
- [ ] Invalid Kconfig combo ⇒ **`ESP_ERR_INVALID_ARG`**, no partial handle leak (§5.3).
- [ ] Tier 3 success ⇒ internal anonymous + manual provenance (§7).
- [ ] Anonymous on + all tiers exhausted ⇒ **`ESP_ERR_NOT_FOUND`** (§5.1).
- [ ] Public header compiles from C/C++ if repo already guarantees that for `spi_nand_flash.h`.
- [ ] Getter returns provenance after successful init (§7).
- [ ] No leak of internal-only flags through `nand_flash_geometry_t::flags` (§7).

## Verification

```bash
cd test_app && idf.py set-target esp32 && idf.py build
cd ../host_test && idf.py --preview set-target linux && idf.py build
# menuconfig: master + manual + legal geometry — boot test if hardware available (optional)
```

## Risks / notes

- **Dangerous defaults:** placeholders must not imply “works everywhere” (§6.2); consider defaults that fail validation until user edits.

## References

- [`../../anonymous_chip_detect_proposal.md`](../../anonymous_chip_detect_proposal.md) §5.1, §5.3–§5.6, §6.1–§6.2, §7, §9.
- [`../../baseline.md`](../../baseline.md) §4.0, §4.2, §9.1.
