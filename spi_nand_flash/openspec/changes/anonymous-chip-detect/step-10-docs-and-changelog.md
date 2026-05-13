# Step 10 — Documentation and CHANGELOG

**PR identifier:** `anonymous-chip-10`  
**Depends on:** steps **08–09** (docs should describe merged behavior + CI)  
**Estimate:** ~80–250 LOC (markdown only)

## Goal

Update component **`README.md`** and **`CHANGELOG.md`** to describe anonymous detection tiers, Kconfig gates, limitations (multi-LUN, SIO-only Tier 2/3, no `oob_size` from ONFI in v1), production guidance, and pointers to [`../../anonymous_chip_detect_proposal.md`](../../anonymous_chip_detect_proposal.md) and [`../../RFC_Anonymous_Chip_Detect.md`](../../RFC_Anonymous_Chip_Detect.md) with explicit note that **RFC OOB layout snippets are not v1 behavior** (proposal §5.4, RFC intro cross-link).

## Scope

| File | Action |
|------|--------|
| [`../../../README.md`](../../../README.md) | User-facing: tiers, Kconfig, `spi_nand_get_chip_source`, safety warnings |
| [`../../../CHANGELOG.md`](../../../CHANGELOG.md) | Versioned entry under next release |

## Out of scope

- Rewriting the full RFC or proposal (link only).
- OpenSpec folder edits beyond this change’s README if not needed.

## Implementation checklist

1. **README section:** “Anonymous chip detection (opt-in)” — bullets for Tier 1/2/3, defaults **`n`**, Linux limitation (proposal §8).

2. **CHANGELOG:** Conventional entry referencing feature; breaking changes **none** expected (new API is additive).

3. **Cross-links:** Relative links from component root to `openspec/` artifacts as other features do.

4. **Hardware evidence:** Point to `test_app` doc from step **08** for §10.C procedure.

## Acceptance criteria

- [ ] New public API and Kconfig symbols are documented with correct defaults (proposal §6).
- [ ] Production warning matches intent: prefer database parts (RFC §Usage Recommendations).
- [ ] No claim that ONFI path validates arbitrary OOB beyond baseline 4-byte markers (proposal §5.4).

## Verification

- Markdown link check (manual): open README in GitHub preview.
- Optional: `idf.py docs` if project generates docs — not required if not in tree.

## Risks / notes

- Over-promising filesystem safety — stay aligned with baseline §9.4 / proposal §3 non-goals.

## References

- [`../../anonymous_chip_detect_proposal.md`](../../anonymous_chip_detect_proposal.md) §2–§7, §10.
- [`../../RFC_Anonymous_Chip_Detect.md`](../../RFC_Anonymous_Chip_Detect.md) (intent + guardrails; OOB code blocks deferred).
