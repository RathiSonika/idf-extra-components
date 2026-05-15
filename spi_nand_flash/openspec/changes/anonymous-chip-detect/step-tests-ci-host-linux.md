# Tests — `test_app` CI, hardware evidence, `host_test` CRC + Linux parity

**PR identifier:** `anonymous-chip-tests`  
**Depends on:** [step-tier3-manual-and-public-api.md](step-tier3-manual-and-public-api.md)  
**Estimate:** ~300–850 LOC (configs, pytest, host tests, policy text)

## Goal

Satisfy proposal **§10** merge bar:

- **§10.A — CRC / pure helpers:** Host-native golden vectors for ONFI parameter page CRC (and any pure parse helpers); no SPI hardware required.
- **§10.B — CI matrix:** `sdkconfig.ci.anonymous` (or agreed name) with **`CONFIG_NAND_FLASH_ANONYMOUS_DETECT=y`** (and manual tier as needed for specific tests); pytest / CI references the preset; **default** preset remains anonymous **`n`** unless project explicitly chooses otherwise (§10 regression clause).
- **§10.C — Hardware evidence:** Document who runs ONFI-path smoke, commands, and what evidence attaches to PR/release (lab log, HW CI, signed checklist) — merge policy if default GitHub CI omits on-target.
- **§10.D — Linux:** On **`IDF_TARGET_LINUX`**, anonymous-related Kconfig does **not** alter baseline emulator init identity/geometry vs pre-feature behavior (proposal §8). Resolve dual interpretation with §8 if master cannot be `y` on linux (document in test comment).

## Scope

| Area | Action |
|------|--------|
| [`../../../test_app/`](../../../test_app/) | `sdkconfig.ci.anonymous`; extend [`../../../test_app/pytest_spi_nand_flash.py`](../../../test_app/pytest_spi_nand_flash.py) (or sibling) for matrix entry. |
| [`../../../host_test/`](../../../host_test/) | Golden vectors (C arrays or small blobs); CRC tests; optional `spi_nand_get_chip_source` assertion post-init if meaningful on Linux. |
| CMake under `host_test/` | Link CRC/helper sources per **Tier 2** linkage choice (always-built vs gated). |
| `test_app/README` or step-owned fragment | **§10.C** procedure text. |

## Out of scope

- Changing Linux synthetic chip IDs unless a bug is found — prefer **prove no drift** (baseline §4.7).
- Full component user guide (**Docs** milestone).

## Background

Baseline §8.1 (`sdkconfig.ci.default`, `sdkconfig.ci.bdl`); **Tier 2** CRC polynomial must match host vectors.

## Implementation checklist

### A — Host CRC + golden vectors (merged from legacy step 09, §10.A)

1. Commit 256-byte parameter page fixtures: valid, corrupted CRC, bad `ONFI` signature.
2. Call shared CRC routine; assert pass/fail edges.
3. Document CRC polynomial (match **Tier 2** header).

### B — Linux §10.D (merged from legacy step 09)

1. If Kconfig forbids `CONFIG_NAND_FLASH_ANONYMOUS_DETECT=y` on linux: test default linux build + init path unchanged; optionally assert ONFI objects not linked.
2. If build-only `y` on linux is allowed: assert emulator geometry / `spi_nand_get_chip_source` matches documented baseline — **lock** interpretation in test comment (cite proposal §8 vs §10.D).

### C — `test_app` CI + hardware procedure (merged from legacy step 08)

1. **Preset:** mirror `sdkconfig.ci.default` / `sdkconfig.ci.bdl` patterns; anonymous enabled for anonymous matrix.
2. **Pytest:** parametrized build selecting anonymous sdkconfig.
3. **On-target (feasible):** known database chip + `spi_nand_get_chip_source` ⇒ **DATABASE** when anonymous enabled.
4. **§10.C:** minimum doc — ONFI-capable part not in table (or ID masked), read/write/GC smoke, evidence expectation.

## Acceptance criteria

- [ ] **`sdkconfig.ci.anonymous`** exists and is referenced by pytest/CI (§10.B).
- [ ] Default / `sdkconfig.ci.default` matrix still passes with anonymous **`n`** (§10 regression).
- [ ] **§10.C** procedure written (README fragment, PR template, or `test_app` doc).
- [ ] Host tests run in CI / `pytest_nand_flash_linux.py` includes new cases where applicable (§10.A).
- [ ] §10.D covered by automated assertion or explicit skipped test with **normative** comment — **prefer** automated assertion for supported Kconfig combo.
- [ ] No new failures in default host matrix.

## Verification

```bash
cd test_app
idf.py set-target esp32 -DSDKCONFIG_DEFAULTS=sdkconfig.ci.anonymous && idf.py build
pytest ../../../test_app/pytest_spi_nand_flash.py -k <anonymous-matrix-selection>

cd ../host_test && idf.py --preview set-target linux && idf.py build
pytest ../host_test/pytest_nand_flash_linux.py -k <new-case>
```

(Adjust paths/options to match repo invocation.)

## Risks / notes

- **Flaky HW CI:** anonymous matrix may be build-only; §10.C then manual gate.
- **Watchdog:** long on-target tests — follow existing timeouts (baseline §9.3).

## References

- [`../../anonymous_chip_detect_proposal.md`](../../anonymous_chip_detect_proposal.md) §8, §10, §5.1.
- [`../../baseline.md`](../../baseline.md) §4.7, §8.1, §9.3.
