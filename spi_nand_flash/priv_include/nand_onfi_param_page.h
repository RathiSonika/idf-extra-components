/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NAND_ONFI_PARAM_PAGE_SIGNATURE_LEN      4
#define NAND_ONFI_PARAM_PAGE_MANUFACTURER_LEN   12
#define NAND_ONFI_PARAM_PAGE_MODEL_LEN          20
#define NAND_ONFI_PARAM_PAGE_SIZE               256
#define NAND_ONFI_PARAM_PAGE_COPIES             3

/** Worst-case heap for one ONFI parameter page read (bytes). */
#define NAND_ONFI_PARAM_PAGE_PROBE_BUF_SIZE     NAND_ONFI_PARAM_PAGE_SIZE

typedef struct __attribute__((packed))
{
    uint8_t  signature[4];
    uint16_t revision;
    uint16_t features;
    uint16_t reserved_8;
    uint8_t  reserved_10_31[22];
    char     manufacturer[NAND_ONFI_PARAM_PAGE_MANUFACTURER_LEN];
    char     model[NAND_ONFI_PARAM_PAGE_MODEL_LEN];
    uint8_t  jedec_id;
    uint16_t date_code;
    uint8_t  reserved_67_79[13];
    uint32_t data_bytes_per_page;
    uint16_t spare_bytes_per_page;
    uint32_t data_bytes_per_partial;
    uint16_t spare_bytes_per_partial;
    uint32_t pages_per_block;
    uint32_t blocks_per_lun;
    uint8_t  num_luns;
    uint8_t  reserved_101;
    uint8_t  bits_per_cell;
    uint16_t max_bad_blocks_per_lun;
    uint16_t block_endurance;
    uint8_t  guaranteed_valid_blocks;
    uint16_t guaranteed_block_endurance;
    uint8_t  programs_per_page;
    uint8_t  partial_prog_attr;
    uint8_t  ecc_correctability;
    uint8_t  interleaved_addr_bits;
    uint8_t  interleaved_op_attr;
    uint8_t  reserved_115_127[13];
    uint8_t  io_capacitance;
    uint16_t io_clock_support;
    uint16_t reserved_131_132;
    uint16_t t_prog_max_us;
    uint16_t t_bers_max_us;
    uint16_t t_r_max_us;
    uint16_t reserved_139_140;
    uint8_t  reserved_141_163[23];
    uint16_t vendor_revision;
    uint8_t  vendor_specific[88];
    uint16_t crc;
} nand_parameter_page_t;

#ifdef __cplusplus
}
#endif
