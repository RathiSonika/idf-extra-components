/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"

#if CONFIG_NAND_FLASH_TRANSPORT_MSPI
#ifndef CONFIG_IDF_TARGET_LINUX

#include "esp_check.h"
#include "spi_nand_flash_primary.h"
#include "spi_nand_transport.h"
#include "nand_impl.h"

#ifdef CONFIG_NAND_FLASH_ENABLE_BDL
#include "esp_nand_blockdev.h"
#endif

static const char *TAG = "nand_primary";

esp_err_t spi_nand_flash_init_raw_primary(const spi_nand_flash_mspi_config_t *config,
                                          spi_nand_flash_device_t **handle)
{
    ESP_RETURN_ON_FALSE(config && handle, ESP_ERR_INVALID_ARG, TAG, "invalid arg");

    spi_nand_transport_t *transport = NULL;
    esp_err_t ret = spi_nand_transport_mspi_create(config, &transport);
    if (ret != ESP_OK) {
        return ret;
    }

    /* Chip detect will fail until MSPI transmit is fully wired. */
    ret = nand_init_device_with_transport(config->io_mode, config->gc_factor, transport, handle);
    return ret;
}

#ifdef CONFIG_NAND_FLASH_ENABLE_BDL
esp_err_t spi_nand_flash_get_raw_blockdev_primary(const spi_nand_flash_mspi_config_t *config,
                                                  esp_blockdev_handle_t *out_bdl)
{
    ESP_RETURN_ON_FALSE(config && out_bdl, ESP_ERR_INVALID_ARG, TAG, "invalid arg");

    /*
     * Reuse GPSPI raw-BDL factory by building a temporary path is not possible
     * without a device_handle. Primary path: init device then wrap — for now
     * callers can use init_raw_primary + a future wrap helper.
     *
     * Scaffold returns NOT_SUPPORTED until nand_flash_get_blockdev accepts a
     * pre-built device / transport.
     */
    (void)config;
    (void)out_bdl;
    return ESP_ERR_NOT_SUPPORTED;
}
#endif

#endif /* !LINUX */
#endif /* CONFIG_NAND_FLASH_TRANSPORT_MSPI */
