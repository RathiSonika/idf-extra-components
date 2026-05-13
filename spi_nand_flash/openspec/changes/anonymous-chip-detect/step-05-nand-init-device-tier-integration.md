# Step 05 — `nand_init_device` tier integration (Tier 1 → Tier 2)

**PR identifier:** `anonymous-chip-05`  
**Depends on:** step **04**  
**Estimate:** ~250–550 LOC

## Goal

Integrate the **three-tier** policy into **`nand_init_device`** (`src/nand_impl.c`) when **`CONFIG_NAND_FLASH_ANONYMOUS_DETECT=y`**: run **Tier 1** as today; on Tier 1 failure (unknown manufacturer **or** vendor init failure before device ready — proposal §5.1), attempt **Tier 2** (ONFI module). Set **`chip_source = DATABASE`** on Tier 1 success with **no** extra SPI traffic vs baseline (proposal §5.1). Tier 3 remains behind step **06** Kconfig — either stub “not compiled yet” or call into step 06 symbols if merged together (prefer **step 05** leaves a clear hook `if (CONFIG_NAND_FLASH_ANONYMOUS_MANUAL) { ... }` filled in step **06**).

## Scope

| File | Action |
|------|--------|
| [`../../../src/nand_impl.c`](../../../src/nand_impl.c) | Tier ordering, logging, return code normalization |
| [`../../../src/nand_flash_blockdev.c`](../../../src/nand_flash_blockdev.c) | **No change** if it only calls `nand_init_device` — verify BDL path inherits behavior (baseline §5.4) |
| `priv_include/nand.h` | Ensure `chip_source` default = DATABASE at handle creation |

## Out of scope

- **`CONFIG_NAND_FLASH_ANONYMOUS_MANUAL`** full implementation if deferred: step 05 may `if (0)` or empty branch with comment “step 06” — **better:** implement step **06** in same PR if tiny; otherwise step 05 ends with Tier 2 only and Tier 3 returns `ESP_ERR_NOT_FOUND` until step 06 merges.
- Linux `nand_impl_linux.c` (proposal §8 — no anonymous SPI); step **09** asserts parity.
- Public API (step **07**).

## Background

Proposal §5.1 table (failure modes, **`ESP_ERR_NOT_FOUND`** when anonymous on and all tiers exhausted); §9 (warnings for Tier 2); §5.6 (if config `io_mode` is QOUT/QIO, **`ESP_LOGW`** and SIO effective path — enforce before heavy I/O if not done in step **04**).

## Implementation checklist

1. When **`CONFIG_NAND_FLASH_ANONYMOUS_DETECT=n`**, compile path must match **pre-change** control flow (proposal §5.1).

2. Tier 1 success: set internal **`chip_source = DATABASE`**, clear internal anonymous flag (proposal §7).

3. Tier 2: invoke read + parse from steps **03–04**; on success apply minimal register setup required for generic SPI NAND operation (block unlock, etc.) — document assumptions (proposal §5.5).

4. **Final errors:** With anonymous **on**, after Tier 2 fails and Tier 3 unavailable/disabled, return **`ESP_ERR_NOT_FOUND`** (proposal §5.1). With anonymous **off**, preserve baseline errors from Tier 1 failure.

5. **Logging:** Tier 2 success → **`ESP_LOGW`** per proposal §9.

## Acceptance criteria

- [ ] Tier 1 success: no ONFI parameter page traffic (proposal §5.1 invariant).
- [ ] Anonymous on + all tried tiers fail ⇒ **`ESP_ERR_NOT_FOUND`** (proposal §5.1).
- [ ] Anonymous off ⇒ baseline **`esp_err_t`** from existing vendor/detect paths unchanged for representative failure cases (document which paths were compared, e.g. unknown ID).
- [ ] `nand_flash_get_blockdev` / legacy init both exercise same `nand_init_device` (baseline §5.3–§5.4).

## Verification

```bash
cd test_app && idf.py set-target esp32 && idf.py build
# With sdkconfig: CONFIG_NAND_FLASH_ANONYMOUS_DETECT=y — smoke init on known database part (should still Tier-1 succeed)
```

Regression: default CI preset without anonymous still green (step **08** formalizes).

## Risks / notes

- **Vendor init side effects:** Tier 1 may partially configure chip before failing; ONFI path must tolerate reset or define ordering (reset chip before Tier 2 if needed — document hardware assumption).
- **Concurrency:** Init runs in caller task context before mutex use pattern stabilizes — same as baseline init (baseline §9.1).

## References

- [`../../anonymous_chip_detect_proposal.md`](../../anonymous_chip_detect_proposal.md) §5.1, §5.5–§5.6, §9.
- [`../../baseline.md`](../../baseline.md) §5.3–§5.4, §9.1.
