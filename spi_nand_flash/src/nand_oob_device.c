/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"

#if CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT

#include "nand_oob_device.h"

#include <stddef.h>
#include <string.h>

#include "esp_check.h"
#include "nand.h"
#include "nand_oob_layout_default.h"

static const char *TAG = "nand_oob_dev";

static uint16_t effective_spare_bytes(const spi_nand_flash_device_t *handle, const spi_nand_oob_layout_t *layout)
{
    if (layout->oob_bytes != 0) {
        return layout->oob_bytes;
    }
#ifdef CONFIG_IDF_TARGET_LINUX
    return (uint16_t)handle->chip.emulated_page_oob;
#else
    switch (handle->chip.page_size) {
    case 512:
        return 16;
    case 2048:
        return 64;
    case 4096:
        return 128;
    default:
        return 64;
    }
#endif
}

static esp_err_t cache_free_ecc_regions(spi_nand_flash_device_t *handle)
{
    const spi_nand_oob_layout_t *layout = handle->oob_layout;
    const void *chip_ctx = handle;

    handle->oob_cached_reg_count_free_ecc = 0;

    for (int section = 0;; section++) {
        spi_nand_oob_region_desc_t desc;
        esp_err_t err = layout->ops->free_region(chip_ctx, section, &desc);
        if (err == ESP_ERR_NOT_FOUND) {
            break;
        }
        ESP_RETURN_ON_ERROR(err, TAG, "free_region failed");

        if (desc.length == 0 || !desc.ecc_protected) {
            continue;
        }

        uint16_t spare = effective_spare_bytes(handle, layout);
        if ((uint32_t)desc.offset + desc.length > spare) {
            return ESP_ERR_INVALID_SIZE;
        }
        if (handle->oob_cached_reg_count_free_ecc >= SPI_NAND_OOB_MAX_REGIONS) {
            return ESP_ERR_INVALID_SIZE;
        }

        handle->oob_cached_regs_free_ecc[handle->oob_cached_reg_count_free_ecc] = desc;
        handle->oob_cached_reg_count_free_ecc++;
    }

    return ESP_OK;
}

esp_err_t nand_oob_attach_default_layout(spi_nand_flash_device_t *handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");

    handle->oob_layout = nand_oob_layout_get_default();
    ESP_RETURN_ON_FALSE(handle->oob_layout != NULL, ESP_ERR_INVALID_STATE, TAG, "default layout missing");
    ESP_RETURN_ON_FALSE(handle->oob_layout->ops != NULL && handle->oob_layout->ops->free_region != NULL,
                        ESP_ERR_INVALID_STATE, TAG, "layout ops incomplete");

    uint16_t spare = effective_spare_bytes(handle, handle->oob_layout);
    const spi_nand_oob_layout_t *l = handle->oob_layout;
    if ((uint32_t)l->bbm.bbm_offset + l->bbm.bbm_length > spare) {
        return ESP_ERR_INVALID_SIZE;
    }

    memset(handle->oob_fields, 0, sizeof(handle->oob_fields));

    ESP_RETURN_ON_ERROR(cache_free_ecc_regions(handle), TAG, "cache_free_ecc_regions failed");

    spi_nand_oob_field_spec_t *pu = &handle->oob_fields[SPI_NAND_OOB_FIELD_PAGE_USED];
    pu->id = SPI_NAND_OOB_FIELD_PAGE_USED;
    pu->length = 2;
    pu->oob_class = SPI_NAND_OOB_CLASS_FREE_ECC;
    pu->logical_offset = 0;
    pu->assigned = true;

    return ESP_OK;
}

#endif /* CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT */
