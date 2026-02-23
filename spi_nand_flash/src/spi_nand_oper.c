/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2023 Espressif Systems (Shanghai) CO LTD
 */

#include <string.h>
#include "spi_nand_oper.h"
#include "driver/spi_master.h"
#if SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE == 1
#include "esp_private/esp_cache_private.h"
#endif

esp_err_t spi_nand_execute_transaction(spi_nand_flash_device_t *handle, spi_nand_transaction_t *transaction)
{
    uint8_t half_duplex = handle->config.flags & SPI_DEVICE_HALFDUPLEX;
    if (!half_duplex) {
        uint32_t len = transaction->miso_len > transaction->mosi_len ? transaction->miso_len : transaction->mosi_len;
        transaction->miso_len = len;
        transaction->mosi_len = len;
    }

    spi_transaction_ext_t e = {
        .base = {
            .flags = SPI_TRANS_VARIABLE_ADDR |  SPI_TRANS_VARIABLE_CMD |  SPI_TRANS_VARIABLE_DUMMY | transaction->flags,
            .rxlength = transaction->miso_len * 8,
            .rx_buffer = transaction->miso_data,
            .length = transaction->mosi_len * 8,
            .tx_buffer = transaction->mosi_data,
            .addr = transaction->address,
            .cmd = transaction->command
        },
        .address_bits = transaction->address_bytes * 8,
        .command_bits = 8,
        .dummy_bits = transaction->dummy_bits
    };

    if (transaction->flags & SPI_TRANS_USE_TXDATA) {
        assert(transaction->mosi_len <= 4 && "SPI_TRANS_USE_TXDATA used for a long transaction");
        memcpy(e.base.tx_data, transaction->mosi_data, transaction->mosi_len);
    }
    if (transaction->flags & SPI_TRANS_USE_RXDATA) {
        assert(transaction->miso_len <= 4 && "SPI_TRANS_USE_RXDATA used for a long transaction");
    }

    esp_err_t ret = spi_device_transmit(handle->config.device_handle, (spi_transaction_t *) &e);
    if (ret == ESP_OK) {
        if (transaction->flags == SPI_TRANS_USE_RXDATA) {
            memcpy(transaction->miso_data, e.base.rx_data, transaction->miso_len);
        }
    }
    return ret;
}

esp_err_t spi_nand_read_manufacturer_id(spi_nand_flash_device_t *handle, uint8_t *manufacturer_id)
{
    esp_err_t ret = ESP_OK;
    spi_nand_transaction_t t = {
        .command = CMD_READ_ID,
        .address = 0, // This normally selects the manufacturer id. Some chips ignores it, but still expects 8 dummy bits here
        .address_bytes = 1,
        .miso_len = 1,
        .miso_data = manufacturer_id,
        .flags = SPI_TRANS_USE_RXDATA,
    };
    ret = spi_nand_execute_transaction(handle, &t);
    return ret;
}

esp_err_t spi_nand_read_device_id(spi_nand_flash_device_t *handle, uint8_t *device_id, uint8_t length)
{
    esp_err_t ret = ESP_OK;
    spi_nand_transaction_t t = {
        .command = CMD_READ_ID,
        .address = 0,
        .address_bytes = 2,
        .miso_len = length,
        .miso_data = device_id,
        .flags = SPI_TRANS_USE_RXDATA,
    };
    ret = spi_nand_execute_transaction(handle, &t);
    return ret;
}

esp_err_t spi_nand_read_register(spi_nand_flash_device_t *handle, uint8_t reg, uint8_t *val)
{
    spi_nand_transaction_t t = {
        .command = CMD_READ_REGISTER,
        .address_bytes = 1,
        .address = reg,
        .miso_len = 1,
        .miso_data = val,
        .flags = SPI_TRANS_USE_RXDATA,
    };

    return spi_nand_execute_transaction(handle, &t);
}

esp_err_t spi_nand_write_register(spi_nand_flash_device_t *handle, uint8_t reg, uint8_t val)
{
    spi_nand_transaction_t t = {
        .command = CMD_SET_REGISTER,
        .address_bytes = 1,
        .address = reg,
        .mosi_len = 1,
        .mosi_data = &val,
        .flags = SPI_TRANS_USE_TXDATA,
    };

    return spi_nand_execute_transaction(handle, &t);
}

