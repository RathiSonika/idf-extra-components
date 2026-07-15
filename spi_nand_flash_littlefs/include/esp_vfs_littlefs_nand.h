/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include "esp_err.h"
#include "esp_blockdev.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mount configuration for LittleFS on SPI NAND Flash wear-leveling BDL.
 */
typedef struct {
    bool format_if_mount_failed; /**< Format the filesystem if mount fails */
    bool read_only;              /**< Mount the filesystem as read-only */
} esp_vfs_littlefs_nand_mount_config_t;

/**
 * @brief Mount LittleFS on SPI NAND Flash wear-leveling block device and register it in VFS.
 *
 * This is a convenience wrapper around `esp_vfs_littlefs_register()` that mounts LittleFS
 * on the wear-leveling block device returned by `spi_nand_flash_init_with_layers()`.
 *
 * @param base_path     VFS mount path (e.g. "/nandflash")
 * @param wl_bdl        Wear-leveling block device handle from `spi_nand_flash_init_with_layers()`
 * @param mount_config  Mount options
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if arguments are invalid
 *      - ESP_ERR_NOT_SUPPORTED if LittleFS blockdev support is unavailable
 *      - other error codes from LittleFS or the NAND driver
 *
 * @note On unmount, `esp_vfs_littlefs_nand_unmount()` releases the block device handle via
 *       `ops->release`, which deinitializes the underlying NAND flash layers.
 */
esp_err_t esp_vfs_littlefs_nand_mount(const char *base_path,
                                      esp_blockdev_handle_t wl_bdl,
                                      const esp_vfs_littlefs_nand_mount_config_t *mount_config);

/**
 * @brief Unmount LittleFS and release the wear-leveling block device.
 *
 * @param wl_bdl Wear-leveling block device handle passed to `esp_vfs_littlefs_nand_mount()`
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if the filesystem is not mounted on this block device
 *      - ESP_ERR_NOT_SUPPORTED if LittleFS blockdev support is unavailable
 */
esp_err_t esp_vfs_littlefs_nand_unmount(esp_blockdev_handle_t wl_bdl);

/**
 * @brief Get LittleFS capacity information for a mounted SPI NAND block device.
 *
 * @param wl_bdl       Wear-leveling block device handle
 * @param total_bytes  Total filesystem size in bytes
 * @param used_bytes   Used filesystem size in bytes
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if not mounted
 *      - ESP_ERR_NOT_SUPPORTED if LittleFS blockdev support is unavailable
 */
esp_err_t esp_vfs_littlefs_nand_info(esp_blockdev_handle_t wl_bdl,
                                     size_t *total_bytes,
                                     size_t *used_bytes);

#ifdef __cplusplus
}
#endif
