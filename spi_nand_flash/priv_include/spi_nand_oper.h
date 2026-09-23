/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2023 Espressif Systems (Shanghai) CO LTD
 */

#pragma once

#include <stdint.h>
#include <esp_err.h>
#include <driver/spi_master.h>
#include "nand.h"

#ifdef __cplusplus
extern "C" {
#endif

struct spi_nand_transaction_t {
    uint8_t command;
    uint8_t address_bytes;
    uint32_t address;
    uint32_t mosi_len;
    const uint8_t *mosi_data;
    uint32_t miso_len;
    uint8_t *miso_data;
    uint32_t dummy_bits;
    uint32_t flags;
};

typedef struct spi_nand_transaction_t spi_nand_transaction_t;

#define CMD_SET_REGISTER    0x1F
#define CMD_READ_REGISTER   0x0F
#define CMD_WRITE_ENABLE    0x06
#define CMD_READ_ID         0x9F
#define CMD_PAGE_READ       0x13
#define CMD_PROGRAM_EXECUTE 0x10
/* Default Program Load opcodes used by most supported vendors (Micron-style
 * random-data form). Some parts (e.g. HeYangTek) require 0x02/0x32 for the
 * first load and reserve 0x84/0x34 for Internal Data Move / random load. */
#define CMD_PROGRAM_LOAD         0x84
#define CMD_PROGRAM_LOAD_X4      0x34
#define CMD_PROGRAM_LOAD_RESET   0x02
#define CMD_PROGRAM_LOAD_RESET_X4 0x32
#define CMD_READ_FAST       0x0B
#define CMD_READ_X2         0x3B
#define CMD_READ_X4         0x6B
#define CMD_ERASE_BLOCK     0xD8
#define CMD_READ_DIO        0xBB
#define CMD_READ_QIO        0xEB

#define REG_PROTECT         0xA0
#define REG_CONFIG          0xB0
#define REG_STATUS          0xC0
#define REG_STATUS_EXT      0xF0

#define STAT_ECCSE_SHIFT    4

#define STAT_BUSY           (1 << 0)
#define STAT_WRITE_ENABLED  (1 << 1)
#define STAT_ERASE_FAILED   (1 << 2)
#define STAT_PROGRAM_FAILED (1 << 3)
#define STAT_ECC0           (1 << 4)
#define STAT_ECC1           (1 << 5)
#define STAT_ECC2           (1 << 6)

size_t spi_nand_get_dma_alignment(void);
esp_err_t spi_nand_execute_transaction(spi_nand_flash_device_t *handle, spi_nand_transaction_t *transaction);

esp_err_t spi_nand_read_manufacturer_id(spi_nand_flash_device_t *handle, uint8_t *manufacturer_id);
esp_err_t spi_nand_read_device_id(spi_nand_flash_device_t *handle, uint8_t *device_id, uint8_t length);
esp_err_t spi_nand_read_register(spi_nand_flash_device_t *handle, uint8_t reg, uint8_t *val);
esp_err_t spi_nand_write_register(spi_nand_flash_device_t *handle, uint8_t reg, uint8_t val);
esp_err_t spi_nand_write_enable(spi_nand_flash_device_t *handle);
esp_err_t spi_nand_read_page(spi_nand_flash_device_t *handle, uint32_t page);
esp_err_t spi_nand_read(spi_nand_flash_device_t *handle, uint8_t *data, uint16_t column, uint16_t length);
esp_err_t spi_nand_program_execute(spi_nand_flash_device_t *handle, uint32_t page);
/**
 * @brief Program Load into cache (first / reset form).
 *
 * Issue at most one reset load per PAGE PROGRAM sequence; use
 * spi_nand_program_load_random() for further column updates.
 * Opcode follows chip flags (default 0x84/0x34; NAND_FLAG_PROG_LOAD_RESET → 0x02/0x32).
 */
esp_err_t spi_nand_program_load(spi_nand_flash_device_t *handle, const uint8_t *data, uint16_t column, uint16_t length);
/**
 * @brief Program Load Random Data into cache (does not reset cache).
 *
 * Opcode is 0x84/0x34 when NAND_FLAG_PROG_LOAD_RESET is set; otherwise same as
 * spi_nand_program_load().
 */
esp_err_t spi_nand_program_load_random(spi_nand_flash_device_t *handle, const uint8_t *data, uint16_t column, uint16_t length);
esp_err_t spi_nand_erase_block(spi_nand_flash_device_t *handle, uint32_t page);

#ifdef __cplusplus
}
#endif
