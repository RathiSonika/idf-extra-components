# Docs — Component README and CHANGELOG

**PR identifier:** `anonymous-chip-docs`  
**Depends on:** [step-tests-ci-host-linux.md](step-tests-ci-host-linux.md)  
**Estimate:** ~80–250 LOC (markdown only)

## Goal

Update component **`README.md`** and **`CHANGELOG.md`** to describe anonymous detection tiers, Kconfig gates, limitations (multi-LUN ONFI, SIO-only Tier 2/3, no `oob_size` from ONFI in v1), production guidance, and pointers to [`../../anonymous_chip_detect_proposal.md`](../../anonymous_chip_detect_proposal.md) and [`../../RFC_Anonymous_Chip_Detect.md`](../../RFC_Anonymous_Chip_Detect.md) with explicit note that **RFC OOB layout snippets are not v1 behavior** (proposal §5.4).

## Scope

| File | Action |
|------|--------|
| [`../../../README.md`](../../../README.md) | User-facing: tiers, Kconfig, `spi_nand_get_chip_source`, safety warnings |
| [`../../../CHANGELOG.md`](../../../CHANGELOG.md) | Versioned entry under next release |

## Out of scope

- Rewriting the full RFC or proposal (link only).
- OpenSpec folder edits unless a cross-link to this change folder is required.

## Background

Proposal §2 goal 4, §11; **Tests** milestone §10.C procedure should be referenced from README for hardware evidence.

## Implementation checklist

1. **README section:** “Anonymous chip detection (opt-in)” — bullets for Tier 1/2/3, defaults **`n`**, Linux limitation (proposal §8).
2. **CHANGELOG:** Conventional entry; breaking changes **none** expected (additive API).
3. **Cross-links:** Relative links from component root to `openspec/` artifacts consistent with other features.
4. **Hardware evidence:** Point to `test_app` / **Tests** milestone doc for §10.C procedure.

## Acceptance criteria

- [ ] New public API and Kconfig symbols documented with correct defaults (proposal §6).
- [ ] Production warning matches intent: prefer database parts (RFC usage recommendations).
- [ ] No claim that ONFI validates arbitrary OOB beyond baseline 4-byte markers (proposal §5.4).

## Verification

- Markdown link check (manual): GitHub preview of README.
- Optional: `idf.py docs` if project generates docs.

## Risks / notes

- Avoid over-promising filesystem safety — align with baseline §9.4 / proposal §3 non-goals.

## References

- [`../../anonymous_chip_detect_proposal.md`](../../anonymous_chip_detect_proposal.md) §2–§7, §10.
- [`../../RFC_Anonymous_Chip_Detect.md`](../../RFC_Anonymous_Chip_Detect.md) (intent + guardrails; OOB code blocks deferred).
