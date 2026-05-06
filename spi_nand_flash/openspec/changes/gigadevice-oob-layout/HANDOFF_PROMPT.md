# Handoff prompt — GigaDevice GD5F1GQ4xA OOB layout (copy into a new session)

Use **one session per step**. Replace **`NN`** below with `01` … `04`.

---

## Paste this into the new chat/session

```
You are implementing ONE incremental PR for the spi_nand_flash component.

REPOSITORY CONTEXT: This component lives in the **idf-extra-components** project (it is **not** inside the ESP-IDF repository). The **git root** is typically `idf-extra-components/`; the **component under work** is the `spi_nand_flash` directory (sibling to other extra components). Use `test_app/` and `host_test/` under that directory with ESP-IDF as the **toolchain/SDK** (build from those app folders with `idf.py`), not as if spi_nand_flash were `$IDF_PATH/components/...`.

EPIC: GigaDevice GD5F1GQ4xA-family OOB layout — the FIRST device-specific OOB layout, building on the configurable-oob-layout framework.

PREREQUISITE (must be merged before any step in this epic):
  - openspec/configurable_oob_layout_proposal.md (framework parent)
  - openspec/changes/configurable-oob-layout/ steps 01–12 (all merged)

If any framework step is missing, STOP and report the gap. Do NOT implement around missing prerequisites.

STEP TO IMPLEMENT: NN only (do not implement earlier or later steps).

MANDATORY READ ORDER (open and follow in full before coding):
1. openspec/configurable_oob_layout_proposal.md — framework intent, default-preserves-bytes invariant (§7.0), priv_include policy, hot-path rules.
2. openspec/gigadevice_oob_layout_proposal.md — THIS epic's intent: the Q4xA layout (§2.1), the vendor selector hook (§2.2), the byte-for-byte compat invariant (§2.4), DI verification gate (§2.2 callout), assumptions (§3), risks (§5), validation (§6).
3. openspec/changes/gigadevice-oob-layout/README.md — global PR rules and the **Implementation decisions** table (selection mechanism, DI scope, BBM length=2, oob_bytes=0, priv_include only).
4. openspec/changes/gigadevice-oob-layout/step-NN-*.md — sole source of truth for THIS PR's scope, files, checklist, acceptance criteria.

ABSOLUTE RULES:
- Implement ONLY step NN. If something obviously depends on a prior step not merged, stop and say so.
- Do NOT modify the vendored Dhara library, nand_impl.c, nand_impl_linux.c, or dhara_glue.c. The framework parent already routes through the layout abstraction; this epic only adds a layout DB row + a vendor hook.
- CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT=n must keep behavior unchanged from before this epic.
- Default-preserves-bytes (parent §7.0): a device formatted on the default layout must read byte-identically when re-mounted with the Q4xA layout selected.
- BBM stays at length 2 (NOT Linux MTD's 1) — Espressif compatibility.
- New layout / hook symbols stay under priv_include/. No new symbols in include/ in this epic.
- DI scope is exactly {0x31, 0x32, 0x25, 0x35}. Step 02 has a hard datasheet-verification gate before merging.
- PR size: aim ≤300–400 meaningful LOC.

STEP 02 REMINDER (only when NN=02): the four DIs MUST be confirmed against the chip's datasheet as Q4xA-family 64 B-spare parts before merging. If any DI cannot be verified, REMOVE it from the resolver and document the deferral.

STEP 03 REMINDER (only when NN=03): host_test cases V1–V5 are mandatory; test_app preset is required to build-pass; on-target full pytest is required ONLY if GD5F1GQ4xA / -2GQ4xA / -4GQ4xA hardware is on the bench.

DELIVERABLES FOR THIS SESSION:
1. Code + configs exactly as step-NN describes.
2. Short summary: files changed, how to build/test per step doc.
3. Explicit statement: acceptance criteria checkboxes from step-NN addressed (yes/no per item).
4. Any blocker (missing prerequisite merge, unclear spec, datasheet uncertainty) with zero guessing.

Start by quoting the step NN title and goal from step-NN-*.md, then implement.
```

---

## Before starting a session (human checklist)

- [ ] All `configurable-oob-layout/` steps **01 … 12** are merged (or you explicitly accept implementing from a branch that contains them).
- [ ] Steps **01 … NN−1** of this epic are merged.
- [ ] Datasheets for GD5F1GQ4xA / GD5F2GQ4xA / GD5F4GQ4xA (all four DIs) are in hand — required before step 02 is merged.
- [ ] You replaced **`NN`** in the prompt.

## Step file names (for lookup)

| NN | File |
|----|------|
| 01 | `step-01-vendor-layout-table.md` |
| 02 | `step-02-vendor-selector-hook.md` |
| 03 | `step-03-tests.md` |
| 04 | `step-04-docs-and-followups.md` |

## Optional one-liner (minimal)

```
Implement spi_nand_flash (idf-extra-components repo) openspec/changes/gigadevice-oob-layout/step-NN-*.md only; PRE-REQ: configurable-oob-layout 01–12 merged; read README + gigadevice_oob_layout_proposal.md + configurable_oob_layout_proposal.md first; rules: no Dhara/nand_impl* changes, default-preserves-bytes, BBM length=2, priv_include only, follow acceptance criteria in the step file.
```
