/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "nand.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Default die-select hook: single-die, or multi-die with die folded into the row address. */
esp_err_t nand_select_die_noop(spi_nand_flash_device_t *dev, uint32_t die);

/** True when a vendor-specific select_die hook is installed (not the default no-op). */
bool nand_has_explicit_die_select(const spi_nand_flash_device_t *dev);

/**
 * Sync num_planes_per_lun from legacy num_planes when left 0, then validate
 * die/LUN geometry. Call after detect_chip().
 */
esp_err_t nand_validate_chip_geometry(spi_nand_flash_device_t *dev);

#ifdef __cplusplus
}
#endif
