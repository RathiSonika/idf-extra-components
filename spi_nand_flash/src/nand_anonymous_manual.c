/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"
#include <string.h>
#include "esp_log.h"
#include "nand.h"
#include "nand_anonymous_manual.h"

static const char *TAG = "nand_manual";

#define NAND_MANUAL_IS_POT2(n) ((n) != 0 && (((n) & ((n) - 1)) == 0))

#define NAND_MANUAL_ASSERT_POT2_IN_RANGE(val, min_val, max_val, name) \
    _Static_assert((val) >= (min_val), name " must be >= " #min_val); \
    _Static_assert((val) <= (max_val), name " must be <= " #max_val); \
    _Static_assert(NAND_MANUAL_IS_POT2(val), name " must be a power of two")

#define NAND_MANUAL_ASSERT_POSITIVE(val, name) \
    _Static_assert((val) > 0, name " must be > 0")

NAND_MANUAL_ASSERT_POT2_IN_RANGE(CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_PAGE_SIZE, 512, 8192,
                                 "CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_PAGE_SIZE");
NAND_MANUAL_ASSERT_POT2_IN_RANGE(CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_PAGES_PER_BLOCK, 32, 256,
                                 "CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_PAGES_PER_BLOCK");
NAND_MANUAL_ASSERT_POSITIVE(CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_NUM_BLOCKS,
                            "CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_NUM_BLOCKS");
NAND_MANUAL_ASSERT_POT2_IN_RANGE(CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_NUM_PLANES, 1, 4,
                                 "CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_NUM_PLANES");
NAND_MANUAL_ASSERT_POSITIVE(CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_T_R_US,
                            "CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_T_R_US");
NAND_MANUAL_ASSERT_POSITIVE(CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_T_PROG_US,
                            "CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_T_PROG_US");
NAND_MANUAL_ASSERT_POSITIVE(CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_T_BERS_US,
                            "CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_T_BERS_US");

esp_err_t nand_anonymous_manual_try_init(spi_nand_flash_device_t *dev)
{
    const uint32_t page_size = CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_PAGE_SIZE;
    const uint32_t pages_per_block = CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_PAGES_PER_BLOCK;
    const uint32_t num_blocks = CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_NUM_BLOCKS;
    const uint32_t num_planes = CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_NUM_PLANES;
    const uint32_t t_r_us = CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_T_R_US;
    const uint32_t t_prog_us = CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_T_PROG_US;
    const uint32_t t_bers_us = CONFIG_NAND_FLASH_ANONYMOUS_MANUAL_T_BERS_US;

    dev->chip.log2_page_size = nand_log2_u32(page_size);
    dev->chip.log2_ppb = nand_log2_u32(pages_per_block);
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

    dev->chip_source = SPI_NAND_CHIP_SOURCE_MANUAL;
    dev->chip_detection_flags |= SPI_NAND_CHIP_FLAG_ANONYMOUS;

    strncpy(dev->device_info.chip_name, "manual", sizeof(dev->device_info.chip_name) - 1);
    dev->device_info.chip_name[sizeof(dev->device_info.chip_name) - 1] = '\0';

    ESP_LOGW(TAG, "Chip configured from manual Kconfig values; verify against datasheet before production use");

    return ESP_OK;
}
