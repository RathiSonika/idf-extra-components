/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "nand.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t nand_onfi_read_parameter_page(spi_nand_flash_device_t *handle, uint8_t *data, uint16_t length);

/**
 * Tier 2: read and validate ONFI parameter page, populate chip geometry.
 * On success sets chip_source ONFI and SPI_NAND_CHIP_FLAG_ANONYMOUS.
 */
esp_err_t nand_onfi_try_init(spi_nand_flash_device_t *dev);

#ifdef __cplusplus
}
#endif
