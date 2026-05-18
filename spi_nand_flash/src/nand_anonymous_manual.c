/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <string.h>
#include "esp_check.h"
#include "esp_log.h"
#include "nand.h"
#include "nand_anonymous_manual.h"

static const char *TAG = "nand_manual";

static bool is_power_of_two(uint32_t n)
{
    return n != 0 && (n & (n - 1)) == 0;
}

static int log2_u32(uint32_t n)
{
    int log2 = 0;
    n >>= 1;
    while (n != 0) {
        n >>= 1;
        log2++;
    }
    return log2;
}

static esp_err_t validate_manual_geometry(uint32_t page_size, uint32_t pages_per_block,
        uint32_t num_blocks, uint32_t num_planes, uint32_t t_r_us, uint32_t t_prog_us,
        uint32_t t_bers_us)
{
    if (page_size < 512 || page_size > 8192 || !is_power_of_two(page_size)) {
        ESP_LOGE(TAG, "Invalid manual page size %" PRIu32 " (power of 2, 512-8192 required)", page_size);
        return ESP_ERR_INVALID_ARG;
    }
    if (pages_per_block < 32 || pages_per_block > 256 || !is_power_of_two(pages_per_block)) {
        ESP_LOGE(TAG, "Invalid manual pages per block %" PRIu32 " (power of 2, 32-256 required)",
                 pages_per_block);
        return ESP_ERR_INVALID_ARG;
    }
    if (num_blocks == 0) {
        ESP_LOGE(TAG, "Invalid manual block count (must be > 0)");
        return ESP_ERR_INVALID_ARG;
    }
    if (num_planes < 1 || num_planes > 4 || !is_power_of_two(num_planes)) {
        ESP_LOGE(TAG, "Invalid manual num_planes %" PRIu32 " (power of 2, 1-4 required)", num_planes);
        return ESP_ERR_INVALID_ARG;
    }
    if (t_r_us == 0 || t_prog_us == 0 || t_bers_us == 0) {
        ESP_LOGE(TAG, "Invalid manual timing (t_r, t_prog, t_bers must be > 0)");
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

esp_err_t nand_anonymous_manual_try_init(spi_nand_flash_device_t *dev)
{
    const uint32_t page_size = CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_PAGE_SIZE;
    const uint32_t pages_per_block = CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_PAGES_PER_BLOCK;
    const uint32_t num_blocks = CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_NUM_BLOCKS;
    const uint32_t num_planes = CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_NUM_PLANES;
    const uint32_t t_r_us = CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_T_R_US;
    const uint32_t t_prog_us = CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_T_PROG_US;
    const uint32_t t_bers_us = CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_T_BERS_US;

    ESP_RETURN_ON_ERROR(validate_manual_geometry(page_size, pages_per_block, num_blocks,
                        num_planes, t_r_us, t_prog_us, t_bers_us),
                        TAG, "manual geometry validation failed");

    dev->chip.log2_page_size = (uint8_t)log2_u32(page_size);
    dev->chip.log2_ppb = (uint8_t)log2_u32(pages_per_block);
    dev->chip.num_blocks = num_blocks;
    dev->chip.read_page_delay_us = t_r_us;
    dev->chip.program_page_delay_us = t_prog_us;
    dev->chip.erase_block_delay_us = t_bers_us;
    dev->chip.num_planes = num_planes;
    if (num_planes > 1) {
        dev->chip.flags |= NAND_FLAG_HAS_PROG_PLANE_SELECT | NAND_FLAG_HAS_READ_PLANE_SELECT;
    }
    dev->chip.has_quad_enable_bit = 0;
    dev->chip.quad_enable_bit_pos = 0;

    dev->chip_source = NAND_CHIP_SOURCE_MANUAL;
    dev->chip_detection_flags |= SPI_NAND_CHIP_FLAG_ANONYMOUS;

    strncpy(dev->device_info.chip_name, "manual", sizeof(dev->device_info.chip_name) - 1);
    dev->device_info.chip_name[sizeof(dev->device_info.chip_name) - 1] = '\0';

    ESP_LOGW(TAG, "Chip configured from manual Kconfig values; verify against datasheet before production use");

    return ESP_OK;
}
