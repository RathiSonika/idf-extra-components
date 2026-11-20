# SPI NAND Flash Driver

[![Component Registry](https://components.espressif.com/components/espressif/spi_nand_flash/badge.svg)](https://components.espressif.com/components/espressif/spi_nand_flash)

This driver supports SPI NAND Flash devices on ESP chipsets. Wear leveling uses the [dhara](https://github.com/dlbeer/dhara) library via `espressif/dhara` (enabled by default with `CONFIG_NAND_FLASH_ENABLE_WL`).

## Features

- Legacy page API (`spi_nand_flash_init_device`) on ESP-IDF 5.0+
- Optional Block Device Layer (BDL) on ESP-IDF 6.0+ (`CONFIG_NAND_FLASH_ENABLE_BDL`)
- Dhara wear leveling, bad-block handling, and garbage collection
- Optional generic chip detection for lab bring-up
- Linux mmap emulation for host tests

## Add to Your Project

```bash
idf.py add-dependency "espressif/spi_nand_flash"
```

## Documentation

- **Programming Guide & API Reference**: [SPI NAND Flash Documentation](https://espressif.github.io/idf-extra-components/latest/spi_nand_flash/index.html)
- Local sources: [`docs/src/index.md`](docs/src/index.md), [`docs/src/api.md`](docs/src/api.md)
- FatFS helpers / examples: [`spi_nand_flash_fatfs`](../spi_nand_flash_fatfs)

## Supported Manufacturers

Winbond, Gigadevice, Alliance, Micron, Zetta, Zbit, XTX, Fudan Microelectronics (FMSH), Macronix.

See the programming guide for the full part list and voltage-class notes.
