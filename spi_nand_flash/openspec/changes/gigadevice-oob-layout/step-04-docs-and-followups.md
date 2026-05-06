# Step 04 — Documentation, CHANGELOG, follow-ups

**PR identifier:** `gd-oob-04`
**Depends on:** steps **01–03** complete and green.
**Estimate:** ~50–150 LOC (docs only).

## Goal

1. **User-facing:** [`CHANGELOG.md`](../../../CHANGELOG.md) entry — first device-specific OOB layout, GD5F1GQ4xA family only, no on-flash format change for existing volumes.
2. **Maintainer-facing:** Short [`README.md`](../../../README.md) note pointing at [`../../gigadevice_oob_layout_proposal.md`](../../gigadevice_oob_layout_proposal.md) and this implementation folder, alongside the existing pointer to the configurable-oob-layout proposal.
3. **Deferred work list** (explicit, must be written into [`../../gigadevice_oob_layout_proposal.md`](../../gigadevice_oob_layout_proposal.md) §7.1 and / or this step doc as a follow-ups appendix):
   - **GigaDevice variant2 (128 B-spare) layout** (parent proposal §7.1 Q3) — separate proposal needed; requires per-chip `oob_bytes` override and Linux mmap stride change for those parts. Owners: TBD when scheduling.
   - **DIs that failed datasheet verification in step 02** — list each removed DI here with the reason ("could not confirm 64 B spare", "datasheet unavailable", etc.) and a pointer to the datasheet revision used.
   - **Promoting per-vendor `get_oob_layout` hook to other vendors** (Winbond / Micron / Alliance / XTX / Zetta) — separate per-vendor proposals; same shape as this epic.
   - **ECC-mode-keyed layouts** (parent proposal §2.1a) — needed for parts where datasheet exposes runtime ECC enable/disable; this epic does not need it because Q4xA-family ECC is always on.
   - **On-target pytest for `sdkconfig.ci.gigadevice_oob`** if step 03 ran build-only — note here when hardware becomes available and link to the run.
   - **`SPI_NAND_OOB_MAX_REGIONS = 8`** (parent proposal Q3) — Q4xA uses 4 sections per class, well within 8. No change needed; just confirm the cap remains adequate.

## Non-goals

- Rewriting baseline.md (separate task once the framework + this epic are both shipped).
- Promoting any private symbol to `include/`. Public API is a separate epic.
- Updating other vendors' device init files. Those are separate per-vendor proposals.

## Files to touch

| File | Action |
|------|--------|
| [`CHANGELOG.md`](../../../CHANGELOG.md) | Entry under the appropriate version section. |
| [`README.md`](../../../README.md) | Short subsection (or paragraph) under whatever section the configurable-oob-layout pointer lives in. |
| [`../../gigadevice_oob_layout_proposal.md`](../../gigadevice_oob_layout_proposal.md) | Update §7.1 to mark Q1 (DI verification) **resolved** with the actual DI list shipped, and to record any deferred DIs. |
| [`../../configurable_oob_layout_proposal.md`](../../configurable_oob_layout_proposal.md) | If this epic exposed a new follow-up that should be tracked in the framework parent (e.g. variant2 layout work), add it under §7.1 as a cross-reference; do **not** rewrite resolved decisions. |

## Implementation checklist

1. **CHANGELOG entry** is **one** short paragraph plus the Kconfig symbol name verbatim. It must clearly state:
   - This is **opt-in** behind `CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT`.
   - It changes **no on-flash bytes** for the targeted GD parts vs the previous default-layout build.
   - It is the **first** device-specific layout — others are separate follow-ups.
2. **README note** is short (4–6 lines) and links to the proposal for anyone who wants the design rationale. Do **not** copy the layout table into the README.
3. **Proposal §7.1 sync:** mark Q1 resolved with the **actual** list of DIs that landed in step 02's resolver. If any DI was deferred, list it under Q3 (variant2 / unverified) with the reason.
4. **Cross-link** the framework parent if this epic surfaces work that belongs there (e.g. a new follow-up to add an ECC-mode-key for some other vendor).
5. **Do not** flip `CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT` to default-on in this step. The framework parent's step 12 owns that decision; landing the first device-specific layout is **not** the trigger.

## Acceptance criteria

- [ ] CHANGELOG entry follows the existing format and mentions the Kconfig symbol verbatim.
- [ ] README has a short note discoverable from the existing OOB-layout pointer.
- [ ] [`../../gigadevice_oob_layout_proposal.md`](../../gigadevice_oob_layout_proposal.md) §7.1 reflects the actual shipped DI set; any deferred DI is documented.
- [ ] No production source file (under `src/` or `priv_include/`) is changed in this PR.
- [ ] No new Kconfig symbols added.
- [ ] If on-target hardware testing was deferred in step 03, this PR's deferred-work list captures it with a clear next step.

## Risks

- **Over-promising stability.** Keep "experimental" language consistent with the framework parent — this epic does not promote anything to stable.
- **Drift between docs.** A late edit in the proposal that isn't reflected in CHANGELOG / README causes confusion. Mitigate by reading all three in one sitting before requesting review.

## Notes for implementers

- After this step, maintainers can open separate epics for **other GigaDevice variants** (variant2 / 128 B spare) and for **other vendors** (Winbond / Micron / Alliance / XTX / Zetta) using this epic as the template.
- The "first device-specific OOB layout" is a milestone worth stating clearly in CHANGELOG, but resist the urge to write a tutorial — link to the proposal instead.
