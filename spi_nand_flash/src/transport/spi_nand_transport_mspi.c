/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"

#if CONFIG_NAND_FLASH_TRANSPORT_MSPI
#ifndef CONFIG_IDF_TARGET_LINUX

#include "spi_nand_transport.h"
#include "spi_nand_flash_primary.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "nand_mspi";

typedef struct {
    spi_nand_transport_t base;
    spi_nand_flash_mspi_config_t cfg;
    /* TODO: memspi_host_inst_t / spi_flash_host_inst_t from esp-idf */
    void *host;
} spi_nand_transport_mspi_t;

static esp_err_t mspi_transmit(spi_nand_transport_t *tr, spi_nand_transaction_t *t)
{
    spi_nand_transport_mspi_t *mspi = (spi_nand_transport_mspi_t *)tr;
    (void)t;

    if (mspi->host == NULL) {
        /* Scaffold: IDF memspi host not wired yet.
         * Intended path: map spi_nand_transaction_t -> spi_flash_trans_t,
         * chunk by max_transfer(), call host->driver->common_command().
         */
        ESP_LOGW(TAG, "MSPI transmit scaffold: host not initialized");
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_ERR_NOT_SUPPORTED;
}

static size_t mspi_max_transfer(spi_nand_transport_t *tr)
{
    (void)tr;
    /* Typical SPI flash HAL FIFO / common_command limit */
    return 64;
}

static bool mspi_is_half_duplex(spi_nand_transport_t *tr)
{
    (void)tr;
    return true;
}

static esp_err_t mspi_lock(spi_nand_transport_t *tr)
{
    (void)tr;
    /* TODO: acquire SPI1 / flash bus lock when sharing with main NOR */
    return ESP_OK;
}

static esp_err_t mspi_unlock(spi_nand_transport_t *tr)
{
    (void)tr;
    return ESP_OK;
}

static void mspi_destroy(spi_nand_transport_t *tr)
{
    free(tr);
}

esp_err_t spi_nand_transport_mspi_create(const spi_nand_flash_mspi_config_t *cfg,
                                         spi_nand_transport_t **out_transport)
{
    ESP_RETURN_ON_FALSE(cfg && out_transport, ESP_ERR_INVALID_ARG, TAG, "invalid arg");

    spi_nand_transport_mspi_t *mspi = calloc(1, sizeof(*mspi));
    if (!mspi) {
        return ESP_ERR_NO_MEM;
    }

    mspi->cfg = *cfg;
    mspi->host = NULL; /* TODO: memspi_host_init_pointers / spi_bus_add_flash_device path */
    mspi->base.transmit = mspi_transmit;
    mspi->base.max_transfer = mspi_max_transfer;
    mspi->base.is_half_duplex = mspi_is_half_duplex;
    mspi->base.lock = mspi_lock;
    mspi->base.unlock = mspi_unlock;
    mspi->base.destroy = mspi_destroy;

    ESP_LOGI(TAG, "MSPI transport scaffold created (host_id=%d cs_io=%d freq=%d MHz)",
             (int)cfg->host_id, cfg->cs_io_num, cfg->freq_mhz);

    *out_transport = &mspi->base;
    return ESP_OK;
}

#endif /* !LINUX */
#endif /* CONFIG_NAND_FLASH_TRANSPORT_MSPI */
