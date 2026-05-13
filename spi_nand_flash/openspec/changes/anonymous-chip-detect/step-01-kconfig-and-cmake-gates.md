# Step 01 — Kconfig and CMake gates (no runtime change)

**PR identifier:** `anonymous-chip-01`  
**Depends on:** none  
**Estimate:** small (~40–120 LOC)

## Goal

Add the **two-level** Kconfig switches from proposal §6.1 and wire **CMake** so anonymous-specific sources can be compiled only when enabled. With **defaults unchanged** (`n` / `n`), there must be **zero** change to chip detection, init return codes, or SPI traffic.

## Scope

| Area | Action |
|------|--------|
| [`../../../Kconfig`](../../../Kconfig) | Add `CONFIG_NAND_FLASH_ANONYMOUS_DETECT` (master) and `CONFIG_NAND_FLASH_ANONYMOUS_MANUAL` (depends on master) |
| [`../../../CMakeLists.txt`](../../../CMakeLists.txt) | Conditional `SRCS` / compile definitions so new `.c` files added in later steps are excluded when master is `n` |

## Out of scope

- Any change to `nand_init_device`, `spi_nand_oper`, vendor `devices/*.c`, `nand.c` public behavior.
- Public headers, `priv_include` types (step **02**).
- Tests beyond “still builds” (full matrix in steps **08–09**).

## Background

Proposal §5.1 (Tier 1 invariant when master `n`), §6.1 (two-level gates), §8 (Linux: master should not enable real SPI probe on `IDF_TARGET_LINUX` unless a host story is defined — **v1:** keep anonymous SPI paths **off** for `linux` target via `depends on !IDF_TARGET_LINUX` or equivalent so default host builds match baseline §4.7).

## Implementation checklist

1. **`CONFIG_NAND_FLASH_ANONYMOUS_DETECT`**
   - `bool`, default **`n`**.
   - Help: master gate for Tier 2 (ONFI) + shared plumbing; when `n`, behavior matches pre-feature baseline (cite proposal §5.1, §6.1).
   - `depends on`: **non-Linux** for the SPI probe story (proposal §6.1 table, §8); align with existing `Kconfig` style (`IDF_TARGET_LINUX` negation).

2. **`CONFIG_NAND_FLASH_ANONYMOUS_MANUAL`**
   - `bool`, default **`n`**.
   - `depends on CONFIG_NAND_FLASH_ANONYMOUS_DETECT` (exact Kconfig symbol name as generated from the prompt).
   - Help: Tier 3 only; user-supplied geometry/delays from menuconfig; not a “works on random NAND” mode (proposal §6.1, §5.3).

3. **CMake**
   - Guard any **new** object files for steps 03+ with `if(CONFIG_NAND_FLASH_ANONYMOUS_DETECT)` (or generator expression consistent with repo patterns).
   - Step 01 may only add **empty** stubs **if** required to satisfy linking — **prefer** no new `.c` files in this PR; only Kconfig + CMake scaffolding.

4. **Build smoke**
   - Default sdkconfig: `idf.py set-target esp32` (or project default) + `idf.py build` from [`../../../test_app`](../../../test_app).
   - `idf.py --preview set-target linux` + `idf.py build` from [`../../../host_test`](../../../host_test).

## Acceptance criteria

- [ ] With both options **`n`** (default): no new SPI sequences, no change to init **`esp_err_t`** semantics vs current `master` for the same hardware/config (proposal §5.1).
- [ ] Flipping master to **`y`** builds (may require later steps’ sources — if so, step 01 CMake must not reference missing files; **either** land minimal stub **or** defer CMake `SRCS` append until step 03 — choose one and document in PR).
- [ ] Manual option is **invisible or forced off** when master is `n` (Kconfig `depends on`).
- [ ] Linux host target builds with default config; anonymous master remains unavailable or off per §8.

## Verification

```bash
cd test_app && idf.py set-target esp32 && idf.py build
cd ../host_test && idf.py --preview set-target linux && idf.py build
```

(Optional: `idf.py menuconfig` — confirm prompts and dependencies.)

## Risks / notes

- **Mis-scoped `depends on`:** Accidentally hiding the symbol on all targets breaks CI; verify against `sdkconfig.defaults` in test apps.
- Do **not** change `sdkconfig.ci.default` / `sdkconfig.ci.bdl` in this step unless required for symbol visibility (prefer step **08**).

## References

- [`../../anonymous_chip_detect_proposal.md`](../../anonymous_chip_detect_proposal.md) §5.1, §6.1, §8.
