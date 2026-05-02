# Step 11 — Tests and CI Kconfig matrix

**PR identifier:** `oob-layout-11`  
**Depends on:** steps **01–10**  
**Estimate:** ~200–650 LOC (configs + a few tests)

## Goal

Cover **both** on-target and host surfaces from baseline §8.1 / §8.2:

1. **`test_app/` (on-target) — mandatory gate:** The feature is **not** complete until **`test_app`** passes the **full** on-target Unity/pytest suite under the experimental OOB Kconfig (**`y`**), on real SPI NAND hardware — **same tests** as the corresponding baseline preset (legacy or BDL), not a reduced subset. Build-only checks are **insufficient** for this step’s acceptance criteria.

2. **`host_test/` (linux)** — mmap emulator; mandatory **full** host test run with **`sdkconfig.ci.oob_layout`** (`y`).

3. **Both Kconfig values** maintained in CI:
   - **`n`** — existing pipelines (**unchanged**): full suite continues to pass.
   - **`y`** — additional preset(s) below; **full suite** must pass for each preset that mirrors an existing CI mode (legacy + BDL where applicable).

## Non-goals

- Full perf benchmark suite (proposal V8 manual).

## `test_app/` (mandatory — full pass on target)

Per baseline §8.1, on-target tests live under [`test_app/`](../../../test_app/) with [`test_app/pytest_spi_nand_flash.py`](../../../test_app/pytest_spi_nand_flash.py). **`test_app` is not optional:** implementations must be validated so **every** test case that runs for the baseline sdkconfig still passes when experimental OOB is enabled (for that same mode: legacy vs BDL).

| Deliverable | Details |
|-------------|--------|
| **`test_app/sdkconfig.ci.oob_layout`** | **New.** Base = **legacy** CI config (same as today’s non-BDL job: baseline references `sdkconfig.ci.default` — use whatever file or `sdkconfig.defaults` chain the legacy pytest job actually uses). Set **`CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT=y`** with minimal other diff. |
| **`test_app/sdkconfig.ci.bdl_oob_layout`** | **New (mandatory)** because the repo already ships **BDL** CI ([`sdkconfig.ci.bdl`](../../../test_app/sdkconfig.ci.bdl)): duplicate that preset and add **`CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT=y`**. The **full** BDL pytest suite must pass on target with this preset. |
| **`pytest_spi_nand_flash.py`** | Extend CI matrix so **both** presets above are built, flashed, and run through the **entire** pytest suite on hardware runners — **same invocations** as default/BDL jobs, only sdkconfig path changes. If the project’s CI has **no** attached hardware, maintainers must still **document and enforce** a manual gate: full `pytest_spi_nand_flash.py` pass on a reference board for **both** OOB presets before merging step 11 / releasing the feature toggle. |
| **`test_app/README.md`** | Document exact commands to reproduce CI locally for **`oob_layout`** and **`bdl_oob_layout`**. |
| **New Unity tests** | Only if gaps found; default expectation is existing tests suffice once behavior matches §1.2 under default layout. |

## `host_test/` (required)

| File | Action |
|------|--------|
| [`host_test/sdkconfig.ci.oob_layout`](../../../host_test/sdkconfig.ci.oob_layout) | **New** — `CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT=y` |
| [`host_test/README.md`](../../../host_test/README.md) | Document local run with OOB preset |
| `host_test/main/test_nand_oob_layout.cpp` | **Optional** unit tests (scatter/gather round-trip) |

## CI orchestration

| File | Action |
|------|--------|
| [`host_test/pytest_nand_flash_linux.py`](../../../host_test/pytest_nand_flash_linux.py) | Matrix / extra invocation with **`sdkconfig.ci.oob_layout`** — **full** suite |
| [`test_app/pytest_spi_nand_flash.py`](../../../test_app/pytest_spi_nand_flash.py) | Matrix: **full** suite with `sdkconfig.ci.oob_layout` + `sdkconfig.ci.bdl_oob_layout` |

Exact mechanics depend on monorepo CI — preserve **default** jobs as today.

## Implementation checklist

1. Add **`test_app/sdkconfig.ci.oob_layout`** (legacy + experimental OOB **`y`**).
2. Add **`test_app/sdkconfig.ci.bdl_oob_layout`** (BDL + experimental OOB **`y`**).
3. Add **`host_test/sdkconfig.ci.oob_layout`**.
4. Wire **Linux** CI: **full** `pytest_nand_flash_linux.py` (or equivalent) with host **`sdkconfig.ci.oob_layout`** — all tests green.
5. Wire **target** CI: **full** `pytest_spi_nand_flash.py` for **`sdkconfig.ci.oob_layout`** and **`sdkconfig.ci.bdl_oob_layout`** — all tests green on hardware. If automation is impossible, file a **tracked manual verification** checklist signed before merge (not a substitute long-term).
6. If scatter/gather unit tests added on host: assert PAGE_USED round-trip at logical offset **0** length **2** for default layout (single free region {offset=2, length=2} per step 03; logical 0 ⇄ physical OOB byte 2). Also assert that scatter at `logical_off=0, len > 2` returns `ESP_ERR_INVALID_SIZE` and does **not** spill into BBM bytes 0–1.

## Acceptance criteria

- [ ] CI green for **existing** presets (`CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT=n` implied) — **full** suites unchanged.
- [ ] **`host_test`**: **full** test run passes with **`sdkconfig.ci.oob_layout`** (`y`).
- [ ] **`test_app`**: **full** `pytest_spi_nand_flash.py` passes on target with **`sdkconfig.ci.oob_layout`** (`y`) — legacy mode parity.
- [ ] **`test_app`**: **full** pytest passes on target with **`sdkconfig.ci.bdl_oob_layout`** (`y`) — BDL mode parity.
- [ ] No merge of step 11 without evidence of the above (CI logs or documented manual full runs).
- [ ] Proposal validation **V8–V9** satisfied for suite coverage; perf spot-check still manual if desired.

## Risks

- CI duration — schedule **`y`** matrix on nightly if PR budget is tight; **do not** drop **full** pytest for **`test_app`** without maintainer sign-off.
- Missing `sdkconfig.ci.default` in tree — use actual legacy CI source of truth from CI logs or `sdkconfig.defaults`.

## Notes for implementers

- **`test_app` full pass on target** is a **hard requirement** for this feature rollout; **`host_test`** alone is not enough (proposal §6 V1, V9).
