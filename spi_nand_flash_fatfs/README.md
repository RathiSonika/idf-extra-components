# SPI NAND Flash FatFS Integration

FatFS integration layer for the SPI NAND Flash driver.

## Requirements (read first)

This component supports **two** FatFs integration modes; pick one per NAND instance.

### Legacy mode (`spi_nand_flash_device_t`)

- Use **`spi_nand_flash_init_device()`** and **`esp_vfs_fat_nand_mount()`** / **`esp_vfs_fat_nand_unmount()`**.
- Keep **`CONFIG_NAND_FLASH_ENABLE_BDL` disabled**. When BDL is enabled, `spi_nand_flash_init_device()` returns `ESP_ERR_NOT_SUPPORTED`, so this path cannot be used.

### BDL mode (ESP-IDF 6.0+, wear-leveling `esp_blockdev_t`)

- Use **`spi_nand_flash_init_with_layers()`** and **`esp_vfs_fat_bdl_mount()`** / **`esp_vfs_fat_bdl_unmount()`** from ESP-IDF’s `esp_vfs_fat.h`.
- Enable **`CONFIG_NAND_FLASH_ENABLE_BDL`** in menuconfig.
- See **`examples/nand_flash_bdl`** for a full project.

**Migration from 0.x:** See the SPI NAND component’s [layered_architecture.md](../spi_nand_flash/layered_architecture.md) — **Migration Guide (0.x → 1.0.0)** (FATFS split, BDL vs legacy init).

## Features

- FATFS diskio adapter and VFS mount helpers using **`spi_nand_flash_device_t`** (legacy mode)
- FatFs on **BDL** via ESP-IDF **`esp_vfs_fat_bdl_*`** and **`spi_nand_flash_init_with_layers()`** (ESP-IDF 6.0+, `CONFIG_NAND_FLASH_ENABLE_BDL=y`)

## Dependencies

- `spi_nand_flash` component (driver)
- ESP-IDF `fatfs` component
- ESP-IDF `vfs` component

## Usage

```c
#include "spi_nand_flash.h"
#include "esp_vfs_fat_nand.h"

// Initialize device (CONFIG_NAND_FLASH_ENABLE_BDL must be off)
spi_nand_flash_device_t *nand_device;
spi_nand_flash_init_device(&config, &nand_device);

// Mount FATFS
esp_vfs_fat_mount_config_t mount_config = {
    .max_files = 4,
    .format_if_mount_failed = true,
};
esp_vfs_fat_nand_mount("/nand", nand_device, &mount_config);

// Use filesystem...
FILE *f = fopen("/nand/test.txt", "w");
// ...

// Unmount
esp_vfs_fat_nand_unmount("/nand", nand_device);
spi_nand_flash_deinit_device(nand_device);
```

## Examples

| Example | Description | `CONFIG_NAND_FLASH_ENABLE_BDL` | IDF |
|---------|-------------|--------------------------------|-----|
| `examples/nand_flash` | FATFS on NAND (`spi_nand_flash_init_device` + `esp_vfs_fat_nand_mount`) | **Must be off** | 5.0+ |
| `examples/nand_flash_bdl` | FATFS on wear-leveling BDL (`spi_nand_flash_init_with_layers` + `esp_vfs_fat_bdl_mount`) | **Must be on** | 6.0+ |
| `examples/nand_flash_debug_app` | Diagnostics (bad blocks, ECC stats, throughput); `spi_nand_flash` only, no VFS | **Must be off** | 5.0+ |

See each example’s `README.md` for hardware and usage.
