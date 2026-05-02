| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |

# SPI NAND flash test application

On-target Unity tests are driven by `pytest_spi_nand_flash.py` (hardware required). CI builds several **sdkconfig.ci.\*** presets via `idf-build-apps` (see repository root `.idf_build_apps.toml`).

## CI sdkconfig presets (ESP32)

| Preset (`config` / build suffix) | sdkconfig source | Role |
| -------------------------------- | ---------------- | ---- |
| `default` | `sdkconfig.defaults` only | Legacy mode, `CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT` off |
| `oob_layout` | `sdkconfig.ci.oob_layout` | Same as legacy plus experimental OOB layout enabled |
| `bdl` | `sdkconfig.ci.bdl` | BDL enabled (IDF ≥ 6.0), experimental OOB off |
| `bdl_oob_layout` | `sdkconfig.ci.bdl_oob_layout` | BDL plus experimental OOB layout |

## Reproduce CI builds locally (ESP32)

With ESP-IDF exported and from this directory:

```bash
idf.py set-target esp32
# Legacy (matches CI `default`)
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults" -B build_esp32_default build
# Experimental OOB (matches CI `oob_layout`)
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.ci.oob_layout" -B build_esp32_oob_layout build
# BDL (matches CI `bdl`)
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.ci.bdl" -B build_esp32_bdl build
# BDL + experimental OOB (matches CI `bdl_oob_layout`)
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.ci.bdl_oob_layout" -B build_esp32_bdl_oob_layout build
```

Adjust `SDKCONFIG_DEFAULTS` quoting for your shell if needed.

## Run pytest on hardware (full suite)

Connect the board referenced by CI (`spi_nand_flash` runner). From the **repository root** (parent of `spi_nand_flash/`), after building all needed configs:

```bash
pytest spi_nand_flash/test_app/pytest_spi_nand_flash.py \
  --target esp32 -m spi_nand_flash \
  --build-dir=build_esp32
```

Each parametrized case selects `build_esp32_<config>` (see root `conftest.py`). Build each suffix under `spi_nand_flash/test_app/` as above, or use `idf-build-apps` like CI.
