| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |

# SPI NAND Flash LittleFS Example (Block Device Layer)

This example mounts LittleFS on SPI NAND Flash using the wear-leveling block device (`esp_blockdev_t`) and **`joltwallet/littlefs` blockdev APIs** (`esp_vfs_littlefs_register` with `.blockdev`).

## Requirements

- ESP-IDF **6.0 or later** (CMake warns and fails configure on older IDF)
- **`CONFIG_NAND_FLASH_ENABLE_BDL=y`** (enabled by default via `sdkconfig.defaults`)
- **`joltwallet/littlefs` >= 1.21.0** (blockdev support)
- **`CONFIG_LITTLEFS_CACHE_SIZE` >= NAND page size** (example defaults to **4096**)
- External SPI NAND Flash chip

## Configuration

- `CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED` — format LittleFS if the first mount fails
- Keep **`CONFIG_NAND_FLASH_ENABLE_BDL` enabled** (Component config → SPI NAND Flash)
- Set LittleFS cache size in menuconfig (Component config → LittleFS → Cache Size), or via `sdkconfig.defaults`

## Hardware Required

Same wiring as the FATFS example in `spi_nand_flash_fatfs/examples/nand_flash`:

* For ESP32 (SPI3): MOSI 23, MISO 19, CLK 18, CS 5, WP 22, HD 21
* For other ESP chips (SPI2): MOSI 13, MISO 12, CLK 14, CS 15, WP 2, HD 4

## How to Use Example

```bash
cd examples/nand_flash_bdl_littlefs
idf.py -p PORT flash monitor
```

The example:

1. Initializes SPI and the NAND wear-leveling BDL via `spi_nand_flash_init_with_layers()`
2. Mounts LittleFS with `esp_vfs_littlefs_register()` (`.blockdev = wl_bdl`)
3. Writes and reads `hello.txt`
4. Unmounts with `esp_vfs_littlefs_unregister_blockdev()` (releases the BDL and NAND layers)

## Example Output

```
I (315) example: DMA CHANNEL: 3
I (355) esp_littlefs: Initializing LittleFS
I (6655) example: LittleFS: 114240 kB total, 8 kB used
I (6655) example: Opening file
I (6685) example: File written
I (6685) example: Reading file
I (6685) example: Read from file: 'Written using ESP-IDF v6.0-...'
```
