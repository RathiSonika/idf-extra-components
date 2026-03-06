/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "esp_system.h"
#include "soc/spi_pins.h"
#include "esp_vfs_fat_nand.h"
#include "esp_nand_blockdev.h"
#include "esp_nand_partition.h"

#define EXAMPLE_FLASH_FREQ_KHZ      40000

static const char *TAG = "example_bdl";

// Pin mapping
// ESP32 (VSPI)
#ifdef CONFIG_IDF_TARGET_ESP32
#define HOST_ID  SPI3_HOST
#define PIN_MOSI SPI3_IOMUX_PIN_NUM_MOSI
#define PIN_MISO SPI3_IOMUX_PIN_NUM_MISO
#define PIN_CLK  SPI3_IOMUX_PIN_NUM_CLK
#define PIN_CS   SPI3_IOMUX_PIN_NUM_CS
#define PIN_WP   SPI3_IOMUX_PIN_NUM_WP
#define PIN_HD   SPI3_IOMUX_PIN_NUM_HD
#define SPI_DMA_CHAN SPI_DMA_CH_AUTO
#else // Other chips (SPI2/HSPI)
#define HOST_ID  SPI2_HOST
#define PIN_MOSI SPI2_IOMUX_PIN_NUM_MOSI
#define PIN_MISO SPI2_IOMUX_PIN_NUM_MISO
#define PIN_CLK  SPI2_IOMUX_PIN_NUM_CLK
#define PIN_CS   SPI2_IOMUX_PIN_NUM_CS
#define PIN_WP   SPI2_IOMUX_PIN_NUM_WP
#define PIN_HD   SPI2_IOMUX_PIN_NUM_HD
#define SPI_DMA_CHAN SPI_DMA_CH_AUTO
#endif

static const char *mount_part1 = "/part1";
static const char *mount_part2 = "/part2";

static esp_err_t test_partition_io_write(const char *mount_path, const char *partition_name)
{
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "%s/test_%s.txt", mount_path, partition_name);

    ESP_LOGI(TAG, "[%s] Writing file: %s", partition_name, filepath);
    FILE *f = fopen(filepath, "wb");
    if (!f) {
        ESP_LOGE(TAG, "[%s] Failed to open file for writing", partition_name);
        return ESP_FAIL;
    }
    fprintf(f, "Hello from partition '%s' (IDF %s)\n", partition_name, esp_get_idf_version());
    fclose(f);

    return ESP_OK;
}
static esp_err_t test_partition_io_read(const char *mount_path, const char *partition_name)
{
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "%s/test_%s.txt", mount_path, partition_name);

    ESP_LOGI(TAG, "[%s] Reading file back", partition_name);
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        ESP_LOGE(TAG, "[%s] Failed to open file for reading", partition_name);
        return ESP_FAIL;
    }
    char line[128];
    fgets(line, sizeof(line), f);
    fclose(f);

    char *pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "[%s] Read: '%s'", partition_name, line);

    return ESP_OK;
}

