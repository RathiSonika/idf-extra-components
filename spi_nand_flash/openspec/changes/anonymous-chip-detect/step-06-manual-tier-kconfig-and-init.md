# Step 06 — Manual tier (Kconfig geometry + Tier 3 init)

**PR identifier:** `anonymous-chip-06`  
**Depends on:** step **05**  
**Estimate:** ~200–500 LOC

## Goal

Implement **Tier 3** when **`CONFIG_NAND_FLASH_ANONYMOUS_MANUAL=y`**: read user-supplied page size, pages per block, block count, and timing fields from Kconfig; validate power-of-two / range rules required by existing geometry code; on invalid combination return **`ESP_ERR_INVALID_ARG`** with clear log (proposal §5.3). Success sets **`chip_source = MANUAL`**, internal **`SPI_NAND_CHIP_FLAG_ANONYMOUS`**, **`ESP_LOGW`** (proposal §9), and **SIO-only** behavior (proposal §5.6).

## Scope

| Area | Action |
|------|--------|
| [`../../../Kconfig`](../../../Kconfig) | Manual geometry + delay `int`/`hex` prompts (exact symbol names TBD; prefix `NAND_FLASH_ANONYMOUS_MANUAL_*` or similar per proposal §6 naming) |
| `src/nand_impl.c` (or `src/nand_anonymous_manual.c`) | Tier 3 branch after Tier 2 failure |

## Out of scope

- Spare-size / per-region OOB Kconfig (proposal §5.4 manual tier: no spare-size Kconfig v1).
- ONFI read changes (steps **03–04**).
- Publishing parameter page struct (proposal §7).

## Background

Proposal §5.3 (validation, `ESP_ERR_INVALID_ARG`); §6.1 (manual depends on master); §6.2 (help text: every value must match datasheet; optional “reject placeholder defaults” — if implemented, document exact rule in Kconfig help).

## Implementation checklist

1. Kconfig symbols for: `page_size` (or `log2`), `pages_per_block`, `num_blocks`, `t_r`, `t_prog`, `t_bers` (names aligned with geometry struct fields).

2. Validation mirrors existing checks used by vendor inits (fail fast on non-power-of-two page size if that is a global invariant).

3. Wire Tier 3 only when master + manual both **`y`** and Tier 2 failed.

4. Apply same minimal register setup strategy as Tier 2 (proposal §5.5) or shared helper to avoid duplication.

5. **QE:** do not enable; warn if QOUT/QIO requested (proposal §5.6).

## Acceptance criteria

- [ ] With **`CONFIG_NAND_FLASH_ANONYMOUS_MANUAL=n`**, no Tier 3 code path (proposal §6.1).
- [ ] Invalid Kconfig combo ⇒ **`ESP_ERR_INVALID_ARG`**, no partial handle leak (proposal §5.3).
- [ ] Tier 3 success ⇒ internal anonymous + manual provenance (proposal §7).
- [ ] Help text warns against production use without datasheet validation (RFC guardrails).

## Verification

```bash
cd test_app && idf.py set-target esp32 && idf.py build
# menuconfig: enable master + manual, set legal geometry — boot test if hardware available (optional)
```

## Risks / notes

- **Dangerous defaults:** Menuconfig placeholders must not imply “works everywhere” (proposal §6.2); consider `default` values that fail validation until user changes them, if product agrees.

## References

- [`../../anonymous_chip_detect_proposal.md`](../../anonymous_chip_detect_proposal.md) §5.3–§5.6, §6.1–§6.2.
