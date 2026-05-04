# Step 12 — Documentation, CHANGELOG, follow-ups

**PR identifier:** `oob-layout-12`  
**Depends on:** steps **01–11** complete and green  
**Estimate:** ~100–350 LOC (docs + changelog)

## Goal

1. **User-facing:** [`CHANGELOG.md`](../../../CHANGELOG.md) entry — experimental Kconfig, default `n`, behavior parity when `y` + default layout.
2. **Maintainer-facing:** Link from [`README.md`](../../../README.md) (short subsection) to [`../../configurable_oob_layout_proposal.md`](../../configurable_oob_layout_proposal.md) and this implementation folder; mention both [`test_app/README.md`](../../../test_app/README.md) and [`host_test/README.md`](../../../host_test/README.md) if step 11 updated them for OOB presets.
3. **Deferred work list** (explicit):
   - **`SPI_NAND_OOB_MAX_REGIONS` (8)** — revisit only if a datasheet-backed layout needs **>8** free-region fragments (root [`README.md`](README.md)).
   - **Public API** under `include/` — raw OOB / field read/write (proposal Q4); remain **private** until promoted (baseline §4.0 stability contract).
   - **Per-vendor layout rows** in `src/devices/*.c` for non-default spare maps (table → generic only; no `config` override — root [`README.md`](README.md)).
   - **~~[`known-bugs.md`](../../known-bugs.md) §11.4.2 (PSRAM / internal RAM)~~** — **fixed step 05**; after merge, trim or mark resolved in `known-bugs.md` / `baseline.md` §11.4 in a follow-up doc sync.
   - **~~§11.4.1 `nand_emul_get_stats`~~** — **fixed step 10**; same doc sync.
   - **`nand_diag_api.c`** — no change in this epic unless a later audit finds OOB assumptions.
   - **Same-plane `nand_copy` invariant audit per part** — proposal §7.0. Schedule a brief per-vendor datasheet check; if any supported part violates it under its ECC mode, that vendor module must opt out of the fast path in a separate change.
   - **Default Kconfig → `y`** — **do not** do in this step unless PM approves stability milestone.

## Non-goals

- Rewriting baseline.md (separate OpenSpec task after release).

## Files to touch

| File | Action |
|------|--------|
| [`CHANGELOG.md`](../../../CHANGELOG.md) | Entry |
| [`README.md`](../../../README.md) | Short “Experimental: configurable OOB layout” |

## Implementation checklist

1. CHANGELOG follows existing format; mention Kconfig symbol name exactly.
2. README explains: enabling experimental flag does not change on-flash format for supported chips vs legacy when using default layout tables.
3. Update [`../../configurable_oob_layout_proposal.md`](../../configurable_oob_layout_proposal.md) §7.1 — note **Q3** cap **8** resolved; **Q4** private for v1; **Q8** split into step **05** / **10** fixes; **Q2** remains (MVP ordering vs full scatter in one train — see root [`README.md`](README.md)). Sync [`../../known-bugs.md`](../../known-bugs.md) if §11.4.1 / §11.4.2 are fully addressed in tree.

## Acceptance criteria

- [ ] New users can discover option from README.
- [ ] Release notes accurate for default **`n`**.

## Risks

- Over-promising stability — keep “experimental” language until default flipped.

## Notes for implementers

- After this step, maintainers may open separate epics for **vendor-specific layouts** and **public debug API**.
