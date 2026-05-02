# Step 12 — Documentation, CHANGELOG, follow-ups

**PR identifier:** `oob-layout-12`  
**Depends on:** steps **01–11** complete and green  
**Estimate:** ~100–350 LOC (docs + changelog)

## Goal

1. **User-facing:** [`CHANGELOG.md`](../../../CHANGELOG.md) entry — experimental Kconfig, default `n`, behavior parity when `y` + default layout.
2. **Maintainer-facing:** Link from [`README.md`](../../../README.md) (short subsection) to [`../../configurable_oob_layout_proposal.md`](../../configurable_oob_layout_proposal.md) and this implementation folder; mention both [`test_app/README.md`](../../../test_app/README.md) and [`host_test/README.md`](../../../host_test/README.md) if step 11 updated them for OOB presets.
3. **Deferred work list** (explicit):
   - **`SPI_NAND_OOB_MAX_REGIONS` validation** against vendor datasheets (proposal Q3).
   - **Public API** under `include/` — raw OOB / field read/write (proposal Q4); remain private until promoted (baseline §4.0 stability contract).
   - **Per-vendor layout overrides** in `src/devices/*.c` for non-default spare maps.
   - **Known bugs / DMA / PSRAM** interaction ([`../../known-bugs.md`](../../known-bugs.md) Q8 / baseline §11.4.2) — step 05 added **no** new heap allocations, so no new pressure here. When the §11.4.2 fix moves the handle to internal RAM, the layout state moves with it for free; no extra change needed.
   - **Same-plane `nand_copy` invariant audit per part** — proposal §7.0 locks "chip-internal page-copy preserves OOB byte-for-byte" as a load-bearing assumption. Schedule a brief per-vendor datasheet check; if any supported part violates it under its ECC mode, that vendor module must opt out of the fast path in a separate change.
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
3. Update [`../../configurable_oob_layout_proposal.md`](../../configurable_oob_layout_proposal.md) §7.1 open questions — mark Q2/Q3/Q4/Q8 as **tracked** with pointers to follow-up issues if desired (optional edit). Q1/Q5/Q6/Q7 are already locked under §7.0 Resolved decisions.

## Acceptance criteria

- [ ] New users can discover option from README.
- [ ] Release notes accurate for default **`n`**.

## Risks

- Over-promising stability — keep “experimental” language until default flipped.

## Notes for implementers

- After this step, maintainers may open separate epics for **vendor-specific layouts** and **public debug API**.
