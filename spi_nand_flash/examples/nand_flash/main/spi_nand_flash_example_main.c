/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#include "esp_system.h"
#include "soc/spi_pins.h"
#include "esp_vfs_fat_nand.h"
#include "spi_nand_flash.h"
#include "ff.h"
#include "diskio_impl.h"
#include "diskio_nand.h"

#define EXAMPLE_FLASH_FREQ_KHZ      20000

static const char *TAG = "example";

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

// Mount path for the partition
const char *base_path = "/nandflash";

static void example_init_nand_flash(spi_nand_flash_device_t **out_handle, spi_device_handle_t *spi_handle)
{
    const spi_bus_config_t bus_config = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_CLK,
        .quadhd_io_num = PIN_HD,
        .quadwp_io_num = PIN_WP,
        .max_transfer_sz = 4096 * 2,
    };

    // Initialize the SPI bus
    ESP_LOGI(TAG, "DMA CHANNEL: %d", SPI_DMA_CHAN);
    ESP_ERROR_CHECK(spi_bus_initialize(HOST_ID, &bus_config, SPI_DMA_CHAN));

    // spi_flags = SPI_DEVICE_HALFDUPLEX -> half duplex
    // spi_flags = 0 -> full_duplex
    const uint32_t spi_flags = SPI_DEVICE_HALFDUPLEX;

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = EXAMPLE_FLASH_FREQ_KHZ * 1000,
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 10,
        .flags = spi_flags,
    };

    spi_device_handle_t spi;
    ESP_ERROR_CHECK(spi_bus_add_device(HOST_ID, &devcfg, &spi));

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .io_mode = SPI_NAND_IO_MODE_SIO,
        .flags = spi_flags,
    };
    assert(devcfg.flags == nand_flash_config.flags);
    spi_nand_flash_device_t *nand_flash_device_handle;
    ESP_ERROR_CHECK(spi_nand_flash_init_device(&nand_flash_config, &nand_flash_device_handle));

    *out_handle = nand_flash_device_handle;
    *spi_handle = spi;
}

static void example_deinit_nand_flash(spi_nand_flash_device_t *flash, spi_device_handle_t spi)
{
    ESP_ERROR_CHECK(spi_nand_flash_deinit_device(flash));
    ESP_ERROR_CHECK(spi_bus_remove_device(spi));
    ESP_ERROR_CHECK(spi_bus_free(HOST_ID));
}

static int s_count = 0;
static unsigned char s_data[10 * 1024];

void max_storage_test_with_subdirs()
{
    int ret = -1;
    int folder_num = 0;
    int files_per_folder = 500;  // Stay under 512 per folder for safety

    do {
        // Create a new folder every 500 files
        if (s_count % files_per_folder == 0) {
            folder_num = s_count / files_per_folder;
            char folder_path[32];
            snprintf(folder_path, sizeof(folder_path), "/nandflash/dir_%d", folder_num);

            // Create directory
            struct stat st = {0};
            if (stat(folder_path, &st) == -1) {
                ESP_LOGI(TAG, "Creating directory: %s", folder_path);
                if (mkdir(folder_path, 0755) != 0) {
                    ESP_LOGE(TAG, "Failed to create directory %s: errno=%d (%s)",
                             folder_path, errno, strerror(errno));
                    break;  // Stop if we can't create directory
                } else {
                    ESP_LOGI(TAG, "✓ Directory created successfully");
                }
            } else {
                ESP_LOGI(TAG, "Directory %s already exists", folder_path);
            }
        }

        // Create file in subdirectory
        ESP_LOGI(TAG, "Opening file %d", ++s_count);
        char fileName[128] = {0};
        sprintf(fileName, "/nandflash/dir_%d/%05d.jpg",
                folder_num, s_count);

        FILE *f = fopen(fileName, "wb");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file %s for writing (errno=%d)",
                     fileName, errno);

            // Print current space
            uint64_t bytes_total, bytes_free;
            esp_vfs_fat_info(base_path, &bytes_total, &bytes_free);
            ESP_LOGE(TAG, "Space: %.2f MB free of %.2f MB total",
                     bytes_free / (1024.0 * 1024.0),
                     bytes_total / (1024.0 * 1024.0));
            break;  // Stop on error
        }
        ESP_LOGI(TAG, "Success to open file %s for writing", fileName);
        ret = fwrite(s_data, sizeof(s_data), 1, f);
        fclose(f);
        ESP_LOGI(TAG, "File written ret:%d", ret);

        // Print FAT FS size information
        uint64_t bytes_total, bytes_free;
        esp_vfs_fat_info(base_path, &bytes_total, &bytes_free);
        ESP_LOGI(TAG, "FAT FS: %" PRIu64 " kB total, %" PRIu64 " kB free",
                 bytes_total / 1024, bytes_free / 1024);

    } while (true);
}

void max_storage_test()
{
    int ret = -1;
    do {
        // Create a file in FAT FS
        ESP_LOGI(TAG, "Opening file %d", ++s_count);
        char fileName[128] = {0};
        sprintf(fileName, "/nandflash/%05d.jpg", s_count);
        FILE *f = fopen(fileName, "wb");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file %s for writing", fileName);
            continue;
        }
        ESP_LOGI(TAG, "Success to open file %s for writing", fileName);
        ret = fwrite(s_data, sizeof(s_data), 1, f);
        fclose(f);
        ESP_LOGI(TAG, "File written ret:%d", ret);

        // Print FAT FS size information
        uint64_t bytes_total, bytes_free;
        esp_vfs_fat_info(base_path, &bytes_total, &bytes_free);
        ESP_LOGI(TAG, "FAT FS: %" PRIu64 " kB total, %" PRIu64 " kB free", bytes_total / 1024, bytes_free / 1024);
    } while (true);
}

void print_filesystem_diagnostics(const char *base_path)
{
    FATFS *fs;
    DWORD free_clusters;
    char drv[3] = {'0', ':', 0};

    FRESULT res = f_getfree(drv, &free_clusters, &fs);
    if (res != FR_OK) {
        ESP_LOGE(TAG, "f_getfree failed: %d", res);
        return;
    }

    ESP_LOGI(TAG, "========================================");

    // Filesystem type
    const char *fs_type;
    bool is_problem = false;

    if (fs->fs_type == FS_FAT12) {
        fs_type = "FAT12";
        is_problem = true;
    } else if (fs->fs_type == FS_FAT16) {
        fs_type = "FAT16 *** PROBLEM - MAX 512 FILES! ***";
        is_problem = true;
    } else if (fs->fs_type == FS_FAT32) {
        fs_type = "FAT32 ✓ CORRECT!";
    } else if (fs->fs_type == FS_EXFAT) {
        fs_type = "exFAT ✓ CORRECT!";
    } else {
        fs_type = "UNKNOWN";
    }

    ESP_LOGI(TAG, "Filesystem Type: %s", fs_type);
    ESP_LOGI(TAG, "Sector Size: %d bytes", fs->ssize);
    ESP_LOGI(TAG, "Cluster Size: %d sectors = %d bytes",
             fs->csize, fs->csize * fs->ssize);

    if (is_problem) {
        ESP_LOGW(TAG, "*** ROOT DIRECTORY LIMIT: 512 files ***");
        ESP_LOGW(TAG, "*** MAX FILE SIZE: ~8 MB per file ***");
        ESP_LOGW(TAG, "*** SOLUTION: Reformat as FAT32! ***");
    } else {
        ESP_LOGI(TAG, "✓ Root directory: Unlimited files");
        ESP_LOGI(TAG, "✓ Max file size: 4 GB");
    }

    uint64_t total_bytes = (uint64_t)(fs->n_fatent - 2) * fs->csize * fs->ssize;
    ESP_LOGI(TAG, "Total Space: %.2f MB", total_bytes / (1024.0 * 1024.0));
    ESP_LOGI(TAG, "========================================");
}

void app_main(void)
{
    esp_err_t ret;
    // Set up SPI bus and initialize the external SPI Flash chip
    spi_device_handle_t spi;
    spi_nand_flash_device_t *flash;
    example_init_nand_flash(&flash, &spi);
    if (flash == NULL) {
        return;
    }

    // *** ERASE FLASH ONCE TO START FRESH (COMMENT OUT AFTER FIRST RUN) ***
    ESP_LOGW(TAG, "Erasing entire chip...");
    ESP_ERROR_CHECK(spi_nand_erase_chip(flash));
    ESP_LOGI(TAG, "Chip erased successfully!");
    // *** END OF ONE-TIME CODE ***

    esp_vfs_fat_mount_config_t config = {
        .max_files = 10,
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = true,
#endif
        .allocation_unit_size = 2 * 1024  // 16KB clusters for FAT32
    };

    ESP_LOGI(TAG, "Attempting to mount filesystem...");
    ret = esp_vfs_fat_nand_mount(base_path, flash, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Mount failed with error: 0x%x", ret);
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the flash memory to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        }
        return;
    }
    ESP_LOGI(TAG, "✓ Mount successful!");

    print_filesystem_diagnostics(base_path);
    // Print FAT FS size information
    uint64_t bytes_total, bytes_free;
    esp_vfs_fat_info(base_path, &bytes_total, &bytes_free);
    ESP_LOGI(TAG, "FAT FS: %" PRIu64 " kB total, %" PRIu64 " kB free", bytes_total / 1024, bytes_free / 1024);

    // Create a file in FAT FS
    ESP_LOGI(TAG, "Opening file");
    FILE *f = fopen("/nandflash/hello.txt", "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    fprintf(f, "Written using ESP-IDF %s\n", esp_get_idf_version());
    fclose(f);
    ESP_LOGI(TAG, "File written");

    // Open file for reading
    ESP_LOGI(TAG, "Reading file");
    f = fopen("/nandflash/hello.txt", "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return;
    }
    char line[128];
    fgets(line, sizeof(line), f);
    fclose(f);
    // strip newline
    char *pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);

    esp_vfs_fat_info(base_path, &bytes_total, &bytes_free);
    ESP_LOGI(TAG, "FAT FS: %" PRIu64 " kB total, %" PRIu64 " kB free", bytes_total / 1024, bytes_free / 1024);

    // Test if filesystem supports directories before running test
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Testing directory creation capability...");
    ESP_LOGI(TAG, "========================================");

    rmdir("/nandflash/test_dir");
    errno = 0;  // Clear errno before test
    int mkdir_result = mkdir("/nandflash/test_dir", 0755);
    int mkdir_errno = errno;
    if (mkdir_result == 0) {
        ESP_LOGI(TAG, "✓ Directory creation SUCCESS!");
        rmdir("/nandflash/test_dir");  // Clean up
        ESP_LOGI(TAG, "✓ Filesystem is healthy and ready!");
    } else {
        ESP_LOGE(TAG, "✗ Directory creation FAILED!");
        ESP_LOGE(TAG, "  mkdir() returned: %d", mkdir_result);
        ESP_LOGE(TAG, "  errno: %d (%s)", mkdir_errno, strerror(mkdir_errno));
        ESP_LOGE(TAG, "");
        ESP_LOGE(TAG, "Possible causes:");
        ESP_LOGE(TAG, "  1. CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED is NOT enabled");
        ESP_LOGE(TAG, "  2. Filesystem was not formatted (mounting old corrupted data)");
        ESP_LOGE(TAG, "  3. FAT16 filesystem doesn't support subdirectories properly");
        ESP_LOGE(TAG, "");
        ESP_LOGE(TAG, "Check the boot log above for:");
        ESP_LOGE(TAG, "  - 'Formatting FATFS partition' message");
        ESP_LOGE(TAG, "  - Filesystem type (should be FAT32, not FAT16)");
        return;
    }
    ESP_LOGI(TAG, "========================================");

    max_storage_test_with_subdirs();
    //max_storage_test();

    esp_vfs_fat_nand_unmount(base_path, flash);

    example_deinit_nand_flash(flash, spi);
}
