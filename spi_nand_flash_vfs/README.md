# SPI NAND Flash VFS Integration

FATFS and VFS integration layer for the SPI NAND Flash driver.

## Features

- **Legacy API**: FATFS diskio adapter using `spi_nand_flash_device_t` (always available)
- **BDL API**: FATFS diskio adapter using `esp_blockdev_handle_t` (when `CONFIG_NAND_FLASH_ENABLE_BDL=y`, requires ESP-IDF 6.0+)
- VFS mount/unmount helpers for both paths

Functionality is intact with or without BDL: use legacy mount/unmount when BDL is disabled, and BDL mount/unmount when BDL is enabled.

## Dependencies

- `spi_nand_flash` component (pure driver)
- ESP-IDF `fatfs` component
- ESP-IDF `vfs` component

## Usage

### With Legacy API

```c
#include "spi_nand_flash.h"
#include "esp_vfs_fat_nand.h"

// Initialize device
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

### With BDL API

```c
#include "spi_nand_flash.h"
#include "esp_nand_blockdev.h"
#include "esp_vfs_fat_nand.h"

// Initialize with BDL
esp_blockdev_handle_t wl_bdl;
spi_nand_flash_init_with_layers(&config, &wl_bdl);

// Mount FATFS using BDL
esp_vfs_fat_mount_config_t mount_config = {
    .max_files = 4,
    .format_if_mount_failed = true,
};
esp_vfs_fat_nand_mount_bdl("/nand", wl_bdl, &mount_config);

// Use filesystem...

// Unmount
esp_vfs_fat_nand_unmount_bdl("/nand", wl_bdl);
```

## Examples

| Example | Description | BDL | IDF |
|---------|-------------|-----|-----|
| `examples/nand_flash` | FATFS on NAND using legacy API (`spi_nand_flash_init_device` + `esp_vfs_fat_nand_mount`) | Optional (works with BDL on or off) | 5.0+ |
| `examples/nand_flash_bdl` | FATFS on NAND using BDL API (`spi_nand_flash_init_with_layers` + `esp_vfs_fat_nand_mount_bdl`) | Required | 6.0+ |
| `examples/nand_flash_debug_app` | Diagnostic tool (bad blocks, ECC stats, throughput); uses `spi_nand_flash` only, no VFS | N/A | 5.0+ |

See each example’s `README.md` for hardware and usage.
