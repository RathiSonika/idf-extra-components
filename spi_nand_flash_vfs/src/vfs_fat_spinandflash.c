/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2023 Espressif Systems (Shanghai) CO LTD
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <esp_check.h>
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "spi_nand_flash.h"
#include "vfs_fat_internal.h"
#include "diskio_impl.h"
#include "diskio_nand.h"

#ifdef CONFIG_NAND_FLASH_ENABLE_BDL
#include "diskio_nand_blockdev.h"
#include "esp_blockdev.h"
#endif

static const char *TAG = "vfs_fat_nand";

#ifdef CONFIG_NAND_FLASH_ENABLE_BDL
/**
 * Parse a FAT BPB from a sector buffer and return the total volume size.
 * Returns true if a valid BPB was found, with fat_size_out set to total bytes.
 */
static bool parse_fat_bpb(const uint8_t *buf, uint64_t *fat_size_out)
{
    bool has_jump = (buf[0] == 0xEB || buf[0] == 0xE9);
    uint16_t bpb_bytes_per_sec = (uint16_t)(buf[11] | (buf[12] << 8));
    bool valid_bps = (bpb_bytes_per_sec >= 512)
                     && (bpb_bytes_per_sec <= 4096)
                     && ((bpb_bytes_per_sec & (bpb_bytes_per_sec - 1)) == 0);

    uint16_t tot_sec16 = (uint16_t)(buf[19] | (buf[20] << 8));
    uint32_t tot_sec32 = (uint32_t)(buf[32] | (buf[33] << 8) | (buf[34] << 16) | (buf[35] << 24));
    uint32_t fat_total_sectors = (tot_sec16 != 0) ? tot_sec16 : tot_sec32;

    if (!has_jump || !valid_bps || fat_total_sectors == 0) {
        return false;
    }

    *fat_size_out = (uint64_t)fat_total_sectors * bpb_bytes_per_sec;
    return true;
}

/**
 * If format_if_mount_failed is set, check whether the partition has an existing
 * FAT with a different size than the blockdev. If so, force format before mount.
 * Returns true when caller should format and then mount (skip initial f_mount).
 */
static bool should_force_format_on_size_change(esp_blockdev_handle_t blockdev)
{
    size_t read_size = blockdev->geometry.read_size;
    if (read_size < 512) {
        return false;
    }

    uint8_t *buf = calloc(1, read_size);
    if (!buf) {
        return false;
    }

    esp_err_t ret = blockdev->ops->read(blockdev, buf, read_size, 0, read_size);
    if (ret != ESP_OK) {
        free(buf);
        return false;
    }

    if (buf[510] != 0x55 || buf[511] != 0xAA) {
        free(buf);
        return false;
    }

    uint64_t fat_size = 0;
    uint64_t partition_size = blockdev->geometry.disk_size;

    if (parse_fat_bpb(buf, &fat_size)) {
        /* SFD layout: boot sector at sector 0 */
        if (fat_size != partition_size) {
            ESP_LOGW(TAG, "Partition size changed (FAT=%" PRIu64 " vs partition=%" PRIu64 "). "
                     "Forcing format.", fat_size, partition_size);
            free(buf);
            return true;
        }
        free(buf);
        return false;
    }

    /* Sector 0 has 0x55AA but no valid BPB — check for MBR */
    uint32_t boot_lba = 0;
    for (int i = 0; i < 4; i++) {
        const uint8_t *entry = buf + 446 + i * 16;
        if (entry[4] != 0) {
            boot_lba = (uint32_t)(entry[8] | (entry[9] << 8) |
                                  (entry[10] << 16) | (entry[11] << 24));
            break;
        }
    }

    if (boot_lba == 0) {
        free(buf);
        return false;
    }

    uint64_t boot_addr = (uint64_t)boot_lba * read_size;
    if (boot_addr + read_size > partition_size) {
        free(buf);
        return true; /* MBR points beyond partition — force format */
    }

    ret = blockdev->ops->read(blockdev, buf, read_size, boot_addr, read_size);
    if (ret != ESP_OK) {
        free(buf);
        return false;
    }

    if (!parse_fat_bpb(buf, &fat_size)) {
        free(buf);
        return false;
    }

    if (fat_size != partition_size) {
        ESP_LOGW(TAG, "Partition size changed (FAT=%" PRIu64 " vs partition=%" PRIu64 "). "
                 "Forcing format.", fat_size, partition_size);
        free(buf);
        return true;
    }

    free(buf);
    return false;
}
#endif /* CONFIG_NAND_FLASH_ENABLE_BDL */

