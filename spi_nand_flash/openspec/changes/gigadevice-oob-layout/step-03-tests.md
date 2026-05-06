# Step 03 — Tests (host_test V1–V5 + optional test_app preset)

**PR identifier:** `gd-oob-03`
**Depends on:** steps **01** and **02** merged.
**Estimate:** ~150–300 LOC (host_test cases + optional test_app sdkconfig preset).

## Goal

Land the validation suite required by parent proposal §6 (V1–V5) and add a **build-only** `test_app` preset so the new layout is exercised in the existing build matrix. On-target full pytest is **only** required if a GD5F1GQ4xA / -2GQ4xA / -4GQ4xA is on the lab bench at PR time.

## Non-goals

- No additional code in `src/` — this PR is tests + sdkconfig only.
- No coverage of variant2 / 128 B-spare GD parts. Out of scope.
- No new public API exposure.

## Files to touch

| File | Action |
|------|--------|
| [`host_test/main/test_nand_oob_layout_gigadevice.cpp`](../../../host_test/main/test_nand_oob_layout_gigadevice.cpp) | **New.** Catch2-style test cases V1–V5 below. Mirrors the existing `test_nand_*` test file conventions (TAG, fixture style, includes). |
| [`host_test/main/CMakeLists.txt`](../../../host_test/main/CMakeLists.txt) | Add the new test file to the host_test sources, gated under the same condition as the rest of the OOB-layout tests landed by `configurable-oob-layout/step-11-tests-and-ci-matrix.md`. |
| [`test_app/sdkconfig.ci.gigadevice_oob`](../../../test_app/sdkconfig.ci.gigadevice_oob) | **New.** Sdkconfig preset enabling `CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT=y`. Otherwise mirrors `sdkconfig.ci.oob_layout` (legacy + OOB on). Build matrix only — no special hardware required to compile. |
| [`test_app/pytest_spi_nand_flash.py`](../../../test_app/pytest_spi_nand_flash.py) | Conditional: register the new preset in the parametrize list **only** if the existing test file enumerates presets in code (some repos do this in `conftest.py` instead). If the preset is auto-discovered, no edit needed here. |
| [`.build-test-rules.yml`](../../../../.build-test-rules.yml) (workspace root) | Add the new preset to the build matrix for the targets the parent epic already covers. Build-only (no on-target gating). |

## Tests to add (host_test cases)

Each `TEST_CASE` is a separate Catch2 case; group under a common section header so they sort in test output.

### V1 — Layout enumeration (parent §6 V1)

```text
- Get layout pointer L = nand_oob_layout_get_gigadevice_q4xa(); assert non-NULL.
- Iterate L->ops->free_region(NULL, section, &desc) for section = 0..3:
    - section 0: offset == 2,  length == 6, programmable == true,  ecc_protected == true
    - section 1: offset == 16, length == 8, programmable == true,  ecc_protected == true
    - section 2: offset == 32, length == 8, programmable == true,  ecc_protected == true
    - section 3: offset == 48, length == 8, programmable == true,  ecc_protected == true
- L->ops->free_region(NULL, 4, &desc) returns ESP_ERR_NOT_FOUND.
- L->ops->free_region(NULL, 100, &desc) returns ESP_ERR_NOT_FOUND.
- ECC ops: assert L->ops->ecc_region != NULL. Iterate sections 0..3:
    - section 0: offset == 8,  length == 8, programmable == false, ecc_protected == true
    - section 1: offset == 24, length == 8
    - section 2: offset == 40, length == 8
    - section 3: offset == 56, length == 8
- L->ops->ecc_region(NULL, 4, &desc) returns ESP_ERR_NOT_FOUND.
```

### V2 — BBM contract (parent §6 V2)

```text
- L->bbm.bbm_offset       == 0
- L->bbm.bbm_length       == 2
- L->bbm.good_pattern[0]  == 0xFF
- L->bbm.good_pattern[1]  == 0xFF
- L->bbm.check_pages_mask == SPI_NAND_BBM_CHECK_FIRST_PAGE
- L->oob_bytes            == 0
```

### V3 — Selector mapping (parent §6 V3)

```text
- Build a fake spi_nand_flash_device_t on the stack with
  device_info.manufacturer_id = SPI_NAND_FLASH_GIGADEVICE_MI (0xC8) and
  device_info.device_id set to each of {0x31, 0x32, 0x25, 0x35} in turn.
  Assert spi_nand_gigadevice_get_oob_layout(&dev) == nand_oob_layout_get_gigadevice_q4xa().

- For device_id values that are NOT in the Q4xA set (sample: 0x51, 0x41, 0x91, 0x92, 0x95, 0xAA, 0x00, 0xFF):
  Assert spi_nand_gigadevice_get_oob_layout(&dev) == NULL.

- For manufacturer_id != 0xC8 (e.g. 0xEF Winbond, 0x2C Micron) with any device_id:
  Assert spi_nand_gigadevice_get_oob_layout(&dev) == NULL.
```

### V4 — Byte-for-byte compat against default (parent §6 V4)

This is the most important case. It proves the §2.4 invariant: a volume formatted under the default layout reads identically when the Q4xA layout is later selected.

