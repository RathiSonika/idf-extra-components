# Step 03 — SPI parameter page read primitive(s)

**PR identifier:** `anonymous-chip-03`  
**Depends on:** step **02**  
**Estimate:** ~200–500 LOC

## Goal

Implement **target-only** SPI NAND sequences to read the **256-byte** ONFI parameter page (implementation-defined command / OTP / row addressing per part class — port **ideas** from POC branch `feat/nand_generic_fallback`, not file-for-file). Buffers must follow proposal §5.7: **bounded heap**, **DMA-capable** where `spi_nand_oper` requires, **freed on every exit path**.

## Scope

| Area | Action |
|------|--------|
| `src/spi_nand_oper.c` / `priv_include/spi_nand_oper.h` | New static or exported helpers: e.g. `spi_nand_read_parameter_page(...)` as needed by step **04** |
| New `src/nand_onfi_read.c` (example) | Orchestrate copies/rows if multi-copy read is required — keep upper bound documented |

## Out of scope

- `nand_init_device` tier wiring (step **05**).
- Parsing geometry from bytes (step **04**).
- Linux emulator path (proposal §8).
- Manual tier (step **06**).

## Background

Proposal §5.2 step 1 (parameter page retrieval); §5.7 (allocation caps, `MALLOC_CAP_DMA`, no persistent probe buffer on `spi_nand_flash_device_t`).

## Implementation checklist

1. Document **worst-case** allocated bytes (e.g. N × 256 + transaction overhead) in code comment or `README` snippet; N must be **fixed constant**, not derived from untrusted flash.

2. Use existing `spi_nand_oper` patterns for command + address + data phases; respect caller `spi_device_handle_t` from config (baseline §5.3).

3. **All** paths: `goto cleanup` or equivalent — **no leaks** on `ESP_ERR_*` early returns (proposal §5.7).

4. Compile only when `CONFIG_NAND_FLASH_ANONYMOUS_DETECT=y` and **not** `IDF_TARGET_LINUX` (align with step **01**).

5. Optionally expose an internal test hook (weak symbol / `TEST_ONLY`) — **not required** if hardware-only until step **08**; prefer keeping surface minimal.

## Acceptance criteria

- [ ] With **`CONFIG_NAND_FLASH_ANONYMOUS_DETECT=n`**, this module is **not** linked or is dead-stripped without references (match CMake story from step **01**).
- [ ] Successful read returns 256 bytes to caller-owned or heap buffer with documented lifetime ownership (caller frees **or** single-shot API allocates+frees internally — pick one pattern and document).
- [ ] Static analysis / code review: every `malloc`/`heap_caps_malloc` paired on **all** branches.

## Verification

- `cd test_app && idf.py set-target esp32 && idf.py build` with **`CONFIG_NAND_FLASH_ANONYMOUS_DETECT=y`** (local `sdkconfig` or fragment).
- Hardware smoke (optional this step): log raw first 16 bytes after read on a known ONFI part — **not** a merge blocker if step **08** covers HW; document if deferred.

## Risks / notes

- **Watchdog:** Long multi-row scans could trigger TWDT; keep copy count small or yield policy documented (baseline §9.3 WDT culture).
- **DMA alignment:** Match fixes noted in baseline §6 (DMA / alignment history).
- Task context only — same as existing `nand_init_device` (baseline §5.3).

## References

- [`../../anonymous_chip_detect_proposal.md`](../../anonymous_chip_detect_proposal.md) §5.2, §5.7, §12 (buffer policy).
