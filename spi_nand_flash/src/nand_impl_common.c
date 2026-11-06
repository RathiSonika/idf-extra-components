/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nand_impl_common.h"
#include "esp_check.h"

static const char *TAG = "nand_impl_common";

esp_err_t nand_select_die_noop(spi_nand_flash_device_t *dev, uint32_t die)
{
    (void)dev;
    (void)die;
    return ESP_OK;
}

bool nand_has_explicit_die_select(const spi_nand_flash_device_t *dev)
{
    return dev->select_die != NULL && dev->select_die != nand_select_die_noop;
}

esp_err_t nand_validate_chip_geometry(spi_nand_flash_device_t *dev)
{
    /* Legacy dual-plane chips only set num_planes; sync before any plane-aware path. */
    if (dev->chip.num_planes_per_lun == 0) {
        dev->chip.num_planes_per_lun = dev->chip.num_planes;
    }

    ESP_RETURN_ON_FALSE(dev->chip.num_dies != 0, ESP_ERR_INVALID_STATE, TAG, "num_dies must be non-zero");
    ESP_RETURN_ON_FALSE(dev->chip.num_luns_per_die != 0, ESP_ERR_INVALID_STATE, TAG,
                        "num_luns_per_die must be non-zero");
    ESP_RETURN_ON_FALSE((dev->chip.num_blocks % (dev->chip.num_dies * dev->chip.num_luns_per_die)) == 0,
                        ESP_ERR_INVALID_STATE, TAG,
                        "num_blocks not divisible by num_dies * num_luns_per_die");

    if (nand_has_explicit_die_select(dev)) {
        ESP_RETURN_ON_FALSE(dev->chip.num_dies > 1, ESP_ERR_INVALID_STATE, TAG,
                            "custom select_die requires num_dies > 1");
    }
    return ESP_OK;
}
