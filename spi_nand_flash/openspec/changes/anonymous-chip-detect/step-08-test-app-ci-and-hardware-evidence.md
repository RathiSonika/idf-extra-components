# Step 08 — `test_app`: CI preset, pytest matrix, hardware evidence (§10.B/C)

**PR identifier:** `anonymous-chip-08`  
**Depends on:** step **07**  
**Estimate:** ~150–450 LOC (plus policy text)

## Goal

Satisfy proposal **§10.B** (preset + CI matrix) and **§10.C** (hardware validation ownership / evidence) for the anonymous feature.

## Scope

| Area | Action |
|------|--------|
| [`../../../test_app/`](../../../test_app/) | Add `sdkconfig.ci.anonymous` (or agreed name) with **`CONFIG_NAND_FLASH_ANONYMOUS_DETECT=y`** and manual tier as needed for specific tests |
| Pytest | Extend [`../../../test_app/pytest_spi_nand_flash.py`](../../../test_app/pytest_spi_nand_flash.py) (or sibling) to build/flash matrix entry |
| Docs in step or `test_app/README` | **§10.C procedure:** who runs HW smoke, what commands, what evidence is attached to release/PR (lab log, HW CI job, or signed checklist) — proposal §10.C |

## Out of scope

- Host-side CRC unit tests (step **09**).
- Component root `README.md` full user guide (step **10**).

## Background

Proposal §10.B–C; baseline §8.1 (existing `sdkconfig.ci.default`, `sdkconfig.ci.bdl`).

## Implementation checklist

1. **Preset:** mirrors patterns from `sdkconfig.ci.default` / `sdkconfig.ci.bdl` with anonymous options enabled; ensure **default** CI preset remains anonymous **`n`** unless project chooses otherwise (proposal §10 regression clause).

2. **Pytest:** new marker or parametrized build that selects the anonymous sdkconfig for on-target build.

3. **On-target tests (feasible):**
   - Known database chip: assert `spi_nand_get_chip_source` ⇒ **DATABASE** when anonymous enabled (no functional regression).
   - If no ONFI-less part in CI farm, document **manual** HW checklist for §10.C instead of automated assertion.

4. **§10.C hardware evidence:** Document minimum: ONFI-capable part **not** in table (or ID masked), read/write/GC smoke, link to log artifact expectation.

## Acceptance criteria

- [ ] **`sdkconfig.ci.anonymous`** exists and is referenced by pytest/CI (proposal §10.B).
- [ ] Default/`sdkconfig.ci.default` matrix still passes with anonymous **`n`** (proposal §10 regression).
- [ ] **§10.C** procedure is written down (README fragment, PR template note, or `test_app` doc) — merge policy per repo: if HW CI absent, **explicit** “evidence attached by maintainer” rule (proposal §10.C).

## Verification

```bash
cd test_app
idf.py set-target esp32 -DSDKCONFIG_DEFAULTS=sdkconfig.ci.anonymous && idf.py build
pytest ../../../test_app/pytest_spi_nand_flash.py -k <anonymous-matrix-selection>
```

(Adjust paths/options to match repo’s pytest invocation.)

## Risks / notes

- **Flaky HW CI:** matrix may build-only for anonymous preset if farm lacks part; still satisfy §10.B “built in CI”; §10.C then manual gate.
- **Watchdog:** long tests on real NAND — follow existing test timeouts (baseline §9.3).

## References

- [`../../anonymous_chip_detect_proposal.md`](../../anonymous_chip_detect_proposal.md) §10.B–C, §5.1.
- [`../../baseline.md`](../../baseline.md) §8.1.