esp_err_t esp_vfs_fat_nand_mount(const char *base_path, spi_nand_flash_device_t *nand_device,
                                 const esp_vfs_fat_mount_config_t *mount_config)
{
    esp_err_t ret = ESP_OK;
    void *workbuf = NULL;
    FATFS *fs = NULL;
    uint32_t page_size;
    const size_t workbuf_size = 4096;

    // connect driver to FATFS
    BYTE pdrv = 0xFF;
    ESP_GOTO_ON_ERROR(ff_diskio_get_drive(&pdrv), fail, TAG, "the maximum count of volumes is already mounted");
    ESP_LOGD(TAG, "using pdrv=%i", pdrv);
    char drv[3] = {(char)('0' + pdrv), ':', 0};

    ESP_GOTO_ON_ERROR(ff_diskio_register_nand(pdrv, nand_device),
                      fail, TAG, "ff_diskio_register_nand failed drv=%i", pdrv);

    ESP_GOTO_ON_ERROR(spi_nand_flash_get_page_size(nand_device, &page_size), fail, TAG, "");

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    esp_vfs_fat_conf_t conf = {
        .base_path = base_path,
        .fat_drive = drv,
        .max_files = mount_config->max_files,
    };
    ESP_GOTO_ON_ERROR(esp_vfs_fat_register_cfg(&conf, &fs), fail, TAG, "esp_vfs_fat_register failed");
#else
    ESP_GOTO_ON_ERROR(esp_vfs_fat_register(base_path, drv, mount_config->max_files, &fs),
                      fail, TAG, "esp_vfs_fat_register failed");
#endif

    // Try to mount partition
    FRESULT fresult = f_mount(fs, drv, 1);
    if (fresult != FR_OK) {
        ESP_LOGW(TAG, "f_mount failed (%d)", fresult);
        if (!((fresult == FR_NO_FILESYSTEM || fresult == FR_INT_ERR)
                && mount_config->format_if_mount_failed)) {
            ret = ESP_FAIL;
            goto fail;
        }

        workbuf = ff_memalloc(workbuf_size);
        if (workbuf == NULL) {
            ret = ESP_ERR_NO_MEM;
            goto fail;
        }
        size_t alloc_unit_size = esp_vfs_fat_get_allocation_unit_size(
                                     page_size,
                                     mount_config->allocation_unit_size);
        ESP_LOGI(TAG, "Formatting FATFS partition, allocation unit size=%d", alloc_unit_size);
        const MKFS_PARM opt = {(BYTE)(FM_ANY | FM_SFD), 0, 0, 0, alloc_unit_size};
        fresult = f_mkfs(drv, &opt, workbuf, workbuf_size);
        if (fresult != FR_OK) {
            ret = ESP_FAIL;
            ESP_LOGE(TAG, "f_mkfs failed (%d)", fresult);
            goto fail;
        }
        free(workbuf);
        workbuf = NULL;
        ESP_LOGI(TAG, "Mounting again");
        fresult = f_mount(fs, drv, 0);
        if (fresult != FR_OK) {
            ret = ESP_FAIL;
            ESP_LOGE(TAG, "f_mount failed after formatting (%d)", fresult);
            goto fail;
        }
    }
    return ESP_OK;

fail:
    if (workbuf) {
        free(workbuf);
    }
    if (fs) {
        esp_vfs_fat_unregister_path(base_path);
    }
    ff_diskio_unregister(pdrv);
    return ret;
}

esp_err_t esp_vfs_fat_nand_unmount(const char *base_path, spi_nand_flash_device_t *nand_device)
{
    BYTE pdrv = ff_diskio_get_pdrv_nand(nand_device);
    if (pdrv == 0xff) {
        return ESP_ERR_INVALID_STATE;
    }

    char drv[3] = {(char)('0' + pdrv), ':', 0};
    f_mount(NULL, drv, 0);

    ff_diskio_unregister(pdrv);
    ff_diskio_clear_pdrv_nand(nand_device);

    esp_err_t err = esp_vfs_fat_unregister_path(base_path);
    return err;
}