esp_err_t spi_nand_write_enable(spi_nand_flash_device_t *handle)
{
    spi_nand_transaction_t t = {
        .command = CMD_WRITE_ENABLE
    };

    return spi_nand_execute_transaction(handle, &t);
}

esp_err_t spi_nand_read_page(spi_nand_flash_device_t *handle, uint32_t page)
{
    spi_nand_transaction_t t = {
        .command = CMD_PAGE_READ,
        .address_bytes = 3,
        .address = page
    };

    return spi_nand_execute_transaction(handle, &t);
}

static uint16_t check_length_alignment(spi_nand_flash_device_t *handle, uint16_t length)
{
    size_t alignment;
    uint16_t data_len = length;

#if SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE == 1
    esp_cache_get_alignment(MALLOC_CAP_DMA, &alignment);
#else
    // For non-L1CACHE targets, use DMA alignment of 4 bytes
    alignment = 4;
#endif

    bool is_length_unaligned = (length & (alignment - 1)) ? true : false;
    if (is_length_unaligned) {
        if (length < alignment) {
            data_len = ((length + alignment) & ~(alignment - 1));
        } else {
            data_len = ((length + (alignment - 1)) & ~(alignment - 1));
        }
    }
    if (!(handle->config.flags & SPI_DEVICE_HALFDUPLEX)) {
        data_len = data_len + alignment;
    }
    return data_len;
}

static esp_err_t spi_nand_quad_read(spi_nand_flash_device_t *handle, uint8_t *data, uint16_t column, uint16_t length)
{
    uint32_t spi_flags = SPI_TRANS_MODE_QIO;
    uint8_t cmd = CMD_READ_X4;
    uint8_t dummy_bits = 8;

    uint8_t *data_read = data;
    uint16_t data_read_len = length;

    // Check if length needs alignment for DMA
    uint16_t aligned_len = check_length_alignment(handle, length);
    if (aligned_len != length) {
        data_read = handle->temp_buffer;
        data_read_len = aligned_len;
    }

    if (handle->config.io_mode == SPI_NAND_IO_MODE_QIO) {
        spi_flags |= SPI_TRANS_MULTILINE_ADDR;
        cmd = CMD_READ_QIO;
        dummy_bits = 4;
    }

    spi_nand_transaction_t t = {
        .command = cmd,
        .address_bytes = 2,
        .address = column,
        .miso_len = data_read_len,
        .miso_data = data_read,
        .dummy_bits = dummy_bits,
        .flags = spi_flags,
    };

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    t.flags |= SPI_TRANS_DMA_BUFFER_ALIGN_MANUAL;
#endif
    esp_err_t ret =  spi_nand_execute_transaction(handle, &t);

    if (ret == ESP_OK && aligned_len != length) {
        memcpy(data, data_read, length);
    }

    return ret;
}

static esp_err_t spi_nand_dual_read(spi_nand_flash_device_t *handle, uint8_t *data, uint16_t column, uint16_t length)
{
    uint32_t spi_flags = SPI_TRANS_MODE_DIO;
    uint8_t cmd = CMD_READ_X2;
    uint8_t dummy_bits = 8;

    uint8_t *data_read = data;
    uint16_t data_read_len = length;

    // Check if length needs alignment for DMA
    uint16_t aligned_len = check_length_alignment(handle, length);
    if (aligned_len != length) {
        data_read = handle->temp_buffer;
        data_read_len = aligned_len;
    }

    if (handle->config.io_mode == SPI_NAND_IO_MODE_DIO) {
        spi_flags |= SPI_TRANS_MULTILINE_ADDR;
        cmd = CMD_READ_DIO;
        dummy_bits = 4;
    }

    spi_nand_transaction_t t = {
        .command = cmd,
        .address_bytes = 2,
        .address = column,
        .miso_len = data_read_len,
        .miso_data = data_read,
        .dummy_bits = dummy_bits,
        .flags = spi_flags,
    };

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    t.flags |= SPI_TRANS_DMA_BUFFER_ALIGN_MANUAL;
#endif
    esp_err_t ret =  spi_nand_execute_transaction(handle, &t);

    if (ret == ESP_OK && aligned_len != length) {
        memcpy(data, data_read, length);
    }

    return ret;
}