```text
- Phase A (default layout):
    - Initialize a Linux mmap-emulated nand handle for a fake GigaDevice DI = 0x31, but force vendor_get_oob_layout = NULL so the default layout is selected. (Either set the field by hand after init, or use a small test seam.)
    - Mount Dhara on top, write a small known pattern across N pages (N >= 8 to cross intra-block boundaries), unmount.
    - Snapshot the emulator backing file (raw bytes).

- Phase B (Q4xA layout):
    - Re-initialize the handle on the SAME backing file, this time with vendor_get_oob_layout = spi_nand_gigadevice_get_oob_layout. Confirm handle->oob_layout == nand_oob_layout_get_gigadevice_q4xa().
    - Mount Dhara, read the same N pages.
    - Assert every byte read in Phase B matches the pattern written in Phase A.
    - Assert nand_is_free / nand_is_bad return identical answers as Phase A would have for the same pages/blocks.

- Phase C (negative — ensure the test would catch a real corruption):
    - Modify one byte in the backing file at OOB offset 2 of page 0 (the PAGE_USED slot).
    - Re-mount with the Q4xA layout.
    - Assert the change is detected (nand_is_free returns the OPPOSITE of Phase A for page 0).
```

The "test seam" referenced above already exists in some form for the framework epic; if a clean way to override the resolver doesn't exist, **stop and reopen the design** — do not add a global setter just to make this test work. A small `static` test helper inside the host_test file that reaches into the handle struct (under `CONFIG_IDF_TARGET_LINUX`) is acceptable.

### V5 — Parity bound (parent §6 V5)

```text
- Programmatically allocate a fake spi_nand_flash_device_t with handle->oob_layout = Q4xA.
- Run nand_oob_device_layout_init() (the framework function from configurable-oob-layout step 05).
- Inspect handle->oob_cached_regs_free_ecc / oob_cached_reg_count_free_ecc:
    - reg_count == 4
    - sum of regs[i].length == 30
    - regs[0..3].programmable == true
    - regs[0..3].ecc_protected == true
    - none of regs[i] overlaps the ECC parity bytes (8-15, 24-31, 40-47, 56-63).
    - none of regs[i] overlaps the BBM (0-1).
- Confirm handle->oob_cached_reg_count_free_no_ecc == 0 (Q4xA has no FREE_NOECC regions).
```

## test_app preset

`sdkconfig.ci.gigadevice_oob` enables `CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT=y` and **otherwise** mirrors `sdkconfig.ci.oob_layout` (the existing legacy + OOB-on preset from `configurable-oob-layout/step-11-tests-and-ci-matrix.md`). Build-only is the gate; on-target pytest runs only if hardware is available (parent proposal §6 V7).

## Acceptance criteria

- [ ] `host_test/main/test_nand_oob_layout_gigadevice.cpp` exists, covers V1–V5, and follows the Catch2 conventions used by sibling `test_nand_*` files (same TAG style, includes, naming).
- [ ] All V1–V5 cases pass via `pytest` from `host_test/`.
- [ ] `host_test/main/CMakeLists.txt` registers the new file under the same gating used for OOB-layout tests added by the framework parent.
- [ ] `test_app/sdkconfig.ci.gigadevice_oob` builds clean for every target the existing OOB CI presets cover (build-only).
- [ ] `.build-test-rules.yml` is updated so the new preset is included in the build matrix for those targets.
- [ ] **No** test temporarily disables / mocks framework-level invariants from `configurable-oob-layout/step-05` (the cached-region asserts must still hold for the default layout when V3 selects `NULL`).
- [ ] If GD5F1GQ4xA / -2GQ4xA / -4GQ4xA hardware is on the bench: `pytest_spi_nand_flash.py` full pass with `sdkconfig.ci.gigadevice_oob`. Document in the PR which DI was used, the chip part number from datasheet markings, and the lab fixture name.
- [ ] If hardware is **not** available: PR description explicitly states "build-only; on-target deferred until hardware available" — and step 04 logs that as a deferred validation item.

## Risks

- **Phase B re-init reaches into the handle struct.** Mitigated by gating the test seam under `CONFIG_IDF_TARGET_LINUX` and keeping it inside the test file (no production-side code changes).
- **Test flakiness from mmap-emulator stride assumption.** Q4xA uses 64 B spare which already matches `emulated_page_oob = 64` for 2 KiB pages — no stride change. Add a sanity assert at the start of V4 that `dev->chip.emulated_page_oob == 64` so a future stride change fails loudly.
- **CI matrix bloat.** One new preset, build-only — minimal impact. If CI duration drifts, drop targets that already exhaust coverage in `sdkconfig.ci.oob_layout`.

## Notes for implementers

- V4 is by far the most valuable case. If everything else passes but V4 doesn't, the proposal's §2.4 invariant is broken — stop and reopen the design.
- Do **not** add a "select layout via Kconfig" symbol to make V3 easier. The resolver is the only selection mechanism (parent proposal §7.0).
- Keep the file under ~300 LOC. If it balloons, split V4 (full Dhara round-trip) into its own file.
