# Handoff prompt — configurable OOB layout (copy into a new session)

Use **one session per step**. Replace **`NN`** below with `01` … `12` (leading zero).

---

## Paste this into the new chat/session

```
You are implementing ONE incremental PR for the spi_nand_flash component.

REPOSITORY CONTEXT: This component lives in the **idf-extra-components** project (it is **not** inside the ESP-IDF repository). The **git root** is typically `idf-extra-components/`; the **component under work** is the `spi_nand_flash` directory (sibling to other extra components). Example: `.../idf-extra-components/spi_nand_flash/`. Use `test_app/` and `host_test/` under that directory with ESP-IDF as the **toolchain/SDK** (build from those app folders with `idf.py`), not as if spi_nand_flash were `$IDF_PATH/components/...`.

STEP TO IMPLEMENT: NN only (do not implement earlier or later steps).

MANDATORY READ ORDER (open and follow in full before coding):
1. openspec/configurable_oob_layout_proposal.md — product intent, §1.2 current OOB bytes, Kconfig policy, constraints (Dhara unchanged, default layout = legacy behavior).
2. openspec/changes/configurable-oob-layout/README.md — global PR rules (≤500–700 LOC per PR, Linux parity rule, test_app mandatory at step 11).
3. openspec/changes/configurable-oob-layout/step-NN-*.md — sole source of truth for THIS PR’s scope, files, checklist, acceptance criteria.

ABSOLUTE RULES:
- Implement ONLY step NN. If something obviously depends on a prior step not merged, stop and say so — do not invent scope.
- Do NOT modify the Dhara library/vendor component; only spi_nand_flash sources + test_app + host_test as the step doc allows.
- CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT=n must preserve legacy behavior exactly until step docs say otherwise.
- When that Kconfig is y and default layout is used, on-flash marker semantics must match proposal §1.2 (same bytes/offsets as today).
- Keep new layout types in priv_include/ unless the step explicitly says otherwise — no new stable public API in include/ without maintainer decision.
- Target nand_impl.c marker/OOB behavior changes under Kconfig=y must stay in sync with Linux nand_impl_linux.c by step 10 at latest (step doc may require same PR earlier — follow the step file).
- PR size: aim ≤500–700 meaningful LOC; one logical concern per PR.

STEP 11 REMINDER (only when NN=11): test_app is mandatory — full pytest_spi_nand_flash.py on hardware for BOTH sdkconfig.ci.oob_layout AND sdkconfig.ci.bdl_oob_layout; host_test full pytest with sdkconfig.ci.oob_layout. Build-only is insufficient.

DELIVERABLES FOR THIS SESSION:
1. Code + configs exactly as step-NN describes.
2. Short summary: files changed, how to build/test per step doc.
3. Explicit statement: acceptance criteria checkboxes from step-NN addressed (yes/no per item).
4. Any blocker (missing prerequisite merge, unclear spec) with zero guessing.

Start by quoting the step NN title and goal from step-NN-*.md, then implement.
```

---

## Before starting a session (human checklist)

- [ ] Steps **01 … NN−1** are merged (or you explicitly accept implementing from a branch that contains them).
- [ ] You replaced **`NN`** and, if needed, the concrete **`.../idf-extra-components/spi_nand_flash`** path in your environment.

## Step file names (for lookup)

| NN | File |
|----|------|
| 01 | `step-01-kconfig-and-build-guard.md` |
| 02 | `step-02-priv-types-header.md` |
| 03 | `step-03-default-layout-table.md` |
| 04 | `step-04-xfer-scatter-gather-contiguous.md` |
| 05 | `step-05-device-state-and-init-hook.md` |
| 06 | `step-06-nand-is-free.md` |
| 07 | `step-07-nand-prog-markers.md` |
| 08 | `step-08-nand-bbm-paths.md` |
| 09 | `step-09-nand-copy.md` |
| 10 | `step-10-linux-parity.md` |
| 11 | `step-11-tests-and-ci-matrix.md` |
| 12 | `step-12-docs-and-follow-ups.md` |

## Optional one-liner (minimal)

```
Implement spi_nand_flash (idf-extra-components repo) openspec/changes/configurable-oob-layout/step-NN-*.md only; read README + openspec/configurable_oob_layout_proposal.md first; rules: no Dhara changes, Kconfig off = legacy, priv_include only for new types, follow acceptance criteria in the step file.
```
