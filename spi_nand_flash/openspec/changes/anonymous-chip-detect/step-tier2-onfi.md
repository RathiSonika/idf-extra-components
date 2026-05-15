# Tier 2 — ONFI (private types, SPI read, parse/init, `nand_init_device` integration)

**PR identifier:** `anonymous-chip-tier2`  
**Depends on:** [step-tier1-foundation-and-database-path.md](step-tier1-foundation-and-database-path.md)  
**Estimate:** ~800–2000 LOC total; **optional split:** (2a) types + SPI + parse module without `nand_init_device` hook, then (2b) init integration — same milestone doc, two PRs if review load demands ([README.md](README.md) conventions).

## Goal

Implement **Tier 2 — ONFI** per proposal §5.2, §5.4–§5.7, §5.6, §9:

1. **`priv_include/`** packed parameter page layout, ONFI constants, **pure CRC** validation (proposal §5.2 integrity, §7 — types stay private).
2. **Target SPI:** Read **256-byte** ONFI parameter page; bounded **heap**, **DMA-capable** where `spi_nand_oper` requires, **freed on every exit path** (§5.7).
3. **Parse / init module:** Signature bytes **0–3 == `ONFI`**; CRC; **`num_luns == 1`** or fail (§5.2a); extract geometry, delays, **`nand_ecc_data_t`**; **ignore** `spare_bytes_per_page` for init (§5.2); **SIO only** — no QE (§5.6); internal **`SPI_NAND_CHIP_FLAG_ANONYMOUS`** + **`chip_source = ONFI`** (§7).
4. **`nand_init_device`:** After **Tier 1** failure (unknown manufacturer **or** vendor init does not yield ready device — §5.1), invoke Tier 2. Tier 1 success: still **no** extra SPI vs baseline (§5.1). Tier 2 success → **`ESP_LOGW`** (§9). Tier 2 fail and Tier 3 unavailable/disabled → **`ESP_ERR_NOT_FOUND`** (§5.1). Anonymous **`n`** → baseline Tier 1 errors unchanged.

## Scope

| Area | Action |
|------|--------|
| `priv_include/` | e.g. `nand_onfi_param_page.h`: packed `nand_parameter_page_t` (or equivalent), fields needed for §5.2 table; **no** `include/` exposure. |
| `src/` | Optional `nand_onfi_crc.c` or static inline — **coordinate** with **Tests** milestone for `host_test` linkage (CRC must be testable from host). |
| `src/spi_nand_oper.c` / `priv_include/spi_nand_oper.h` | Parameter page read helper(s) for Tier 2. |
| New `src/` unit(s) | e.g. `nand_onfi_read.c`, `nand_onfi.c`: orchestrate reads, `nand_onfi_try_init(...)` or equivalent. |
| [`../../../src/nand_impl.c`](../../../src/nand_impl.c) | Call Tier 2 after Tier 1 failure when `CONFIG_NAND_FLASH_ANONYMOUS_DETECT=y`; normalize errors; **`ESP_LOGW`** on Tier 2 success; if `io_mode` QOUT/QIO → **`ESP_LOGW`** + effective SIO (§5.6). |
| `priv_include/nand.h` | Private `chip_source` enum value ONFI; anonymous flag storage if not present. |

## Out of scope

- **Tier 3** manual Kconfig fields and manual init branch (**Tier 3** milestone).
- Public **`spi_nand_get_chip_source`** (**Tier 3**).
- **`sdkconfig.ci.anonymous`**, pytest matrix, host golden vectors (**Tests** milestone).
- Linux emulator ONFI/SPI (proposal §8).

## Background

Proposal §5.5 (minimal register setup, assumptions); §12 POC gap list (ONFI gate, no public param page type, no QE guess, SIO).

## Implementation checklist

### A — Private types + CRC (merged from legacy step 02)

1. **`nand_parameter_page_t`:** packing / endianness for fields read in parse (`data_bytes_per_page`, `pages_per_block`, `blocks_per_lun`, `num_luns`, timings, `ecc_correctability`, manufacturer/model strings as needed).
2. **CRC API:** 256-byte buffer; validate bytes **0–253** vs **254–255**; document polynomial/init in header (verify against ONFI 1.0 / errata in implementation).
3. **Include guards**; no inclusion from `include/spi_nand_flash.h`.
4. **Host linkage:** choose always-built tiny CRC unit vs `CONFIG_NAND_FLASH_ANONYMOUS_DETECT` gating — **lock** in PR so **Tests** milestone CMake does not guess.

