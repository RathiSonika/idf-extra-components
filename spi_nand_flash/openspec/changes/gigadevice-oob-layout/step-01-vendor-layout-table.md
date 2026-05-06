# Step 01 — Add the GigaDevice GD5F1GQ4xA layout table

**PR identifier:** `gd-oob-01`
**Depends on:** all `configurable-oob-layout/` steps 01–12 merged.
**Estimate:** ~80–150 LOC (one new header + one new .c, all under `priv_include/` and `src/`).

## Goal

Land the **layout table only** — a single static `spi_nand_oob_layout_t` describing GD5F1GQ4xA-family spare, plus a getter `nand_oob_layout_get_gigadevice_q4xa()`. **No callers** in this PR. The next step (02) wires the selector.

This isolates the data definition from the integration so the diff is reviewable without dragging in handle-struct changes or vendor-init wiring.

## Non-goals

- No change to `nand_oob_device.c`, the device handle, or any vendor init file. Those land in step 02.
- No tests. Tests land in step 03 (so V1–V5 see both the table and the resolver).
- No 128 B-spare variant2 layout. That is a separate proposal (parent proposal §7.1 Q3).
- No public API in `include/`.

## Files to touch

| File | Action |
|------|--------|
| [`priv_include/nand_oob_layout_gigadevice.h`](../../../priv_include/nand_oob_layout_gigadevice.h) | **New.** Declare `nand_oob_layout_get_gigadevice_q4xa(void)`. |
| [`src/nand_oob_layout_gigadevice.c`](../../../src/nand_oob_layout_gigadevice.c) | **New.** Static `s_nand_oob_layout_gigadevice_q4xa` + free/ecc ops + getter. |
| [`CMakeLists.txt`](../../../CMakeLists.txt) | Add `src/nand_oob_layout_gigadevice.c` to the source list, gated the same way `src/nand_oob_layout_default.c` is (under `CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT`). |

## Layout to encode (mirror parent proposal §2.1)

Both ops are simple `switch` statements; the offsets and lengths come straight from §2.1 of [`../../gigadevice_oob_layout_proposal.md`](../../gigadevice_oob_layout_proposal.md):

```text
oob_bytes        : 0   (defer to chip mapping = 64 for 2 KiB page)
bbm.offset       : 0
bbm.length       : 2   (Espressif 2-byte BBM — preserves default-layout bytes)
bbm.good_pattern : {0xFF, 0xFF}
bbm.check_pages_mask : SPI_NAND_BBM_CHECK_FIRST_PAGE

free_region(section)            ecc_region(section)
  0: {2,  6, prog=true,  ecc_prot=true}     0: { 8, 8, prog=false, ecc_prot=true}
  1: {16, 8, prog=true,  ecc_prot=true}     1: {24, 8, prog=false, ecc_prot=true}
  2: {32, 8, prog=true,  ecc_prot=true}     2: {40, 8, prog=false, ecc_prot=true}
  3: {48, 8, prog=true,  ecc_prot=true}     3: {56, 8, prog=false, ecc_prot=true}
  >=4: ESP_ERR_NOT_FOUND                    >=4: ESP_ERR_NOT_FOUND
```

`chip_ctx` is **unused** by both callbacks (cast to `(void)`). The layout is selected per-DI by step 02; the callbacks themselves are DI-independent.

## Implementation checklist

1. **Header:** mirror `priv_include/nand_oob_layout_default.h` style (single getter, no struct definitions, `extern "C"` guard).
2. **Source:** mirror `src/nand_oob_layout_default.c` style — file-static layout struct, file-static ops struct, single exported getter.
3. **Free ops:** four `if (section == N) { fill; return ESP_OK; }` arms, plus a final `return ESP_ERR_NOT_FOUND;`. **Do not** loop or compute offsets at runtime — explicit constants make the table reviewable byte-for-byte against the datasheet.
4. **ECC ops:** same shape, four arms, one for each parity slice.
5. **Region fields:** `programmable = true` for free, `false` for ECC; `ecc_protected = true` for both (these bytes are inside the ECC engine's coverage even though parity bytes themselves are reserved).
6. **No allocations**, no logging, no globals other than the `static const` layout / ops structs.
7. **CMake:** the new `.c` is built under exactly the same condition that gates `nand_oob_layout_default.c` and the rest of the OOB plumbing — no new Kconfig symbol.

## Acceptance criteria

- [ ] `priv_include/nand_oob_layout_gigadevice.h` exists, has `extern "C"` guard, declares `nand_oob_layout_get_gigadevice_q4xa(void)` returning `const spi_nand_oob_layout_t *`, and includes only `nand_oob_layout_types.h`.
- [ ] `src/nand_oob_layout_gigadevice.c` defines exactly **one** layout struct, **one** ops struct, **two** static callbacks (`free_region`, `ecc_region`), and **one** exported getter.
- [ ] `CMakeLists.txt` includes the new source file under the same condition as `nand_oob_layout_default.c`.
- [ ] `idf.py build` succeeds for the existing `test_app` `sdkconfig.ci.oob_layout` preset (no new preset added in this step).
- [ ] `idf.py build --target linux` succeeds for `host_test` (still the existing preset; no new test cases yet).
- [ ] **Static review:** every offset/length in the file matches §2.1 of [`../../gigadevice_oob_layout_proposal.md`](../../gigadevice_oob_layout_proposal.md) exactly. A reviewer can cross-check by eye.
- [ ] No reference to the new getter from any other file in this PR.

## Risks

- **Wrong constants.** Mitigate via explicit per-section arms (§implementation 3 above) and reviewer cross-check against the proposal §2.1 table.
- **Style drift.** Mitigate by mirroring `nand_oob_layout_default.{c,h}` line for line.

## Notes for implementers

- This step is intentionally tiny. If the diff exceeds ~200 LOC, something extra crept in — most likely a handle-struct change that belongs in step 02. Pull it out.
- The four sections are written as four explicit `if (section == N)` arms (not a loop / lookup table). This is deliberate: the reviewer reads the file and sees the datasheet table directly. Compactness is **not** a goal here.
- Do not include `<stdio.h>`, `<esp_log.h>`, or any TAG macro. The file has no log statements.
