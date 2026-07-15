/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_check.h"
#include "esp_littlefs.h"
#include "esp_vfs_littlefs_nand.h"

static const char *TAG = "vfs_littlefs_nand";

esp_err_t esp_vfs_littlefs_nand_mount(const char *base_path,
                                      esp_blockdev_handle_t wl_bdl,
                                      const esp_vfs_littlefs_nand_mount_config_t *mount_config)
{
#if !ESP_LITTLEFS_HAS_BLOCKDEV
    return ESP_ERR_NOT_SUPPORTED;
#else
    ESP_RETURN_ON_FALSE(base_path != NULL, ESP_ERR_INVALID_ARG, TAG, "base_path is NULL");
    ESP_RETURN_ON_FALSE(wl_bdl != NULL, ESP_ERR_INVALID_ARG, TAG, "wl_bdl is NULL");
    ESP_RETURN_ON_FALSE(mount_config != NULL, ESP_ERR_INVALID_ARG, TAG, "mount_config is NULL");

    esp_vfs_littlefs_conf_t conf = {
        .base_path = base_path,
        .partition_label = NULL,
        .partition = NULL,
        .blockdev = wl_bdl,
        .format_if_mount_failed = mount_config->format_if_mount_failed,
        .read_only = mount_config->read_only,
        .dont_mount = false,
        .grow_on_mount = false,
    };

    return esp_vfs_littlefs_register(&conf);
#endif
}

esp_err_t esp_vfs_littlefs_nand_unmount(esp_blockdev_handle_t wl_bdl)
{
#if !ESP_LITTLEFS_HAS_BLOCKDEV
    return ESP_ERR_NOT_SUPPORTED;
#else
    ESP_RETURN_ON_FALSE(wl_bdl != NULL, ESP_ERR_INVALID_ARG, TAG, "wl_bdl is NULL");
    return esp_vfs_littlefs_unregister_blockdev(wl_bdl);
#endif
}

esp_err_t esp_vfs_littlefs_nand_info(esp_blockdev_handle_t wl_bdl,
                                     size_t *total_bytes,
                                     size_t *used_bytes)
{
#if !ESP_LITTLEFS_HAS_BLOCKDEV
    return ESP_ERR_NOT_SUPPORTED;
#else
    ESP_RETURN_ON_FALSE(wl_bdl != NULL, ESP_ERR_INVALID_ARG, TAG, "wl_bdl is NULL");
    ESP_RETURN_ON_FALSE(total_bytes != NULL, ESP_ERR_INVALID_ARG, TAG, "total_bytes is NULL");
    ESP_RETURN_ON_FALSE(used_bytes != NULL, ESP_ERR_INVALID_ARG, TAG, "used_bytes is NULL");
    return esp_littlefs_blockdev_info(wl_bdl, total_bytes, used_bytes);
#endif
}
