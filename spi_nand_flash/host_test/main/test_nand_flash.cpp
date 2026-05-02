/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "sdkconfig.h"

#include "spi_nand_flash.h"
#include "spi_nand_flash_test_helpers.h"
#include "nand_linux_mmap_emul.h"
#include "nand_private/nand_impl_wrap.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("verify mark_bad_block works", "[spi_nand_flash]")
{
    nand_file_mmap_emul_config_t conf = {"", 50 * 1024 * 1024, true};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    spi_nand_flash_device_t *device_handle;
    REQUIRE(spi_nand_flash_init_device(&nand_flash_config, &device_handle) == ESP_OK);

    uint32_t block_num;
    REQUIRE(spi_nand_flash_get_block_num(device_handle, &block_num) == 0);

    uint32_t test_block = 15;
    REQUIRE((test_block < block_num) == true);

    bool is_bad_status = false;
    // Verify if test_block is not bad block
    REQUIRE(nand_wrap_is_bad(device_handle, test_block, &is_bad_status) == 0);
    REQUIRE(is_bad_status == false);
    // mark test_block as a bad block
    REQUIRE(nand_wrap_mark_bad(device_handle, test_block) == 0);
    // Verify if test_block is marked as bad block
    REQUIRE(nand_wrap_is_bad(device_handle, test_block, &is_bad_status) == 0);
    REQUIRE(is_bad_status == true);

    spi_nand_flash_deinit_device(device_handle);
}

TEST_CASE("verify nand_prog, nand_read, nand_copy, nand_is_free works", "[spi_nand_flash]")
{
    nand_file_mmap_emul_config_t conf = {"", 50 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    spi_nand_flash_device_t *device_handle;
    REQUIRE(spi_nand_flash_init_device(&nand_flash_config, &device_handle) == ESP_OK);

    uint32_t sector_num, sector_size, block_size;
    REQUIRE(spi_nand_flash_get_capacity(device_handle, &sector_num) == 0);
    REQUIRE(spi_nand_flash_get_sector_size(device_handle, &sector_size) == 0);
    REQUIRE(spi_nand_flash_get_block_size(device_handle, &block_size) == 0);

    uint8_t *pattern_buf = (uint8_t *)malloc(sector_size);
    REQUIRE(pattern_buf != NULL);
    uint8_t *temp_buf = (uint8_t *)malloc(sector_size);
    REQUIRE(temp_buf != NULL);

    spi_nand_flash_fill_buffer(pattern_buf, sector_size / sizeof(uint32_t));

    bool is_page_free = true;
    uint32_t test_block = 20;
    uint32_t test_page = test_block * (block_size / sector_size); //(block_num * pages_per_block)
    uint32_t dst_page = test_page + 1;

    REQUIRE((test_page < sector_num) == true);

    // Verify if test_page is free
    REQUIRE(nand_wrap_is_free(device_handle, test_page, &is_page_free) == 0);
    REQUIRE(is_page_free == true);
    // Write/program test_page
    REQUIRE(nand_wrap_prog(device_handle, test_page, pattern_buf) == 0);
    // Verify if test_page is used/programmed
    REQUIRE(nand_wrap_is_free(device_handle, test_page, &is_page_free) == 0);
    REQUIRE(is_page_free == false);

    REQUIRE(nand_wrap_read(device_handle, test_page, 0, sector_size, temp_buf) == 0);
    REQUIRE(spi_nand_flash_check_buffer(temp_buf, sector_size / sizeof(uint32_t)) == 0);

    REQUIRE(nand_wrap_copy(device_handle, test_page, dst_page) == 0);

    REQUIRE(nand_wrap_read(device_handle, dst_page, 0, sector_size, temp_buf) == 0);
    REQUIRE(spi_nand_flash_check_buffer(temp_buf, sector_size / sizeof(uint32_t)) == 0);

    free(pattern_buf);
    free(temp_buf);
    spi_nand_flash_deinit_device(device_handle);
}

#ifdef CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT

TEST_CASE("experimental OOB layout: nand_wrap cross-block copy marks dst used", "[spi_nand_flash][oob_layout]")
{
    nand_file_mmap_emul_config_t conf = {"", 50 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    spi_nand_flash_device_t *dev = nullptr;
    REQUIRE(spi_nand_flash_init_device(&nand_flash_config, &dev) == ESP_OK);

    uint32_t sector_size = 0;
    uint32_t block_size = 0;
    uint32_t sector_num = 0;
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sector_size) == ESP_OK);
    REQUIRE(spi_nand_flash_get_block_size(dev, &block_size) == ESP_OK);
    REQUIRE(spi_nand_flash_get_capacity(dev, &sector_num) == ESP_OK);

    uint32_t ppb = block_size / sector_size;
    uint32_t src_page = 9 * ppb;
    uint32_t dst_page = 10 * ppb;
    REQUIRE(dst_page < sector_num);

    REQUIRE(nand_wrap_erase_block(dev, 9) == ESP_OK);
    REQUIRE(nand_wrap_erase_block(dev, 10) == ESP_OK);

    uint8_t *pattern_buf = (uint8_t *)malloc(sector_size);
    uint8_t *temp_buf = (uint8_t *)malloc(sector_size);
    REQUIRE(pattern_buf != nullptr);
    REQUIRE(temp_buf != nullptr);
    spi_nand_flash_fill_buffer(pattern_buf, sector_size / sizeof(uint32_t));

    REQUIRE(nand_wrap_prog(dev, src_page, pattern_buf) == ESP_OK);
    REQUIRE(nand_wrap_copy(dev, src_page, dst_page) == ESP_OK);

    REQUIRE(nand_wrap_read(dev, dst_page, 0, sector_size, temp_buf) == ESP_OK);
    REQUIRE(spi_nand_flash_check_buffer(temp_buf, sector_size / sizeof(uint32_t)) == 0);

    bool used = true;
    REQUIRE(nand_wrap_is_free(dev, dst_page, &used) == ESP_OK);
    REQUIRE(used == false);

    free(pattern_buf);
    free(temp_buf);
    spi_nand_flash_deinit_device(dev);
}

