/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/**
 * @file spi_nand_flash_primary.h
 * @brief System / primary-NAND bring-up (MSPI). Not the app-facing GPSPI WL path.
 *
 * Applications using external NAND over GPSPI should keep using spi_nand_flash.h
 * (init_device / init_with_layers). This header is for bootloader / board / IDF
 * integration that attaches NAND on MSPI and exposes Raw NAND access.
 */

#include <stdint.h>
#include "esp_err.h"
#include "spi_nand_flash.h"

#ifndef CONFIG_IDF_TARGET_LINUX
#include "hal/spi_types.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Flexible MSPI attach config (topology not fixed yet).
 *
 * Typical expectation is SPI1_HOST; CS/freq/mode are board-specific.
 */
typedef struct spi_nand_flash_mspi_config_t {
#ifndef CONFIG_IDF_TARGET_LINUX
    spi_host_device_t host_id;   ///< Typically SPI1_HOST
#endif
    int cs_io_num;               ///< GPIO for NAND CS
    int cs_id;                   ///< CS line id when applicable (e.g. SPI1 CS1)
    int freq_mhz;                ///< NAND clock on MSPI
    int input_delay_ns;          ///< Optional input delay; 0 if unknown
    spi_nand_flash_io_mode_t io_mode;
    uint8_t gc_factor;            ///< Unused for raw primary init; reserved for later WL mount
} spi_nand_flash_mspi_config_t;

#if CONFIG_NAND_FLASH_TRANSPORT_MSPI

/**
 * @brief Initialize primary NAND via MSPI Raw path (no wear-leveling).
 *
 * Creates MSPI transport + NAND core. Intended for loader / system bring-up.
 *
 * @note MSPI host wiring is scaffolded; transmit may return ESP_ERR_NOT_SUPPORTED
 *       until memspi integration is completed.
 */
esp_err_t spi_nand_flash_init_raw_primary(const spi_nand_flash_mspi_config_t *config,
                                          spi_nand_flash_device_t **handle);

#ifdef CONFIG_NAND_FLASH_ENABLE_BDL
/**
 * @brief Same as init_raw_primary, but returns Raw NAND as esp_blockdev_t.
 */
esp_err_t spi_nand_flash_get_raw_blockdev_primary(const spi_nand_flash_mspi_config_t *config,
                                                  esp_blockdev_handle_t *out_bdl);
#endif

#endif /* CONFIG_NAND_FLASH_TRANSPORT_MSPI */

/* Physical reads for the loader: see spi_nand_flash_read_physical*() in spi_nand_flash.h */

#ifdef __cplusplus
}
#endif
