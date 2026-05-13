# Step 09 — `host_test`: ONFI CRC golden vectors + Linux parity (§10.A/D)

**PR identifier:** `anonymous-chip-09`  
**Depends on:** step **02** (CRC); step **07** optional for getter assertion  
**Estimate:** ~150–400 LOC

## Goal

**§10.A:** Add **host** tests for ONFI parameter page **CRC** validation and any **pure** parse helpers (golden vectors: valid page, corrupted CRC, bad `ONFI` signature).  
**§10.D:** Assert that on **`IDF_TARGET_LINUX`**, enabling anonymous-related Kconfig symbols **does not** change emulator init identity/geometry path vs baseline (proposal §8, §10.D).

## Scope

| Area | Action |
|------|--------|
| [`../../../host_test/`](../../../host_test/) | New or extended `test_*.cpp` / `unity` cases |
| CMake / `main/CMakeLists.txt` | Link CRC/helper sources if built as separate compile unit (coordinate step **02** decision) |

## Out of scope

- Real SPI from host OS.
- Changing Linux emulator synthetic IDs (baseline §4.7) unless a bug is found — **this step should prove no drift**, not change behavior.

## Background

Proposal §10.A, §10.D; baseline §4.7 synthetic chip values.

## Implementation checklist

1. **Golden vectors:** Commit small binary blobs or C arrays under `host_test/` for 256-byte parameter pages (valid + invalid cases). Document CRC polynomial used (match step **02**).

2. **CRC tests:** Call shared CRC routine; assert pass/fail edges.

3. **Linux parity test:** Build with `sdkconfig.defaults` fragment that sets `CONFIG_NAND_FLASH_ANONYMOUS_DETECT=y` **if** step **01** allows symbol on linux for build-only — **note:** step **01** may forbid linux for master; then §10.D is satisfied by “symbol absent on linux” + test that default linux build unchanged. **Normative:** proposal §8 says anonymous SPI not supported on linux; §10.D requires anonymous **does not alter** emulator path — if Kconfig cannot be `y` on linux, the test documents “linux build + `spi_nand_flash_init_device` still yields synthetic `0xEF` / `0xE100` / mmap path” with master **`n`** and optionally verifies no link of ONFI object files. **Lock** the chosen interpretation in the test comment citing proposal §8 vs §10.D.

4. If `spi_nand_get_chip_source` is testable on Linux after init: assert **DATABASE** or documented equivalent for emulator (baseline: Linux uses database-like path — verify actual post-init enum value and update test if emulator is special-cased).

## Acceptance criteria

- [ ] Host test executable runs in CI / `pytest_nand_flash_linux.py` includes new cases (proposal §10.A).
- [ ] §10.D documented by an automated assertion or an explicit **skipped** test with comment — **prefer** automated assertion for the supported Kconfig combination.
- [ ] No new failures in default host matrix.

## Verification

```bash
cd host_test && idf.py --preview set-target linux && idf.py build flash monitor
# or
pytest ../host_test/pytest_nand_flash_linux.py -k <new-case>
```

## Risks / notes

- **Dual interpretation of §10.D:** Resolve once in PR description with proposal §8 citation.

## References

- [`../../anonymous_chip_detect_proposal.md`](../../anonymous_chip_detect_proposal.md) §8, §10.A, §10.D.
- [`../../baseline.md`](../../baseline.md) §4.7.
