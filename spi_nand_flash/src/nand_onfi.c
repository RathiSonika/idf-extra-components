/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "nand.h"
#include "nand_onfi.h"
#include "nand_onfi_param_page.h"
#include "nand_onfi_crc.h"

static const char *TAG = "nand_onfi";

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

static esp_err_t parse_parameter_page(spi_nand_flash_device_t *dev, const nand_parameter_page_t *param)
{
    if (memcmp(param->signature, "ONFI", NAND_ONFI_PARAM_PAGE_SIGNATURE_LEN) != 0) {
        return ESP_ERR_NOT_FOUND;
    }
    if (!nand_onfi_param_page_crc_valid((const uint8_t *)param, NAND_ONFI_PARAM_PAGE_SIZE)) {
        return ESP_ERR_INVALID_CRC;
    }
    if (param->num_luns != 1) {
        ESP_LOGW(TAG, "ONFI parameter page: num_luns=%u (only single-LUN supported)", param->num_luns);
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (!is_power_of_two(param->data_bytes_per_page) || !is_power_of_two(param->pages_per_block)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (param->data_bytes_per_page == 0 || param->pages_per_block == 0 || param->blocks_per_lun == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    dev->chip.log2_page_size = (uint8_t)log2_u32(param->data_bytes_per_page);
    dev->chip.log2_ppb = (uint8_t)log2_u32(param->pages_per_block);
    dev->chip.num_blocks = param->blocks_per_lun;
    dev->chip.read_page_delay_us = param->t_r_max_us;
    dev->chip.erase_block_delay_us = param->t_bers_max_us;
    dev->chip.program_page_delay_us = param->t_prog_max_us;
    dev->chip.num_planes = 1;
    dev->chip.has_quad_enable_bit = 0;
    dev->chip.quad_enable_bit_pos = 0;

    dev->device_info.manufacturer_id = param->jedec_id;
    snprintf(dev->device_info.chip_name, sizeof(dev->device_info.chip_name),
             "%.12s %.20s", param->manufacturer, param->model);
    dev->device_info.chip_name[sizeof(dev->device_info.chip_name) - 1] = '\0';

    if (param->ecc_correctability > 0) {
        dev->chip.ecc_data.ecc_data_refresh_threshold =
            (param->ecc_correctability >= 8) ? 6 : 4;
    }

    ESP_LOGD(TAG, "ONFI spare_bytes_per_page=%u (not used for geometry)",
             (unsigned)param->spare_bytes_per_page);

    return ESP_OK;
}

esp_err_t nand_onfi_try_init(spi_nand_flash_device_t *dev)
{
    esp_err_t ret;
    uint8_t *raw_buf = heap_caps_malloc(NAND_ONFI_PARAM_PAGE_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(raw_buf != NULL, ESP_ERR_NO_MEM, TAG, "parameter page buffer alloc failed");

    ret = nand_onfi_read_parameter_page(dev, raw_buf, NAND_ONFI_PARAM_PAGE_SIZE);
    if (ret != ESP_OK) {
        free(raw_buf);
        return ret;
    }

    ret = parse_parameter_page(dev, (const nand_parameter_page_t *)raw_buf);
    free(raw_buf);
    if (ret != ESP_OK) {
        return ret;
    }

    dev->chip_source = NAND_CHIP_SOURCE_ONFI;
    dev->chip_detection_flags |= SPI_NAND_CHIP_FLAG_ANONYMOUS;

    ESP_LOGW(TAG, "Chip detected via ONFI parameter page; verify parameters before production use");

    return ESP_OK;
}
