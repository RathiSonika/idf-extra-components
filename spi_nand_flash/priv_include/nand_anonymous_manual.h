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

/**
 * Tier 3: apply user-supplied geometry and delays from Kconfig.
 * On success sets chip_source MANUAL and SPI_NAND_CHIP_FLAG_ANONYMOUS.
 */
esp_err_t nand_anonymous_manual_try_init(spi_nand_flash_device_t *dev);

#ifdef __cplusplus
}
#endif