#ifdef CONFIG_NAND_FLASH_ENABLE_BDL
esp_err_t esp_vfs_fat_nand_mount_bdl(const char *base_path, esp_blockdev_handle_t blockdev,
                                     const esp_vfs_fat_mount_config_t *mount_config)
{
    esp_err_t ret = ESP_OK;
    void *workbuf = NULL;
    FATFS *fs = NULL;
    size_t page_size;
    const size_t workbuf_size = 4096;

    if (blockdev == NULL || blockdev->ops == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // connect driver to FATFS
    BYTE pdrv = 0xFF;
    ESP_GOTO_ON_ERROR(ff_diskio_get_drive(&pdrv), fail, TAG, "the maximum count of volumes is already mounted");
    ESP_LOGD(TAG, "using pdrv=%i", pdrv);
    char drv[3] = {(char)('0' + pdrv), ':', 0};

    ESP_GOTO_ON_ERROR(ff_diskio_register_blockdev(pdrv, blockdev),
                      fail, TAG, "ff_diskio_register_blockdev failed drv=%i", pdrv);

    page_size = blockdev->geometry.read_size;

    FRESULT fresult;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    esp_vfs_fat_conf_t conf = {
        .base_path = base_path,
        .fat_drive = drv,
        .max_files = mount_config->max_files,
    };
    ESP_GOTO_ON_ERROR(esp_vfs_fat_register_cfg(&conf, &fs), fail, TAG, "esp_vfs_fat_register failed");
#else
    ESP_GOTO_ON_ERROR(esp_vfs_fat_register(base_path, drv, mount_config->max_files, &fs),
                      fail, TAG, "esp_vfs_fat_register failed");
#endif

    /* When format_if_mount_failed is set, check for partition size change and
     * force format so the filesystem matches the current partition size. */
    if (mount_config->format_if_mount_failed && should_force_format_on_size_change(blockdev)) {
        goto format;
    }

    // Try to mount partition
    fresult = f_mount(fs, drv, 1);
    if (fresult != FR_OK) {
        ESP_LOGW(TAG, "f_mount failed (%d)", fresult);
        if (!((fresult == FR_NO_FILESYSTEM || fresult == FR_INT_ERR)
                && mount_config->format_if_mount_failed)) {
            ret = ESP_FAIL;
            goto fail;
        }
    }

format:
    workbuf = ff_memalloc(workbuf_size);
    if (workbuf == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto fail;
    }
    size_t alloc_unit_size = esp_vfs_fat_get_allocation_unit_size(
                                 page_size,
                                 mount_config->allocation_unit_size);
    ESP_LOGI(TAG, "Formatting FATFS partition, allocation unit size=%d", alloc_unit_size);
    const MKFS_PARM opt = {(BYTE)(FM_ANY | FM_SFD), 0, 0, 0, alloc_unit_size};
    fresult = f_mkfs(drv, &opt, workbuf, workbuf_size);
    if (fresult != FR_OK) {
        ret = ESP_FAIL;
        ESP_LOGE(TAG, "f_mkfs failed (%d)", fresult);
        goto fail;
    }
    free(workbuf);
    workbuf = NULL;
    ESP_LOGI(TAG, "Mounting again");
    fresult = f_mount(fs, drv, 0);
    if (fresult != FR_OK) {
        ret = ESP_FAIL;
        ESP_LOGE(TAG, "f_mount failed after formatting (%d)", fresult);
        goto fail;
    }
    return ESP_OK;

fail:
    if (workbuf) {
        free(workbuf);
    }
    if (fs) {
        esp_vfs_fat_unregister_path(base_path);
    }
    ff_diskio_unregister(pdrv);
    return ret;
}

esp_err_t esp_vfs_fat_nand_unmount_bdl(const char *base_path, esp_blockdev_handle_t blockdev)
{
    BYTE pdrv = ff_diskio_get_pdrv_blockdev(blockdev);
    if (pdrv == 0xff) {
        return ESP_ERR_INVALID_STATE;
    }

    char drv[3] = {(char)('0' + pdrv), ':', 0};
    f_mount(NULL, drv, 0);

    ff_diskio_unregister(pdrv);
    ff_diskio_clear_pdrv_blockdev(blockdev);

    esp_err_t err = esp_vfs_fat_unregister_path(base_path);
    return err;
}
#endif // CONFIG_NAND_FLASH_ENABLE_BDL