static esp_err_t spi_nand_fast_read(spi_nand_flash_device_t *handle, uint8_t *data, uint16_t column, uint16_t length)
{
    uint8_t *data_read = data;
    uint16_t data_read_len = length;
    uint8_t half_duplex = handle->config.flags & SPI_DEVICE_HALFDUPLEX;

    // Check if length needs alignment for DMA
    uint16_t aligned_len = check_length_alignment(handle, length);
    if (aligned_len != length) {
        data_read = handle->temp_buffer;
        data_read_len = aligned_len;
    }

    spi_nand_transaction_t t = {
        .command = CMD_READ_FAST,
        .address_bytes = 2,
        .address = column,
        .miso_len = data_read_len,
        .miso_data = data_read,
    };

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    t.flags = SPI_TRANS_DMA_BUFFER_ALIGN_MANUAL;
#endif

    if (half_duplex) {
        t.dummy_bits = 8;
    }
    esp_err_t ret = spi_nand_execute_transaction(handle, &t);
    if (ret != ESP_OK) {
        goto fail;
    }

    if (aligned_len != length) {
        if (!half_duplex) {
            memcpy(data, data_read + 1, length);
        } else {
            memcpy(data, data_read, length);
        }
    }

fail:
    return ret;
}

esp_err_t spi_nand_read(spi_nand_flash_device_t *handle, uint8_t *data, uint16_t column, uint16_t length)
{
    if (handle->config.io_mode == SPI_NAND_IO_MODE_DOUT || handle->config.io_mode == SPI_NAND_IO_MODE_DIO) {
        return spi_nand_dual_read(handle, data, column, length);
    } else if (handle->config.io_mode == SPI_NAND_IO_MODE_QOUT || handle->config.io_mode == SPI_NAND_IO_MODE_QIO) {
        return spi_nand_quad_read(handle, data, column, length);
    }
    return spi_nand_fast_read(handle, data, column, length);
}

esp_err_t spi_nand_program_execute(spi_nand_flash_device_t *handle, uint32_t page)
{
    spi_nand_transaction_t t = {
        .command = CMD_PROGRAM_EXECUTE,
        .address_bytes = 3,
        .address = page
    };

    return spi_nand_execute_transaction(handle, &t);
}

esp_err_t spi_nand_program_load(spi_nand_flash_device_t *handle, const uint8_t *data, uint16_t column, uint16_t length)
{
    uint8_t cmd = CMD_PROGRAM_LOAD;
    uint32_t spi_flags = 0;
    if (handle->config.io_mode == SPI_NAND_IO_MODE_QOUT || handle->config.io_mode == SPI_NAND_IO_MODE_QIO) {
        cmd = CMD_PROGRAM_LOAD_X4;
        spi_flags = SPI_TRANS_MODE_QIO;
    }

    const uint8_t *data_write = data;
    uint16_t data_write_len = length;

    spi_nand_transaction_t t = {
        .command = cmd,
        .address_bytes = 2,
        .address = column,
        .mosi_len = data_write_len,
        .mosi_data = data_write,
        .flags = spi_flags,
    };

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    // Check if length needs alignment for DMA
    uint16_t aligned_len = check_length_alignment(handle, length);

    if (aligned_len == length) {
        t.flags |= SPI_TRANS_DMA_BUFFER_ALIGN_MANUAL;
    }
#endif

    return spi_nand_execute_transaction(handle, &t);
}

esp_err_t spi_nand_erase_block(spi_nand_flash_device_t *handle, uint32_t page)
{
    spi_nand_transaction_t  t = {
        .command = CMD_ERASE_BLOCK,
        .address_bytes = 3,
        .address = page
    };

    return spi_nand_execute_transaction(handle, &t);
}

/* Parameter page can be at one of three row addresses depending on vendor (ONFI). */
static const uint32_t param_page_row_addrs[] = {
    PARAM_PAGE_ROW_ADDR_1,
    PARAM_PAGE_ROW_ADDR_2,
    PARAM_PAGE_ROW_ADDR_3,
};

