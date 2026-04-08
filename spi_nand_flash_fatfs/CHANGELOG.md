## [1.1.0]

### Added
- Example **`examples/nand_flash_bdl`**: FatFs on SPI NAND using **`spi_nand_flash_init_with_layers()`**, **`CONFIG_NAND_FLASH_ENABLE_BDL`**, and ESP-IDF **`esp_vfs_fat_bdl_mount()`** (ESP-IDF 6.0+).

### Documentation
- README updated: document **legacy** vs **BDL** FatFs modes and add the new example to the examples table. Removed outdated note that FatFs on `esp_blockdev_t` would be added only in a future release.

## [1.0.0]

### Breaking Changes
- FATFS integration for SPI NAND Flash now lives in this component. Projects that previously relied on FATFS support bundled inside `spi_nand_flash` must add `spi_nand_flash_fatfs` as a dependency and include its headers.

**Migration:** See **Migration Guide (0.x → 1.0.0)** in [`spi_nand_flash/layered_architecture.md`](../spi_nand_flash/layered_architecture.md) (FATFS split, legacy init with BDL disabled, and related driver changes).
