/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_check.h"
#include "esp_log.h"
#include "nand.h"
#include "spi_nand_oper.h"
#include "nand_flash_devices.h"
#include "nand_device_types.h"

static const char *TAG = "nand_onfi";

extern esp_err_t nand_read_parameter_page(spi_nand_flash_device_t *handle, nand_parameter_page_t *param_page);

int log2_rightshift(int n)
{
    int count = 0;
    n = n >> 1;  // Shift once because we're counting shifts
    while (n != 0) {
        n = n >> 1;
        count++;
    }
    return count;
}

esp_err_t spi_nand_onfi_init(spi_nand_flash_device_t *dev)
{
    esp_err_t ret = ESP_OK;
    nand_parameter_page_t param_page = {0};
    ESP_RETURN_ON_ERROR(nand_read_parameter_page(dev, &param_page), TAG, "%s, Failed to read parameter page %d", __func__, ret);
    strncpy(dev->device_info.chip_name, param_page.manufacturer, 12);

    dev->chip.has_quad_enable_bit = 1;
    dev->chip.quad_enable_bit_pos = 0;
    dev->chip.read_page_delay_us = param_page.t_r_max_us;
    dev->chip.erase_block_delay_us = param_page.t_bers_max_us;
    dev->chip.program_page_delay_us = param_page.t_prog_max_us;
    uint32_t block_num = param_page.blocks_per_lun * param_page.num_luns;
    dev->chip.num_blocks = block_num;
    dev->chip.log2_ppb = log2_rightshift(param_page.pages_per_block);
    dev->chip.log2_page_size = log2_rightshift(param_page.data_bytes_per_page);

    dev->chip.flags |= SPI_NAND_CHIP_FLAG_GENERIC;
    dev->chip_source = SPI_NAND_CHIP_SOURCE_ONFI;
    ESP_LOGW(TAG, "Chip detected via ONFI parameter page");
    ESP_LOGW(TAG, "Verify chip parameters before production use");

    return ESP_OK;
}