static const uint8_t onfi_signature[NAND_PARAM_PAGE_SIGNATURE_LEN] = { 'O', 'N', 'F', 'I' };

static bool is_onfi_signature_valid(const uint8_t *page_data)
{
    return memcmp(page_data, onfi_signature, NAND_PARAM_PAGE_SIGNATURE_LEN) == 0;
}

/**
 * ONFI CRC-16 over parameter page (bytes 0-253); stored in bytes 254-255.
 * Polynomial: 0x8005, initial value: 0x4F4E.
 */
static uint16_t param_page_crc16(const uint8_t *data, size_t length)
{
    const uint16_t poly = 0x8005;
    uint16_t crc = 0x4F4E;

    for (size_t i = 0; i < length; i++) {
        crc ^= ((uint16_t)data[i] << 8);
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ poly;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

static bool is_param_page_crc_valid(const uint8_t *page_data)
{
    uint16_t computed = param_page_crc16(page_data, NAND_PARAM_PAGE_SIZE - 2);
    uint16_t stored = (uint16_t)page_data[254] | ((uint16_t)page_data[255] << 8);
    return computed == stored;
}

#define PARAM_PAGE_NUM_ADDRS  (sizeof(param_page_row_addrs) / sizeof(param_page_row_addrs[0]))

esp_err_t spi_nand_read_parameter_page(spi_nand_flash_device_t *handle, uint8_t *data, uint16_t length)
{
    esp_err_t ret = ESP_OK;
    uint8_t orig_config = 0;
    uint8_t *probe_buf = (uint8_t *)heap_caps_malloc(256 * 3, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);

    // Step 1: Read current REG_CONFIG (0xB0) value and save it
    ret = spi_nand_read_register(handle, REG_CONFIG, &orig_config);
    if (ret != ESP_OK) {
        return ret;
    }

    // Step 2: Set OTP_EN bit to access the OTP/parameter page area
    uint8_t new_config = orig_config | REG_CONFIG_OTP_EN;
    ret = spi_nand_write_register(handle, REG_CONFIG, new_config);
    if (ret != ESP_OK) {
        return ret;
    }

    // Outer loop: try each possible row address (one page contains 3 copies)
    for (int addr = 0; addr < (int)PARAM_PAGE_NUM_ADDRS; addr++) {
        // One PAGE READ loads the full page (with 3 copies) into cache
        spi_nand_transaction_t t = {
            .command = CMD_PAGE_READ,
            .address_bytes = 3,
            .address = param_page_row_addrs[addr],
        };
        ret = spi_nand_execute_transaction(handle, &t);
        if (ret != ESP_OK) {
            goto restore;
        }

        while (true) {
            uint8_t status;
            ret = spi_nand_read_register(handle, REG_STATUS, &status);
            if (ret != ESP_OK) {
                goto restore;
            }
            if ((status & STAT_BUSY) == 0) {
                break;
            }
        }

        // Inner loop: check each of the 3 redundant copies in this page
        for (int copy = 0; copy < NAND_PARAM_PAGE_COPIES; copy++) {
            uint16_t column = (uint16_t)(copy * NAND_PARAM_PAGE_SIZE);

            ret = spi_nand_read(handle, probe_buf, column, (uint16_t)NAND_PARAM_PAGE_SIZE);
            if (ret != ESP_OK) {
                goto restore;
            }

            if (!is_onfi_signature_valid(probe_buf)) {
                continue;
            }
            if (!is_param_page_crc_valid(probe_buf)) {
                continue;
            }

            // Valid copy found
            uint16_t copy_len = (length >= (uint16_t)NAND_PARAM_PAGE_SIZE)
                                ? (uint16_t)NAND_PARAM_PAGE_SIZE
                                : length;
            memcpy(data, probe_buf, copy_len);
            ret = ESP_OK;
            goto restore;
        }
        // All 3 copies at this row address failed; try next address
    }

    ret = ESP_ERR_INVALID_CRC;

restore:
    free(probe_buf);
    // Step 3: Restore original REG_CONFIG value (clear OTP_EN)
    spi_nand_write_register(handle, REG_CONFIG, orig_config);

    return ret;
}
