/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "spi_nand_flash.h"
#include "nand_linux_mmap_emul.h"

#include <catch2/catch_test_macros.hpp>

/*
 * Linux host target: CONFIG_NAND_FLASH_ANONYMOUS_DETECT is not available (Kconfig
 * depends on !IDF_TARGET_LINUX). Anonymous SPI/ONFI init must not change the mmap
 * emulator path — geometry and chip_source stay on the synthetic database profile.
 */

TEST_CASE("Linux emulator init unchanged with anonymous Kconfig unavailable", "[linux_emulator]")
{
    nand_file_mmap_emul_config_t conf = {"", 50 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    spi_nand_flash_device_t *device_handle = nullptr;

    REQUIRE(spi_nand_flash_init_device(&nand_flash_config, &device_handle) == ESP_OK);

    uint32_t page_size = 0;
    uint32_t block_size = 0;
    REQUIRE(spi_nand_flash_get_page_size(device_handle, &page_size) == ESP_OK);
    REQUIRE(spi_nand_flash_get_block_size(device_handle, &block_size) == ESP_OK);
    REQUIRE(page_size == 2048);
    REQUIRE(block_size == 64 * 2048);

    spi_nand_chip_source_t source = SPI_NAND_CHIP_SOURCE_ONFI;
    REQUIRE(spi_nand_get_chip_source(device_handle, &source) == ESP_OK);
    REQUIRE(source == SPI_NAND_CHIP_SOURCE_DATABASE);

    spi_nand_flash_deinit_device(device_handle);
}
