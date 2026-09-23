/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "esp_check.h"
#include "nand.h"
#include "spi_nand_oper.h"
#include "nand_flash_devices.h"

static const char *TAG = "nand_heyangtek";

esp_err_t spi_nand_heyangtek_init(spi_nand_flash_device_t *dev)
{
    esp_err_t ret = ESP_OK;
    uint8_t device_id = 0;
    ESP_RETURN_ON_ERROR(spi_nand_read_device_id(dev, &device_id, sizeof(device_id)), TAG,
                        "%s, Failed to get the device ID %d", __func__, ret);
    dev->device_info.device_id = device_id;
    snprintf(dev->device_info.chip_name, sizeof(dev->device_info.chip_name),
             "heyangtek-0x%02" PRIx8, device_id);
    ESP_LOGD(TAG, "%s: device_id: %x\n", __func__, device_id);

    /* HYF* datasheet: Program Load is 02h/32h; 84h/34h are IDM / random only. */
    dev->chip.flags |= NAND_FLAG_PROG_LOAD_RESET;

    dev->chip.has_quad_enable_bit = 1;
    dev->chip.quad_enable_bit_pos = 0;  // QE at B0[0]
    /* Typical values from HYF8GQ4UACCAE datasheet programming/erase table */
    dev->chip.read_page_delay_us = 160;
    dev->chip.program_page_delay_us = 420;
    dev->chip.erase_block_delay_us = 4000;

    switch (device_id) {
    case HEYANGTEK_DI_58: // HYF8GQ4UACCAE
        dev->chip.num_blocks = 8192;
        dev->chip.log2_ppb = 6;         // 64 pages per block
        dev->chip.log2_page_size = 11;  // 2048 bytes per page
        break;
    default:
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}