void app_main(void)
{
    esp_err_t ret;
    spi_device_handle_t spi = NULL;
    esp_blockdev_handle_t wl_bdl = NULL;
    esp_blockdev_handle_t part1_bdl = NULL;
    esp_blockdev_handle_t part2_bdl = NULL;
    bool part1_mounted = false;
    bool part2_mounted = false;

    // Initialize SPI bus
    const spi_bus_config_t bus_config = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_CLK,
        .quadhd_io_num = PIN_HD,
        .quadwp_io_num = PIN_WP,
        .max_transfer_sz = 4096 * 2,
    };

    ESP_LOGI(TAG, "DMA CHANNEL: %d", SPI_DMA_CHAN);
    ESP_ERROR_CHECK(spi_bus_initialize(HOST_ID, &bus_config, SPI_DMA_CHAN));

    const uint32_t spi_flags = SPI_DEVICE_HALFDUPLEX;
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = EXAMPLE_FLASH_FREQ_KHZ * 1000,
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 10,
        .flags = spi_flags,
    };

    ESP_ERROR_CHECK(spi_bus_add_device(HOST_ID, &devcfg, &spi));

    // Create Flash + WL block device stack
    spi_nand_flash_config_t config = {
        .device_handle = spi,
        .io_mode = SPI_NAND_IO_MODE_SIO,
        .flags = spi_flags,
        .gc_factor = 4,
    };

    ret = spi_nand_flash_init_with_layers(&config, &wl_bdl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create BDL: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    // Calculate partition sizes from actual disk geometry
    uint64_t disk_size = wl_bdl->geometry.disk_size;
    size_t erase_size = wl_bdl->geometry.erase_size;

    ESP_LOGI(TAG, "WL disk: %" PRIu64 " bytes, erase_size: %zu",
             disk_size, erase_size);

    // Register two partitions on the WL block device
    spi_nand_partition_config_t partitions[] = {
        {
            .name = "data1",
            .offset = 0,
            .size = 100 * 1024 * 1024,
        },
        {
            .name = "data2",
            .offset = 120 * 1024 * 1024,
            .size =  8 * 1024 * 1024,
        },
    };

    ret = spi_nand_flash_register_partitions(wl_bdl, partitions, 2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register partitions: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    // Obtain partition handles from the driver
    ret = spi_nand_flash_get_partition(wl_bdl, "data1", &part1_bdl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get partition 'data1': %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ret = spi_nand_flash_get_partition(wl_bdl, "data2", &part2_bdl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get partition 'data2': %s", esp_err_to_name(ret));
        goto cleanup;
    }

    // Mount FAT on each partition independently
    esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
        .allocation_unit_size = 16 * 1024,
    };

    ESP_LOGI(TAG, "Mounting partition 'data1' at %s", mount_part1);
    ret = esp_vfs_fat_nand_mount_bdl(mount_part1, part1_bdl, &mount_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount 'data1': %s", esp_err_to_name(ret));
        goto cleanup;
    }
    part1_mounted = true;
    ESP_LOGI(TAG, "Mounted partition 'data1' at %s", mount_part1);

    ESP_LOGI(TAG, "Mounting partition 'data2' at %s", mount_part2);
    ret = esp_vfs_fat_nand_mount_bdl(mount_part2, part2_bdl, &mount_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount 'data2': %s", esp_err_to_name(ret));
        goto cleanup_fs;
    }
    part2_mounted = true;
    ESP_LOGI(TAG, "Mounted partition 'data2' at %s", mount_part2);

    // Print filesystem info for both partitions
    uint64_t total, free_space;
    esp_vfs_fat_info(mount_part1, &total, &free_space);
    ESP_LOGI(TAG, "'data1': %" PRIu64 " kB total, %" PRIu64 " kB free", total / 1024, free_space / 1024);

    esp_vfs_fat_info(mount_part2, &total, &free_space);
    ESP_LOGI(TAG, "'data2': %" PRIu64 " kB total, %" PRIu64 " kB free", total / 1024, free_space / 1024);

    // Perform independent read/write on each partition
    ret = test_partition_io_write(mount_part1, "data1");
    if (ret != ESP_OK) {
        goto cleanup_fs;
    }

    ret = test_partition_io_write(mount_part2, "data2");
    if (ret != ESP_OK) {
        goto cleanup_fs;
    }

    ret = test_partition_io_read(mount_part1, "data1");
    if (ret != ESP_OK) {
        goto cleanup_fs;
    }

    ret = test_partition_io_read(mount_part2, "data2");
    if (ret != ESP_OK) {
        goto cleanup_fs;
    }
    // Verify isolation: files on one partition must not be visible on the other
    ESP_LOGI(TAG, "Verifying partition isolation...");
    FILE *f = fopen("/part2/test_data1.txt", "rb");
    if (f) {
        fclose(f);
        ESP_LOGE(TAG, "ISOLATION FAILURE: data1's file found on partition data2");
    } else {
        ESP_LOGI(TAG, "OK: data1's file is not accessible from partition data2");
    }

    f = fopen("/part1/test_data2.txt", "rb");
    if (f) {
        fclose(f);
        ESP_LOGE(TAG, "ISOLATION FAILURE: data2's file found on partition data1");
    } else {
        ESP_LOGI(TAG, "OK: data2's file is not accessible from partition data1");
    }

    // Final filesystem info
    esp_vfs_fat_info(mount_part1, &total, &free_space);
    ESP_LOGI(TAG, "'data1' final: %" PRIu64 " kB total, %" PRIu64 " kB free", total / 1024, free_space / 1024);

    esp_vfs_fat_info(mount_part2, &total, &free_space);
    ESP_LOGI(TAG, "'data2' final: %" PRIu64 " kB total, %" PRIu64 " kB free", total / 1024, free_space / 1024);

cleanup_fs:
    if (part2_mounted) {
        esp_vfs_fat_nand_unmount_bdl(mount_part2, part2_bdl);
    }
    if (part1_mounted) {
        esp_vfs_fat_nand_unmount_bdl(mount_part1, part1_bdl);
    }

cleanup:
    if (wl_bdl) {
        if (spi_nand_flash_release_partitions(wl_bdl) == ESP_ERR_NOT_FOUND) {
            wl_bdl->ops->release(wl_bdl);
        }
    }
    if (spi) {
        ESP_ERROR_CHECK(spi_bus_remove_device(spi));
    }
    ESP_ERROR_CHECK(spi_bus_free(HOST_ID));
}
