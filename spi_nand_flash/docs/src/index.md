# SPI NAND Flash Programming Guide

This driver supports SPI NAND Flash devices on ESP chipsets. Wear leveling is provided by [Dhara](https://github.com/dlbeer/dhara) through the `espressif/dhara` component and is enabled by default (`CONFIG_NAND_FLASH_ENABLE_WL`).

## Add the Component

```bash
idf.py add-dependency "espressif/spi_nand_flash"
```

## ESP-IDF Version and API Modes

- **ESP-IDF 5.0–5.x:** Use the legacy API only (`spi_nand_flash_init_device()` and page helpers). BDL is not available.
- **ESP-IDF 6.0+:** Optionally enable `CONFIG_NAND_FLASH_ENABLE_BDL` and use `spi_nand_flash_init_with_layers()` with `esp_blockdev_t`. With BDL disabled, the legacy API behaves as on older IDF versions.

## Wear Leveling (`CONFIG_NAND_FLASH_ENABLE_WL`)

Wear leveling is **on by default**. Disabling it compiles Dhara out and is **not recommended** for general storage or filesystem use unless you provide your own FTL (raw NAND still needs erase-before-program and bad-block handling).

After toggling the option:

```bash
rm -f dependencies.lock
idf.py fullclean reconfigure build
```

`idf.py reconfigure` / `fullclean` alone is not enough: Component Manager keeps solved components in `dependencies.lock`.

## Initialize the SPI Bus and Config

Before calling `spi_nand_flash_init_device()` (legacy) or `spi_nand_flash_init_with_layers()` (BDL), initialize the ESP-IDF SPI bus and pass the resulting `spi_device_handle_t` in `spi_nand_flash_config_t`:

| Field | Description |
|-------|-------------|
| `device_handle` | `spi_device_handle_t` from `spi_bus_add_device()` |
| `io_mode` | `SPI_NAND_IO_MODE_SIO`, `DIO`, `DOUT`, `QIO`, or `QOUT` |
| `flags` | `SPI_DEVICE_HALFDUPLEX` for half-duplex (required for DIO/DOUT/QIO/QOUT); `0` for full-duplex SIO. Must match `spi_device_interface_config_t.flags` |
| `gc_factor` | Optional wear-leveling GC tuning; `0` uses the driver default |

For Zbit parts, keep WP#/IO2 and HOLD#/IO3 high during initialization (`data_io_default_level = true` where available).

## Allocate Device — Legacy API

```c
#include "spi_nand_flash.h"

spi_nand_flash_config_t nand_flash_config = {
    .device_handle = spi,                 // from spi_bus_add_device()
    .io_mode = SPI_NAND_IO_MODE_SIO,
    .flags = SPI_DEVICE_HALFDUPLEX,       // must match spi_device_interface_config_t.flags
    .gc_factor = 0,                       // driver default
};

spi_nand_flash_device_t *nand_handle = NULL;
ESP_ERROR_CHECK(spi_nand_flash_init_device(&nand_flash_config, &nand_handle));
```

> **Note:** When `CONFIG_NAND_FLASH_ENABLE_BDL` is enabled, `spi_nand_flash_init_device()` returns `ESP_ERR_NOT_SUPPORTED`. Use `spi_nand_flash_init_with_layers()` instead.

## Page Read / Write

Public APIs use **page** terminology (`read_page`, `write_page`, `get_page_count`, `get_page_size`). Sector-named helpers remain as compatibility aliases.

```c
uint32_t page_size = 0;
ESP_ERROR_CHECK(spi_nand_flash_get_page_size(nand_handle, &page_size));

uint8_t *buffer = calloc(1, page_size);
assert(buffer);

ESP_ERROR_CHECK(spi_nand_flash_read_page(nand_handle, buffer, 0));
// modify buffer...
ESP_ERROR_CHECK(spi_nand_flash_write_page(nand_handle, buffer, 0));
ESP_ERROR_CHECK(spi_nand_flash_sync(nand_handle));

free(buffer);
```

Call `spi_nand_flash_trim()` when discarding a logical page (for example after file delete/resize). Call `spi_nand_flash_gc()` to run an explicit wear-leveling garbage-collection step during idle time.

## Allocate Device — BDL API (ESP-IDF 6.0+)

Enable `CONFIG_NAND_FLASH_ENABLE_BDL` in menuconfig, then:

```c
#include "spi_nand_flash.h"
#include "esp_blockdev.h"

spi_nand_flash_config_t nand_flash_config = {
    .device_handle = spi,
    .io_mode = SPI_NAND_IO_MODE_SIO,
    .flags = SPI_DEVICE_HALFDUPLEX,
};

esp_blockdev_handle_t wl_bdl = NULL;
ESP_ERROR_CHECK(spi_nand_flash_init_with_layers(&nand_flash_config, &wl_bdl));
```

For raw flash access without the wear-leveling layer, use `nand_flash_get_blockdev()` from `esp_nand_blockdev.h`. Prefer the wear-leveling BDL for filesystem workloads.

See [Layered Architecture](architecture.md) for BDL details and migration from 0.x.

## FatFS Integration

Use the separate [`spi_nand_flash_fatfs`](https://github.com/espressif/idf-extra-components/tree/master/spi_nand_flash_fatfs) component:

1. **Legacy (BDL off):** `spi_nand_flash_init_device()` + `esp_vfs_fat_nand_mount()` / `esp_vfs_fat_nand_unmount()`
2. **BDL + FatFS (ESP-IDF 6.1+):** `spi_nand_flash_init_with_layers()` + `esp_vfs_fat_bdl_mount()` / `esp_vfs_fat_bdl_unmount()`

## Generic Chip Detection

Optional bring-up path for unknown parts. Disabled by default; requires BDL. Not for production FatFS / wear-leveling. See [Generic Chip Detection](generic_detection.md).

## Supported Chips

- Winbond — W25N01GVxxxG/T/R, W25N512GVxIG/IT, W25N512GWxxR/T, W25N01JWxxxG/T, W25N02KVxxIR/U, W25N04KVxxIR/U
- Gigadevice — GD5F1GQ5UExxG, GD5F1GQ5RExxG, GD5F2GQ5UExxG, GD5F2GQ5RExxG, GD5F2GM7xExxG, GD5F4GQ6UExxG, GD5F4GQ6RExxG, GD5F4GM8xExxG, GD5F1GM7xExxG, GD5F4GM7UExxG, GD5F8GM8UExxG
- Alliance — AS5F31G04SND-08LIN, AS5F32G04SND-08LIN, AS5F12G04SND-10LIN, AS5F34G04SND-08LIN, AS5F14G04SND-10LIN, AS5F38G04SND-08LIN, AS5F18G04SND-10LIN
- Micron — MT29F4G01ABAFDWB, MT29F1G01ABAFDSF-AAT:F, MT29F2G01ABAGDWB-IT:G
- Zetta — ZD35Q1GC
- Zbit — ZB35Q01CYIG
- XTX — XT26G08D
- Fudan Microelectronics (FMSH) — FM25S005BI3
- Macronix — MX35LF2GE4AD, MX35LF4GE4AD

**GigaDevice voltage classes:** `UExxG` parts are 2.7–3.6 V (typical ESP32 3.3 V designs). `RExxG` parts are 1.7–2.0 V and need level shifting / a 1.8 V supply.

## FAQ / Troubleshooting

- **Verify writes:** enable `NAND_FLASH_VERIFY_WRITE` under *Component config → SPI NAND Flash configuration*. Failed verify logs an error on the console.
- **Host tests:** Linux mmap emulation is documented in [`host_test/README.md`](https://github.com/espressif/idf-extra-components/tree/master/spi_nand_flash/host_test).
