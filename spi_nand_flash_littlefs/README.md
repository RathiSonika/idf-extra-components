# SPI NAND Flash LittleFS Integration

LittleFS integration for SPI NAND Flash using the ESP-IDF Block Device Layer (BDL).

## Requirements

- **ESP-IDF 6.0 or later** (`esp_blockdev` and LittleFS blockdev support)
- **`CONFIG_NAND_FLASH_ENABLE_BDL=y`** in menuconfig (Component config → SPI NAND Flash)
- **`joltwallet/littlefs` >= 1.21.0** (blockdev mount support)
- **`CONFIG_LITTLEFS_CACHE_SIZE` >= NAND page size** (see below)

### LittleFS cache size

`joltwallet/littlefs` allocates per-file buffers at compile time using `CONFIG_LITTLEFS_CACHE_SIZE` (Component config → LittleFS → Cache Size). For SPI NAND blockdev mounts, this value must be **greater than or equal to the NAND page size** exposed by the wear-leveling BDL (typically **2048** or **4096** bytes depending on the chip). If it is too small, mount fails with an error such as `No valid cache_size <= … for block=…`.

Setting **`CONFIG_LITTLEFS_CACHE_SIZE=4096`** works for all SPI NAND parts supported by `spi_nand_flash`. Add it to your project's `sdkconfig.defaults` if needed:

```
CONFIG_LITTLEFS_CACHE_SIZE=4096
```

This component mounts LittleFS on the **wear-leveling** block device from `spi_nand_flash_init_with_layers()`. Do not use the legacy `spi_nand_flash_init_device()` path with this component.

For FAT filesystem support on the legacy API, use [`spi_nand_flash_fatfs`](../spi_nand_flash_fatfs).

## Dependencies

- `spi_nand_flash` (with BDL enabled)
- `joltwallet/littlefs` (>= 1.21.0)
- ESP-IDF `vfs` component

## Usage

```c
#include "spi_nand_flash.h"
#include "esp_nand_blockdev.h"
#include "esp_vfs_littlefs_nand.h"

spi_nand_flash_config_t config = {
    .device_handle = spi,
    .io_mode = SPI_NAND_IO_MODE_SIO,
    .flags = SPI_DEVICE_HALFDUPLEX,
};

esp_blockdev_handle_t wl_bdl;
ESP_ERROR_CHECK(spi_nand_flash_init_with_layers(&config, &wl_bdl));

esp_vfs_littlefs_nand_mount_config_t mount_config = {
    .format_if_mount_failed = true,
    .read_only = false,
};
ESP_ERROR_CHECK(esp_vfs_littlefs_nand_mount("/nandflash", wl_bdl, &mount_config));

FILE *f = fopen("/nandflash/hello.txt", "w");
fprintf(f, "Hello from LittleFS on SPI NAND\n");
fclose(f);

size_t total = 0, used = 0;
esp_vfs_littlefs_nand_info(wl_bdl, &total, &used);

/* Unmount releases wl_bdl via ops->release (deinitializes NAND layers). */
esp_vfs_littlefs_nand_unmount(wl_bdl);
```

## Examples

| Example | Description | BDL | IDF |
|---------|-------------|-----|-----|
| `examples/nand_flash_bdl_littlefs` | LittleFS on NAND via wear-leveling BDL | **Must be on** | 6.0+ |

See the example README for hardware wiring and build steps.

## Architecture

```
Application (fopen, etc.)
        ↓
esp_vfs_littlefs_nand_mount()
        ↓
joltwallet/littlefs (esp_vfs_littlefs_register, blockdev backend)
        ↓
spi_nand_flash wear-leveling BDL (esp_blockdev_t, Dhara FTL)
        ↓
spi_nand_flash raw flash BDL
        ↓
SPI NAND hardware
```

LittleFS uses the wear-leveling BDL in **logical block mode** (`erase_before_write=0`, `default_val_after_erase=1`), which matches the geometry exported by `spi_nand_flash_wl_get_blockdev()`.