TEST_CASE("experimental OOB layout: neighbor physical page stays free after nand_wrap prog", "[spi_nand_flash][oob_layout]")
{
    nand_file_mmap_emul_config_t conf = {"", 50 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    spi_nand_flash_device_t *dev = nullptr;
    REQUIRE(spi_nand_flash_init_device(&nand_flash_config, &dev) == ESP_OK);

    uint32_t sector_size = 0;
    uint32_t block_size = 0;
    uint32_t sector_num = 0;
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sector_size) == ESP_OK);
    REQUIRE(spi_nand_flash_get_block_size(dev, &block_size) == ESP_OK);
    REQUIRE(spi_nand_flash_get_capacity(dev, &sector_num) == ESP_OK);

    uint32_t ppb = block_size / sector_size;
    REQUIRE(ppb >= 2);

    uint32_t blk = 14;
    REQUIRE(nand_wrap_erase_block(dev, blk) == ESP_OK);

    uint32_t p0 = blk * ppb;
    uint32_t p1 = p0 + 1;
    REQUIRE(p1 < sector_num);

    bool f = false;
    REQUIRE(nand_wrap_is_free(dev, p1, &f) == ESP_OK);
    REQUIRE(f == true);

    uint8_t *buf = (uint8_t *)malloc(sector_size);
    REQUIRE(buf != nullptr);
    spi_nand_flash_fill_buffer(buf, sector_size / sizeof(uint32_t));
    REQUIRE(nand_wrap_prog(dev, p0, buf) == ESP_OK);

    REQUIRE(nand_wrap_is_free(dev, p0, &f) == ESP_OK);
    REQUIRE(f == false);
    REQUIRE(nand_wrap_is_free(dev, p1, &f) == ESP_OK);
    REQUIRE(f == true);

    free(buf);
    spi_nand_flash_deinit_device(dev);
}

#endif /* CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT */
