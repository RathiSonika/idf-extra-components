/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "ff.h"
#include "spi_nand_flash.h"
#include "nand_linux_mmap_emul.h"
#include "diskio_nand.h"
#include "diskio_impl.h"
#include "esp_log.h"

#include <catch2/catch_test_macros.hpp>
static const char *TAG = "example";
const char *base_path = "/nandflash";
/*
void __cyg_profile_func_enter (void *this_fn, void *call_site) {
    printf( "entering %p\n", this_fn );
}

void __cyg_profile_func_exit (void *this_fn, void *call_site) {
    printf( "leaving %p\n", this_fn );
}
*/
/*
TEST_CASE("Create NAND volume, open file, write and read back data", "[nand_flash][basic]")
{
    FRESULT fr_result;
    BYTE pdrv;
    FATFS fs;
    FIL file;
    UINT bw;
    esp_err_t esp_result;

    // Setup NAND emulation - 50MB size
    nand_file_mmap_emul_config_t nand_config = {"", 50 * 1024 * 1024, true};

    spi_nand_flash_config_t flash_config = {&nand_config, 0, SPI_NAND_IO_MODE_SIO, 0};

    spi_nand_flash_device_t *nand_device;
    esp_result = spi_nand_flash_init_device(&flash_config, &nand_device);
    REQUIRE(esp_result == ESP_OK);
    REQUIRE(nand_device != nullptr);

    // Get a physical drive
    esp_result = ff_diskio_get_drive(&pdrv);
    REQUIRE(esp_result == ESP_OK);

    // Register NAND device with FATFS
    esp_result = ff_diskio_register_nand(pdrv, nand_device);
    REQUIRE(esp_result == ESP_OK);

    char drv[3] = {(char)('0' + pdrv), ':', 0};

    // Format filesystem
    const size_t workbuf_size = 4096;
    void *workbuf = ff_memalloc(workbuf_size);
    REQUIRE(workbuf != nullptr);

    // For host tests, include FM_SFD flag when formatting
    const MKFS_PARM opt = {(BYTE)(FM_ANY | FM_SFD), 2, 0, 128, 0};
    fr_result = f_mkfs(drv, &opt, workbuf, workbuf_size);
    free(workbuf);
    REQUIRE(fr_result == FR_OK);

    // Mount the volume
    fr_result = f_mount(&fs, drv, 0);
    REQUIRE(fr_result == FR_OK);

    // Open, write and read data
    char filepath[32];
    snprintf(filepath, sizeof(filepath), "%s/test.txt", drv);
    fr_result = f_open(&file, filepath, FA_OPEN_ALWAYS | FA_READ | FA_WRITE);
    REQUIRE(fr_result == FR_OK);

    // Generate data
    uint32_t data_size = 1000;

    char *data = (char*) malloc(data_size);
    char *read = (char*) malloc(data_size);

    for(uint32_t i = 0; i < data_size; i += sizeof(i))
    {
        *((uint32_t*)(data + i)) = i;
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

    // Unmount volume
    fr_result = f_mount(0, drv, 0);
    REQUIRE(fr_result == FR_OK);

    // Cleanup
    free(read);
    free(data);
    ff_diskio_clear_pdrv_nand(nand_device);
    spi_nand_flash_deinit_device(nand_device);
}
*/
static bool create_test_file(const char *filepath, size_t file_size_bytes)
{
    FIL file;
    FRESULT fr_result;
    UINT bw;

    fr_result = f_open(&file, filepath, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr_result != FR_OK) {
        printf("FR_Result: %d\n", fr_result);
        return false;
    }

    // Create test data pattern
    const size_t chunk_size = 1024;
    char *buffer = (char *)malloc(chunk_size);
    if (!buffer) {
        f_close(&file);
        return false;
    }

    // Fill buffer with recognizable pattern
    for (size_t i = 0; i < chunk_size; i++) {
        buffer[i] = (char)(i % 256);
    }

    // Write data in chunks
    size_t remaining = file_size_bytes;
    while (remaining > 0) {
        size_t write_size = (remaining > chunk_size) ? chunk_size : remaining;
        fr_result = f_write(&file, buffer, write_size, &bw);
        if (fr_result != FR_OK || bw != write_size) {
            free(buffer);
            f_close(&file);
            return false;
        }
        remaining -= write_size;
    }

    free(buffer);
    f_close(&file);
    return true;
}
static void get_filesystem_usage(const char *drive, DWORD &free_clusters, DWORD &total_clusters)
{
    FATFS *fs;
    FRESULT fr_result = f_getfree(drive, &free_clusters, &fs);
    REQUIRE(fr_result == FR_OK);
    total_clusters = fs->n_fatent - 2;
}

void max_storage_test_with_subdirs(char *drv)
{
    const size_t file_size = 10 * 1024;  // 10KB files
    const int files_per_dir = 300;       // Limit files per directory

    char filepath[32];
    char dirpath[32];
    int files_created = 0;
    int target_files = 3000;  // More than single-directory scenario

    printf("Creating 10KB files distributed across directories...\n");
    printf("Max %d files per directory to avoid expansion issues\n\n", files_per_dir);

    DWORD initial_free, total_clusters;
    get_filesystem_usage(drv, initial_free, total_clusters);
    printf("total_clusters: %d, initial_free:%d\n", total_clusters, initial_free);

    // Create files distributed across directories
    for (int i = 0; i < target_files; i++) {
        int dir_num = i / files_per_dir;
        int file_in_dir = i % files_per_dir;

        snprintf(dirpath, sizeof(dirpath), "%s/dir_%d", drv, dir_num);
        snprintf(filepath, sizeof(filepath), "%s/file_%d.jpg", dirpath, file_in_dir);

        // Create directory if it's the first file in this directory
        if (file_in_dir == 0) {
            FRESULT fr_result = f_mkdir(dirpath);
            if (fr_result != FR_OK && fr_result != FR_EXIST) {
                printf("Failed to create directory %s: resurl: %d\n", dirpath, fr_result);
                break;
            }
        }

        if (!create_test_file(filepath, file_size)) {
            printf("Failed to create file %d: %s\n", i, filepath);
            files_created = i;
            break;
        }

        // Print progress
        if ((i + 1) % 500 == 0) {
            DWORD current_free, current_total;
            get_filesystem_usage(drv, current_free, current_total);
            float usage_percent = ((float)(total_clusters - current_free) / total_clusters) * 100.0f;
            printf("Created %d files in %d directories, %.1f%% used\n",
                   i + 1, (i / files_per_dir) + 1, usage_percent);
        }
    }

    if (files_created == 0) {
        files_created = target_files;  // All files created successfully
    }

    DWORD final_free, final_total;
    get_filesystem_usage(drv, final_free, final_total);
    float final_usage = ((float)(total_clusters - final_free) / total_clusters) * 100.0f;

    printf("=== DIRECTORY DISTRIBUTION RESULT ===\n");
    printf("Files created: %d\n", files_created);
    printf("Directories used: %d\n", (files_created / files_per_dir) + 1);
    printf("Free clusters: %u (%.1f%% used)\n", (unsigned)final_free, final_usage);
    printf("Distribution reduces expansion pressure\n");
    printf("====================================\n");

    // Should create more files than single directory scenario
    // But still limited by underlying fragmentation
    REQUIRE(files_created >= 2500);  // Should improve over single directory

    printf("\nTest completed: Directory distribution helps but doesn't solve fragmentation\n");
}

TEST_CASE("Format NAND volume and verify filesystem type", "[nand_flash][basic]")
{
    FRESULT fr_result;
    BYTE pdrv;
    FATFS fs;
    esp_err_t esp_result;

    nand_file_mmap_emul_config_t nand_config = {"", 256 * 1024 * 1024, true};

    spi_nand_flash_config_t flash_config = {&nand_config, 0, SPI_NAND_IO_MODE_SIO, 0};
    spi_nand_flash_device_t *nand_device;
    esp_result = spi_nand_flash_init_device(&flash_config, &nand_device);
    REQUIRE(esp_result == ESP_OK);

    // Get drive
    esp_result = ff_diskio_get_drive(&pdrv);
    REQUIRE(esp_result == ESP_OK);

    esp_result = ff_diskio_register_nand(pdrv, nand_device);
    REQUIRE(esp_result == ESP_OK);

    char drv[3] = {(char)('0' + pdrv), ':', 0};

    // Format with specific cluster size
    const size_t workbuf_size = 4096;
    void *workbuf = ff_memalloc(workbuf_size);
    REQUIRE(workbuf != nullptr);

    const MKFS_PARM opt = {(BYTE)(FM_ANY | FM_SFD), 2, 0, 0, 2048};  // 1 sector per cluster
    fr_result = f_mkfs(drv, &opt, workbuf, workbuf_size);
    free(workbuf);
    REQUIRE(fr_result == FR_OK);

    // Mount and check filesystem
    fr_result = f_mount(&fs, drv, 1);
    REQUIRE(fr_result == FR_OK);

    // Get filesystem information
    DWORD free_clusters, total_clusters;
    FATFS *fs_ptr;
    fr_result = f_getfree(drv, &free_clusters, &fs_ptr);
    REQUIRE(fr_result == FR_OK);

    total_clusters = fs_ptr->n_fatent - 2;

    printf("=== NAND Filesystem Info ===\n");
    printf("FAT Type: %s\n", (fs_ptr->fs_type == FS_FAT32) ? "FAT32" :
           (fs_ptr->fs_type == FS_FAT16) ? "FAT16" : "FAT12");
    printf("Total clusters: %u\n", (unsigned)total_clusters);
    printf("Free clusters: %u\n", (unsigned)free_clusters);
    printf("Cluster size: %u sectors\n", (unsigned)fs_ptr->csize);
    printf("============================\n");

    // Verify we have a valid filesystem
    REQUIRE(total_clusters > 0);
    REQUIRE(free_clusters <= total_clusters);

    max_storage_test_with_subdirs(drv);
    // Unmount
    fr_result = f_mount(0, drv, 0);
    REQUIRE(fr_result == FR_OK);

    // Cleanup
    ff_diskio_clear_pdrv_nand(nand_device);
    spi_nand_flash_deinit_device(nand_device);
}


