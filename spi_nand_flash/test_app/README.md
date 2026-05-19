| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |

# SPI NAND Flash on-target tests

Unity tests under `main/` exercise real SPI NAND hardware on the `spi_nand_flash` CI runner.

## Sdkconfig CI presets

| Preset | Purpose |
|--------|---------|
| `sdkconfig.ci.default` | Legacy API, anonymous detect **off** (baseline regression) |
| `sdkconfig.ci.bdl` | Block device layer, anonymous detect **off** |
| `sdkconfig.ci.anonymous` | `CONFIG_NAND_FLASH_ANONYMOUS_DETECT=y`; same pytest suite as default |

CI builds all `sdkconfig.ci.*` files via the repo `idf_build_apps` matrix. On-target pytest (`pytest_spi_nand_flash.py`) runs on hardware for `default`, `bdl`, and `anonymous` when the `spi_nand_flash` runner is available.

## Anonymous detect — hardware validation (ONFI path)

Default GitHub CI may **build** `sdkconfig.ci.anonymous` without running on-target tests if the hardware runner is unavailable. Before merging changes that touch Tier 2 (ONFI) init or parameter-page handling, complete the following on **at least one** ONFI-capable SPI NAND that is **not** in the device table (or whose ID is masked so Tier 1 fails):

1. Enable `CONFIG_NAND_FLASH_ANONYMOUS_DETECT=y` (use `sdkconfig.ci.anonymous` or equivalent).
2. Confirm init succeeds and `spi_nand_get_chip_source()` returns `SPI_NAND_CHIP_SOURCE_ONFI`.
3. Run read/write smoke using existing marker-dependent tests (erase, page program/read, copy).
4. Attach evidence to the PR or release notes: serial log excerpt, chip marking / part number, fixture ID, and date.

**Ownership:** component maintainers or the engineer changing ONFI code. If CI omits the on-target run, the PR must state that manual validation is pending or link completed evidence.

**Lab command (example):**

```bash
cd test_app
idf.py set-target esp32 -DSDKCONFIG_DEFAULTS=sdkconfig.ci.anonymous
idf.py build flash monitor
pytest pytest_spi_nand_flash.py -k anonymous
```
