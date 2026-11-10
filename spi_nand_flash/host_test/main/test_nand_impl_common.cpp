/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "spi_nand_flash.h"
#include "spi_nand_flash_test_helpers.h"
#include "nand_linux_mmap_emul.h"
#include "nand_private/nand_impl_wrap.h"
#include "nand.h"
#include "nand_impl_common.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("init syncs plane geometry and installs die-select no-op", "[spi_nand_flash][impl_common]")
{
    nand_file_mmap_emul_config_t conf = {"", 50 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    spi_nand_flash_device_t *device_handle = nullptr;
    REQUIRE(spi_nand_flash_init_device(&nand_flash_config, &device_handle) == ESP_OK);

    REQUIRE(device_handle->chip.num_dies == 1);
    REQUIRE(device_handle->chip.num_luns_per_die == 1);
    REQUIRE(device_handle->chip.num_planes == 1);
    REQUIRE(device_handle->chip.num_planes_per_lun == device_handle->chip.num_planes);
    REQUIRE(device_handle->chip.num_planes_per_lun != 0);
    REQUIRE(device_handle->select_die != nullptr);
    REQUIRE(nand_has_explicit_die_select(device_handle) == false);

    spi_nand_flash_deinit_device(device_handle);
}

TEST_CASE("cross-die nand_copy succeeds with address-folded num_dies=2", "[spi_nand_flash][impl_common]")
{
    /* 16 MiB mmap image yields an even block count with default 2 KiB page + 64 B OOB. */
    nand_file_mmap_emul_config_t conf = {"", 16 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    spi_nand_flash_device_t *device_handle = nullptr;
    REQUIRE(spi_nand_flash_init_device(&nand_flash_config, &device_handle) == ESP_OK);

    REQUIRE((device_handle->chip.num_blocks % 2U) == 0);
    device_handle->chip.num_dies = 2;
    REQUIRE(nand_validate_chip_geometry(device_handle) == ESP_OK);

    uint32_t sector_size = 0;
    uint32_t block_size = 0;
    REQUIRE(spi_nand_flash_get_sector_size(device_handle, &sector_size) == ESP_OK);
    REQUIRE(spi_nand_flash_get_block_size(device_handle, &block_size) == ESP_OK);

    uint8_t *pattern_buf = (uint8_t *)malloc(sector_size);
    uint8_t *temp_buf = (uint8_t *)malloc(sector_size);
    REQUIRE(pattern_buf != nullptr);
    REQUIRE(temp_buf != nullptr);
    spi_nand_flash_fill_buffer(pattern_buf, sector_size / sizeof(uint32_t));

    const uint32_t pages_per_block = block_size / sector_size;
    const uint32_t blocks_per_die = device_handle->chip.num_blocks / device_handle->chip.num_dies;
    const uint32_t src_page = 10 * pages_per_block;
    const uint32_t dst_page = blocks_per_die * pages_per_block; /* first page of die 1 */

    REQUIRE(nand_wrap_prog(device_handle, src_page, pattern_buf) == ESP_OK);
    REQUIRE(nand_wrap_copy(device_handle, src_page, dst_page) == ESP_OK);
    REQUIRE(nand_wrap_read(device_handle, dst_page, 0, sector_size, temp_buf) == ESP_OK);
    REQUIRE(spi_nand_flash_check_buffer(temp_buf, sector_size / sizeof(uint32_t)) == 0);

    free(pattern_buf);
    free(temp_buf);
    spi_nand_flash_deinit_device(device_handle);
}
