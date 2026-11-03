/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"

#if !defined(CONFIG_NAND_FLASH_TRANSPORT_GPSPI) || CONFIG_NAND_FLASH_TRANSPORT_GPSPI
#ifndef CONFIG_IDF_TARGET_LINUX

#include "spi_nand_transport.h"
#include "driver/spi_master.h"
#include "esp_idf_version.h"
#include "esp_check.h"

typedef struct {
    spi_nand_transport_t base;
    spi_device_handle_t device_handle;
    uint8_t flags;
} spi_nand_transport_gpspi_t;

static uint32_t map_flags_to_spi_trans(uint32_t nand_flags)
{
    uint32_t spi_flags = 0;
    if (nand_flags & SPI_NAND_TRANS_USE_TXDATA) {
        spi_flags |= SPI_TRANS_USE_TXDATA;
    }
    if (nand_flags & SPI_NAND_TRANS_USE_RXDATA) {
        spi_flags |= SPI_TRANS_USE_RXDATA;
    }
    if (nand_flags & SPI_NAND_TRANS_MODE_DIO) {
        spi_flags |= SPI_TRANS_MODE_DIO;
    }
    if (nand_flags & SPI_NAND_TRANS_MODE_QIO) {
        spi_flags |= SPI_TRANS_MODE_QIO;
    }
    if (nand_flags & SPI_NAND_TRANS_MULTILINE_ADDR) {
        spi_flags |= SPI_TRANS_MULTILINE_ADDR;
    }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    if (nand_flags & SPI_NAND_TRANS_DMA_BUFFER_ALIGN_MANUAL) {
        spi_flags |= SPI_TRANS_DMA_BUFFER_ALIGN_MANUAL;
    }
#endif
    return spi_flags;
}

static esp_err_t gpspi_transmit(spi_nand_transport_t *tr, spi_nand_transaction_t *transaction)
{
    spi_nand_transport_gpspi_t *gpspi = (spi_nand_transport_gpspi_t *)tr;
    uint8_t half_duplex = gpspi->flags & SPI_DEVICE_HALFDUPLEX;

    spi_nand_transaction_t local = *transaction;
    if (!half_duplex) {
        uint32_t len = local.miso_len > local.mosi_len ? local.miso_len : local.mosi_len;
        local.miso_len = len;
        local.mosi_len = len;
    }

    uint32_t spi_flags = map_flags_to_spi_trans(local.flags);
    spi_transaction_ext_t e = {
        .base = {
            .flags = SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_DUMMY | spi_flags,
            .rxlength = local.miso_len * 8,
            .rx_buffer = local.miso_data,
            .length = local.mosi_len * 8,
            .tx_buffer = local.mosi_data,
            .addr = local.address,
            .cmd = local.command,
        },
        .address_bits = local.address_bytes * 8,
        .command_bits = 8,
        .dummy_bits = local.dummy_bits,
    };

    if (spi_flags & SPI_TRANS_USE_TXDATA) {
        assert(local.mosi_len <= 4 && "SPI_TRANS_USE_TXDATA used for a long transaction");
        memcpy(e.base.tx_data, local.mosi_data, local.mosi_len);
    }
    if (spi_flags & SPI_TRANS_USE_RXDATA) {
        assert(local.miso_len <= 4 && "SPI_TRANS_USE_RXDATA used for a long transaction");
    }

    esp_err_t ret = spi_device_transmit(gpspi->device_handle, (spi_transaction_t *)&e);
    if (ret == ESP_OK && (spi_flags & SPI_TRANS_USE_RXDATA)) {
        memcpy(local.miso_data, e.base.rx_data, local.miso_len);
    }
    return ret;
}

static size_t gpspi_max_transfer(spi_nand_transport_t *tr)
{
    (void)tr;
    /* GPSPI with DMA can move full NAND pages; no small HW FIFO cap like MSPI. */
    return SIZE_MAX;
}

static bool gpspi_is_half_duplex(spi_nand_transport_t *tr)
{
    spi_nand_transport_gpspi_t *gpspi = (spi_nand_transport_gpspi_t *)tr;
    return (gpspi->flags & SPI_DEVICE_HALFDUPLEX) != 0;
}

static esp_err_t gpspi_lock(spi_nand_transport_t *tr)
{
    (void)tr;
    return ESP_OK;
}

static esp_err_t gpspi_unlock(spi_nand_transport_t *tr)
{
    (void)tr;
    return ESP_OK;
}

static void gpspi_destroy(spi_nand_transport_t *tr)
{
    free(tr);
}

esp_err_t spi_nand_transport_gpspi_create(spi_device_handle_t device_handle, uint8_t flags,
                                          spi_nand_transport_t **out_transport)
{
    ESP_RETURN_ON_FALSE(device_handle && out_transport, ESP_ERR_INVALID_ARG, "nand_gpspi", "invalid arg");

    spi_nand_transport_gpspi_t *gpspi = calloc(1, sizeof(*gpspi));
    if (!gpspi) {
        return ESP_ERR_NO_MEM;
    }

    gpspi->device_handle = device_handle;
    gpspi->flags = flags;
    gpspi->base.transmit = gpspi_transmit;
    gpspi->base.max_transfer = gpspi_max_transfer;
    gpspi->base.is_half_duplex = gpspi_is_half_duplex;
    gpspi->base.lock = gpspi_lock;
    gpspi->base.unlock = gpspi_unlock;
    gpspi->base.destroy = gpspi_destroy;

    *out_transport = &gpspi->base;
    return ESP_OK;
}

#endif /* !CONFIG_IDF_TARGET_LINUX */
#endif /* GPSPI enabled */