### B — SPI parameter page read (merged from legacy step 03)

1. Document **worst-case** allocated bytes (fixed **N** × 256 + overhead); **N** fixed constant, not from untrusted flash.
2. Use existing `spi_nand_oper` patterns; respect `spi_device_handle_t` (baseline §5.3).
3. **All** paths free heap — no leaks on `ESP_ERR_*`.
4. Compile when `CONFIG_NAND_FLASH_ANONYMOUS_DETECT=y` and **not** `IDF_TARGET_LINUX` (align **Tier 1** Kconfig).

### C — ONFI parse and chip setup (merged from legacy step 04)

1. Reject bad signature/CRC; document mapping to `ESP_ERR_*`; **Tier 2** integration normalizes aggregate failure to **`ESP_ERR_NOT_FOUND`** when anonymous on (§5.1).
2. Map page/block fields; **`num_blocks`** when `num_luns == 1`; reject **`num_luns != 1`** (§5.2a).
3. Timing → `read_page_delay_us`, `program_page_delay_us`, `erase_block_delay_us` (match vendor modules).
4. **`ecc_correctability`:** log / conservative `nand_ecc_data_t` (§5.5).
5. Optional log of `spare_bytes_per_page` — **not** used for geometry (§5.2).
6. **QE:** do not enable (§5.6); single ownership for SIO downgrade with **`nand_impl`** as needed.

### D — `nand_init_device` Tier 2 wiring (merged from legacy step 05, Tier 2 portions)

1. Invoke read + parse from A–C on Tier 1 failure.
2. Tier 2 success: minimal register setup (block unlock, etc.) — document assumptions (§5.5).
3. Leave **`if (CONFIG_NAND_FLASH_ANONYMOUS_MANUAL)`** hook for **Tier 3** or empty branch returning **`ESP_ERR_NOT_FOUND`** until **Tier 3** merges.

## Acceptance criteria

- [ ] **`CONFIG_NAND_FLASH_ANONYMOUS_DETECT=n`:** Tier 2 code not linked or dead-stripped without references (CMake matches **Tier 1**).
- [ ] Tier 1 success: **no** ONFI parameter page traffic (§5.1).
- [ ] Anonymous on + Tier 1 fail + Tier 2 fail + Tier 3 disabled ⇒ **`ESP_ERR_NOT_FOUND`** (§5.1).
- [ ] Anonymous off ⇒ baseline **`esp_err_t`** from Tier 1 failure unchanged (document compared paths).
- [ ] **`num_luns != 1`** ⇒ ONFI tier fails without “success” mutation (§5.2a).
- [ ] Tier 2/3 path does **not** enable quad/QE; **`ESP_LOGW`** if QOUT/QIO requested (§5.6).
- [ ] No public `oob_size` from ONFI (§5.4–§5.5).
- [ ] Every `malloc` / `heap_caps_malloc` paired on **all** branches (§5.7).

## Verification

```bash
cd test_app && idf.py set-target esp32 && idf.py build
# Local sdkconfig or fragment: CONFIG_NAND_FLASH_ANONYMOUS_DETECT=y — smoke init on known database part (Tier 1 still succeeds)
```

(Optional: hardware log first 16 bytes of param page on ONFI part — not merge blocker if **Tests** / lab covers HW.)

## Risks / notes

- **Vendor init side effects:** Tier 1 may partially configure chip; ONFI path may need reset or ordering — document hardware assumption.
- **Watchdog:** multi-row scans — keep copy count small (baseline §9.3).
- **DMA alignment:** baseline §6.
- **Task context** only for init (baseline §5.3).

## References

- [`../../anonymous_chip_detect_proposal.md`](../../anonymous_chip_detect_proposal.md) §5.1, §5.2, §5.2a, §5.4–§5.7, §5.6, §7, §9, §12.
- [`../../baseline.md`](../../baseline.md) §5.3–§5.4, §6, §9.1, §9.3.
