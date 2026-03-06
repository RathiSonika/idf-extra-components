/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp_nand_partition.h
 * @brief Partition Block Device Layer for SPI NAND Flash
 *
 * Provides APIs to create and manage partition block devices on top of
 * the Wear-Leveling Block Device Layer (WL BDL). Each partition is a
 * logical sub-region of the WL device, suitable for independent
 * filesystem mounting.
 *
 * Uses the generic partition API from esp_blockdev_util internally.
 *
 * @note Requires CONFIG_NAND_FLASH_ENABLE_BDL and ESP-IDF >= 6.0.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef CONFIG_NAND_FLASH_ENABLE_BDL
#include "esp_blockdev.h"
#include "spi_nand_flash.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SPI_NAND_PARTITION_NAME_MAX_LEN  16
#define SPI_NAND_MAX_PARTITIONS          8
#define SPI_NAND_MAX_INSTANCES           4

/**
 * @brief Configuration for a single NAND partition
 */
typedef struct {
    const char *name;                /*!< Partition name (max SPI_NAND_PARTITION_NAME_MAX_LEN-1 chars) */
    size_t offset;                   /*!< Offset within the WL block device (must be aligned to read_size) */
    size_t size;                     /*!< Size of the partition (must be aligned to read_size) */
} spi_nand_partition_config_t;

/**
 * @brief Register partitions on top of an existing WL block device
 *
 * Creates partition block devices on top of the WL BDL using the generic
 * partition API from esp_blockdev_util. Partition metadata is stored
 * internally and can be retrieved via spi_nand_flash_get_partition().
 *
 * @param wl_bdl          WL block device handle (from spi_nand_flash_init_with_layers)
 * @param partitions      Array of partition configurations
 * @param num_partitions  Number of partitions (1 to SPI_NAND_MAX_PARTITIONS)
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if arguments are invalid, partitions overlap, or bounds exceeded
 *      - ESP_ERR_INVALID_STATE if partitions are already registered for this WL BDL
 *      - ESP_ERR_NO_MEM if no free partition table slots or memory allocation failed
 */
esp_err_t spi_nand_flash_register_partitions(esp_blockdev_handle_t wl_bdl,
        const spi_nand_partition_config_t *partitions,
        size_t num_partitions);

/**
 * @brief Initialize SPI NAND Flash and register partitions in one call
 *
 * Convenience function that combines spi_nand_flash_init_with_layers() and
 * spi_nand_flash_register_partitions().
 *
 * @param config          NAND flash configuration
 * @param partitions      Array of partition configurations
 * @param num_partitions  Number of partitions
 * @param[out] wl_bdl     Pointer to store WL block device handle
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if arguments are invalid
 *      - ESP_ERR_NO_MEM if insufficient memory
 *      - other error codes from lower layers
 */
esp_err_t spi_nand_flash_init_with_partitions(spi_nand_flash_config_t *config,
        const spi_nand_partition_config_t *partitions,
        size_t num_partitions,
        esp_blockdev_handle_t *wl_bdl);

/**
 * @brief Get a partition block device handle by name
 *
 * @param wl_bdl          WL block device handle that owns the partitions
 * @param name            Partition name to look up
 * @param[out] partition  Pointer to store partition block device handle
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if arguments are NULL
 *      - ESP_ERR_NOT_FOUND if partition name not found or no partitions registered
 */
esp_err_t spi_nand_flash_get_partition(esp_blockdev_handle_t wl_bdl,
                                       const char *name,
                                       esp_blockdev_handle_t *partition);

/**
 * @brief Release all partitions associated with a WL block device
 *
 * Releases all partition block devices and the WL BDL itself (which in turn
 * releases the Flash BDL and deinitializes the NAND device).
 *
 * @note All partitions must be unmounted before calling this function.
 *
 * @param wl_bdl WL block device handle
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if wl_bdl is NULL
 *      - ESP_ERR_NOT_FOUND if no partition table found for this WL BDL
 */
esp_err_t spi_nand_flash_release_partitions(esp_blockdev_handle_t wl_bdl);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_NAND_FLASH_ENABLE_BDL
