/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "sdkconfig.h"
#include "spi_nand_oper.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Bus transport under the NAND command layer.
 *
 * GPSPI and MSPI backends implement this; NAND oper/core must not call
 * spi_device_* or flash-host APIs directly.
 */
typedef struct spi_nand_transport spi_nand_transport_t;

struct spi_nand_transport {
    esp_err_t (*transmit)(spi_nand_transport_t *tr, spi_nand_transaction_t *t);
    size_t (*max_transfer)(spi_nand_transport_t *tr);
    bool (*is_half_duplex)(spi_nand_transport_t *tr);
    esp_err_t (*lock)(spi_nand_transport_t *tr);
    esp_err_t (*unlock)(spi_nand_transport_t *tr);
    void (*destroy)(spi_nand_transport_t *tr);
};

#ifndef CONFIG_IDF_TARGET_LINUX
#include "driver/spi_master.h"

/**
 * @brief Create GPSPI transport from an existing spi_device_handle_t.
 *
 * Compiled when CONFIG_NAND_FLASH_TRANSPORT_GPSPI=y (default).
 *
 * @param device_handle  Handle from spi_bus_add_device()
 * @param flags          Same meaning as spi_nand_flash_config_t.flags (SPI_DEVICE_HALFDUPLEX bit)
 */
#if !defined(CONFIG_NAND_FLASH_TRANSPORT_GPSPI) || CONFIG_NAND_FLASH_TRANSPORT_GPSPI
esp_err_t spi_nand_transport_gpspi_create(spi_device_handle_t device_handle, uint8_t flags,
                                          spi_nand_transport_t **out_transport);
#endif
#endif

#if CONFIG_NAND_FLASH_TRANSPORT_MSPI
struct spi_nand_flash_mspi_config_t; /* defined in spi_nand_flash_primary.h */

/**
 * @brief Create MSPI transport from primary attach config.
 *
 * Scaffold: stores config and exposes ops; full memspi wiring is incomplete.
 */
esp_err_t spi_nand_transport_mspi_create(const struct spi_nand_flash_mspi_config_t *cfg,
                                         spi_nand_transport_t **out_transport);
#endif /* CONFIG_NAND_FLASH_TRANSPORT_MSPI */

#ifdef __cplusplus
}
#endif
