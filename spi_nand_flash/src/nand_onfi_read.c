/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_heap_caps.h"
#include "spi_nand_oper.h"
#include "nand_onfi_param_page.h"
#include "nand_onfi_crc.h"

#define REG_CONFIG_OTP_EN   (1 << 6)

#define PARAM_PAGE_ROW_ADDR_1  0x000000
#define PARAM_PAGE_ROW_ADDR_2  0x000001
#define PARAM_PAGE_ROW_ADDR_3  0x000004

static const uint32_t param_page_row_addrs[] = {
    PARAM_PAGE_ROW_ADDR_1,
    PARAM_PAGE_ROW_ADDR_2,
    PARAM_PAGE_ROW_ADDR_3,
};

static const uint8_t onfi_signature[NAND_ONFI_PARAM_PAGE_SIGNATURE_LEN] = { 'O', 'N', 'F', 'I' };

#define PARAM_PAGE_NUM_ADDRS  (sizeof(param_page_row_addrs) / sizeof(param_page_row_addrs[0]))

static bool is_onfi_signature_valid(const uint8_t *page_data)
{
    return memcmp(page_data, onfi_signature, NAND_ONFI_PARAM_PAGE_SIGNATURE_LEN) == 0;
}

static esp_err_t wait_not_busy(spi_nand_flash_device_t *handle)
{
    int polls = 0;
    while (polls < 10000) {
        uint8_t status;
        esp_err_t ret = spi_nand_read_register(handle, REG_STATUS, &status);
        if (ret != ESP_OK) {
            return ret;
        }
        if ((status & STAT_BUSY) == 0) {
            return ESP_OK;
        }
        polls++;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t nand_onfi_read_parameter_page(spi_nand_flash_device_t *handle, uint8_t *data, uint16_t length)
{
    esp_err_t ret;
    uint8_t orig_config = 0;

    if (handle == NULL || data == NULL || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = spi_nand_read_register(handle, REG_CONFIG, &orig_config);
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t *probe_buf = heap_caps_malloc(NAND_ONFI_PARAM_PAGE_PROBE_BUF_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (probe_buf == NULL) {
        return ESP_ERR_NO_MEM;
    }

    uint8_t new_config = (uint8_t)(orig_config | REG_CONFIG_OTP_EN);
    ret = spi_nand_write_register(handle, REG_CONFIG, new_config);
    if (ret != ESP_OK) {
        free(probe_buf);
        return ret;
    }

    ret = ESP_ERR_NOT_FOUND;

    for (size_t addr = 0; addr < PARAM_PAGE_NUM_ADDRS; addr++) {
        spi_nand_transaction_t t = {
            .command = CMD_PAGE_READ,
            .address_bytes = 3,
            .address = param_page_row_addrs[addr],
        };
        if (spi_nand_execute_transaction(handle, &t) != ESP_OK) {
            continue;
        }

        if (wait_not_busy(handle) != ESP_OK) {
            continue;
        }

        for (int copy = 0; copy < NAND_ONFI_PARAM_PAGE_COPIES; copy++) {
            uint16_t column = (uint16_t)(copy * NAND_ONFI_PARAM_PAGE_SIZE);

            if (spi_nand_read_sio(handle, probe_buf, column, NAND_ONFI_PARAM_PAGE_SIZE) != ESP_OK) {
                continue;
            }
            if (!is_onfi_signature_valid(probe_buf)) {
                continue;
            }
            if (!nand_onfi_param_page_crc_valid(probe_buf, NAND_ONFI_PARAM_PAGE_SIZE)) {
                continue;
            }

            uint16_t copy_len = length >= NAND_ONFI_PARAM_PAGE_SIZE ? NAND_ONFI_PARAM_PAGE_SIZE : length;
            memcpy(data, probe_buf, copy_len);
            ret = ESP_OK;
            goto restore;
        }
    }

restore:
    spi_nand_write_register(handle, REG_CONFIG, orig_config);
    free(probe_buf);
    return ret;
}
