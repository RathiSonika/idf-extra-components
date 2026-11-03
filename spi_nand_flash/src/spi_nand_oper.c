/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2026 Espressif Systems (Shanghai) CO LTD
 */

#include <string.h>
#include "spi_nand_oper.h"
#include "spi_nand_transport.h"
#include "nand.h"
#include "esp_idf_version.h"
#include "esp_memory_utils.h"
#if SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE == 1
#include "esp_private/esp_cache_private.h"
#endif

esp_err_t spi_nand_execute_transaction(spi_nand_flash_device_t *handle, spi_nand_transaction_t *transaction)
{
    spi_nand_transport_t *tr = handle->transport;
    if (tr == NULL || tr->transmit == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_OK;
    if (tr->lock) {
        ret = tr->lock(tr);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    ret = tr->transmit(tr, transaction);

    if (tr->unlock) {
        tr->unlock(tr);
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
        .flags = SPI_NAND_TRANS_USE_RXDATA,
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
        .flags = SPI_NAND_TRANS_USE_RXDATA,
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
        .flags = SPI_NAND_TRANS_USE_RXDATA,
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
        .flags = SPI_NAND_TRANS_USE_TXDATA,
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

size_t spi_nand_get_dma_alignment(void)
{
    size_t alignment;
#if SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE == 1
    esp_cache_get_alignment(MALLOC_CAP_DMA, &alignment);
#else
    // For non-L1CACHE targets, use DMA alignment of 4 bytes
    alignment = 4;
#endif
    return alignment;
}

static uint16_t check_length_alignment(spi_nand_flash_device_t *handle, uint16_t length, size_t alignment)
{
    uint16_t data_len = length;

    bool is_length_unaligned = (length & (alignment - 1)) ? true : false;
    if (is_length_unaligned) {
        if (length < alignment) {
            data_len = ((length + alignment) & ~(alignment - 1));
        } else {
            data_len = ((length + (alignment - 1)) & ~(alignment - 1));
        }
    }
    if (handle->transport && !handle->transport->is_half_duplex(handle->transport)) {
        data_len = data_len + alignment;
    }
    return data_len;
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
static bool spi_nand_buf_dma_aligned(const void *buf, size_t alignment)
{
    return esp_ptr_dma_capable(buf) && (((uintptr_t)buf % alignment) == 0);
}

/**
 * Prepare the TX buffer for a NAND program-load SPI transaction.
 *
 * When @p length is DMA-aligned but @p user_buf is not DMA-capable or not properly
 * aligned (ESP-IDF >= 5.2), data is copied into handle->temp_buffer and
 * @p *out_manual_dma is set so the caller sets SPI_NAND_TRANS_DMA_BUFFER_ALIGN_MANUAL.
 *
 * When @p length is not DMA-aligned we cannot pad the write (extra bytes would be
 * programmed into the NAND page); @p *out_manual_dma stays false and the SPI driver
 * handles alignment internally.
 */
static void spi_nand_tx_prepare_write_buffers(spi_nand_flash_device_t *handle,
                                              const uint8_t *user_buf, uint16_t length,
                                              const uint8_t **out_data_write, bool *out_manual_dma)
{
    *out_data_write = user_buf;
    *out_manual_dma = false;

    size_t alignment = spi_nand_get_dma_alignment();
    uint16_t aligned_len = check_length_alignment(handle, length, alignment);
    if (aligned_len != length) {
        return;
    }

    if (!spi_nand_buf_dma_aligned(user_buf, alignment)) {
        memcpy(handle->temp_buffer, user_buf, length);
        *out_data_write = handle->temp_buffer;
    }
    *out_manual_dma = true;
}
#endif // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)

/**
 * Prepare the RX buffer for a NAND read SPI transaction.
 *
 * Decides whether the caller's buffer can be used directly for DMA, or whether the
 * receive must be bounced through handle->temp_buffer.
 *
 * Bounce is needed when:
 *  1. @p length is not DMA-aligned — the padded amount is read into temp_buffer
 *     (*out_data_read_len > length).
 *  2. @p length is DMA-aligned but @p user_buf is not DMA-capable or not properly
 *     aligned (ESP-IDF >= 5.2) — the original length is read into temp_buffer
 *     (*out_data_read_len == length).
 *
 * When *out_data_read != @p user_buf after a successful transaction, copy the useful
 * data back into @p user_buf. The copy is not always from offset 0: in full-duplex
 * fast read, byte 0 of the receive buffer is a dummy clocked during the command /
 * address phase and must be skipped (see spi_nand_fast_read).
 */
static void spi_nand_rx_prepare_read_buffers(spi_nand_flash_device_t *handle, uint8_t *user_buf, uint16_t length,
                                             uint8_t **out_data_read, uint16_t *out_data_read_len)
{
    size_t alignment = spi_nand_get_dma_alignment();
    uint16_t aligned_len = check_length_alignment(handle, length, alignment);

    if (aligned_len != length) {
        *out_data_read = handle->temp_buffer;
        *out_data_read_len = aligned_len;
        return;
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    if (!spi_nand_buf_dma_aligned(user_buf, alignment)) {
        *out_data_read = handle->temp_buffer;
        *out_data_read_len = length;
        return;
    }
#endif

    *out_data_read = user_buf;
    *out_data_read_len = length;
}

static esp_err_t spi_nand_quad_read(spi_nand_flash_device_t *handle, uint8_t *data, uint16_t column, uint16_t length)
{
    uint32_t spi_flags = SPI_NAND_TRANS_MODE_QIO;
    uint8_t cmd = CMD_READ_X4;
    uint8_t dummy_bits = 8;

    uint8_t *data_read;
    uint16_t data_read_len;

    spi_nand_rx_prepare_read_buffers(handle, data, length, &data_read, &data_read_len);

    if (handle->config.io_mode == SPI_NAND_IO_MODE_QIO) {
        spi_flags |= SPI_NAND_TRANS_MULTILINE_ADDR;
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
    t.flags |= SPI_NAND_TRANS_DMA_BUFFER_ALIGN_MANUAL;
#endif
    esp_err_t ret = spi_nand_execute_transaction(handle, &t);

    if (ret == ESP_OK && (data_read != data)) {
        memcpy(data, data_read, length);
    }

    return ret;
}

static esp_err_t spi_nand_dual_read(spi_nand_flash_device_t *handle, uint8_t *data, uint16_t column, uint16_t length)
{
    uint32_t spi_flags = SPI_NAND_TRANS_MODE_DIO;
    uint8_t cmd = CMD_READ_X2;
    uint8_t dummy_bits = 8;

    uint8_t *data_read;
    uint16_t data_read_len;

    spi_nand_rx_prepare_read_buffers(handle, data, length, &data_read, &data_read_len);

    if (handle->config.io_mode == SPI_NAND_IO_MODE_DIO) {
        spi_flags |= SPI_NAND_TRANS_MULTILINE_ADDR;
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
    t.flags |= SPI_NAND_TRANS_DMA_BUFFER_ALIGN_MANUAL;
#endif
    esp_err_t ret = spi_nand_execute_transaction(handle, &t);

    if (ret == ESP_OK && (data_read != data)) {
        memcpy(data, data_read, length);
    }

    return ret;
}

static esp_err_t spi_nand_fast_read(spi_nand_flash_device_t *handle, uint8_t *data, uint16_t column, uint16_t length)
{
    bool half_duplex = handle->transport && handle->transport->is_half_duplex(handle->transport);
    uint8_t *data_read;
    uint16_t data_read_len;

    spi_nand_rx_prepare_read_buffers(handle, data, length, &data_read, &data_read_len);

    spi_nand_transaction_t t = {
        .command = CMD_READ_FAST,
        .address_bytes = 2,
        .address = column,
        .miso_len = data_read_len,
        .miso_data = data_read,
    };

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    t.flags = SPI_NAND_TRANS_DMA_BUFFER_ALIGN_MANUAL;
#endif

    if (half_duplex) {
        t.dummy_bits = 8;
    }
    esp_err_t ret = spi_nand_execute_transaction(handle, &t);
    if (ret != ESP_OK) {
        goto fail;
    }

    if (data_read != data) {
        if (!half_duplex) {
            /* In full-duplex fast read, byte 0 in the receive buffer is a dummy
               clocked in during the command/address phase — skip it.
               check_length_alignment() guarantees data_read_len > length for
               full-duplex (it unconditionally pads by 'alignment'), so the +1
               offset is always within bounds.*/
            assert(data_read_len > length);
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
        spi_flags = SPI_NAND_TRANS_MODE_QIO;
    }

    const uint8_t *data_write = data;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    bool manual_dma = false;
    spi_nand_tx_prepare_write_buffers(handle, data, length, &data_write, &manual_dma);
    if (manual_dma) {
        spi_flags |= SPI_NAND_TRANS_DMA_BUFFER_ALIGN_MANUAL;
    }
#endif

    spi_nand_transaction_t t = {
        .command = cmd,
        .address_bytes = 2,
        .address = column,
        .mosi_len = length,
        .mosi_data = data_write,
        .flags = spi_flags,
    };

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
