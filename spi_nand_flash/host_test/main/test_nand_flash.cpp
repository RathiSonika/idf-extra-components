/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "ff.h"
#include "esp_partition.h"
#include "diskio_impl.h"
#include "diskio_nand.h"
#include "spi_nand_flash.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Create volume, open file, write and read back data", "[fatfs, spi_nand_flash]")
{
    FRESULT fr_result;
    BYTE pdrv;
    FATFS fs;
    FIL file;
    UINT bw;

    esp_err_t esp_result;
    spi_nand_flash_config_t nand_flash_config;
    spi_nand_flash_device_t *device_handle;
    REQUIRE(spi_nand_flash_init_device(&nand_flash_config, &device_handle) == ESP_OK);

    // Get a physical drive
    esp_result = ff_diskio_get_drive(&pdrv);
    REQUIRE(esp_result == ESP_OK);

    // Register physical drive as wear-levelled partition
    esp_result = ff_diskio_register_nand(pdrv, device_handle);

    // Create FAT volume on the entire disk
    LBA_t part_list[] = {100, 0, 0, 0};
    BYTE work_area[FF_MAX_SS];

    fr_result = f_fdisk(pdrv, part_list, work_area);
    REQUIRE(fr_result == FR_OK);

    char drv[3] = {(char)('0' + pdrv), ':', 0};
    const MKFS_PARM opt = {(BYTE)(FM_ANY), 0, 0, 0, 0};
    fr_result = f_mkfs(drv, &opt, work_area, sizeof(work_area)); // Use default volume
    REQUIRE(fr_result == FR_OK);

    // Mount the volume
    fr_result = f_mount(&fs, drv, 0);
    REQUIRE(fr_result == FR_OK);

    // Open, write and read data
    fr_result = f_open(&file, "0:/test.txt", FA_OPEN_ALWAYS | FA_READ | FA_WRITE);
    REQUIRE(fr_result == FR_OK);

    // Generate data
    uint32_t data_size = 1000;

    char *data = (char *) malloc(data_size);
    char *read = (char *) malloc(data_size);

    for (uint32_t i = 0; i < data_size; i += sizeof(i)) {
        *((uint32_t *)(data + i)) = i;
    }

    // Write generated data
    fr_result = f_write(&file, data, data_size, &bw);
    REQUIRE(fr_result == FR_OK);
    REQUIRE(bw == data_size);

    // Move to beginning of file
    fr_result = f_lseek(&file, 0);
    REQUIRE(fr_result == FR_OK);

    // Read written data
    fr_result = f_read(&file, read, data_size, &bw);
    REQUIRE(fr_result == FR_OK);
    REQUIRE(bw == data_size);

    REQUIRE(memcmp(data, read, data_size) == 0);

    // Close file
    fr_result = f_close(&file);
    REQUIRE(fr_result == FR_OK);

    // Unmount default volume
    fr_result = f_mount(0, drv, 0);
    REQUIRE(fr_result == FR_OK);

    // Clear
    free(read);
    free(data);
    ff_diskio_unregister(pdrv);
    ff_diskio_clear_pdrv_nand(device_handle);
    spi_nand_flash_deinit_device(device_handle);
}
