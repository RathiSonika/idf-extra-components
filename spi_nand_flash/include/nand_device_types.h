/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief NAND Flash ECC status enumeration */
typedef enum {
    NAND_ECC_OK = 0,                     /*!< No ECC errors detected */
    NAND_ECC_1_TO_3_BITS_CORRECTED = 1, /*!< 1-3 bits corrected */
    NAND_ECC_BITS_CORRECTED = NAND_ECC_1_TO_3_BITS_CORRECTED,
    NAND_ECC_NOT_CORRECTED = 2,          /*!< ECC errors not correctable */
    NAND_ECC_4_TO_6_BITS_CORRECTED = 3, /*!< 4-6 bits corrected */
    NAND_ECC_MAX_BITS_CORRECTED = NAND_ECC_4_TO_6_BITS_CORRECTED,
    NAND_ECC_7_8_BITS_CORRECTED = 5,    /*!< 7-8 bits corrected */
    NAND_ECC_MAX
} nand_ecc_status_t;

/** @brief NAND Flash ECC configuration and status */
typedef struct {
    uint8_t ecc_status_reg_len_in_bits;     /*!< Length of ECC status register in bits */
    uint8_t ecc_data_refresh_threshold;     /*!< ECC error threshold for data refresh */
    nand_ecc_status_t ecc_corrected_bits_status; /*!< Current ECC correction status */
} nand_ecc_data_t;

/** @brief NAND Flash chip geometry and characteristics */
typedef struct {
    uint8_t log2_page_size;                 /*!< Page size as power of 2 (e.g., 11 for 2048 bytes) */
    uint8_t log2_ppb;                       /*!< Pages per block as power of 2 (e.g., 6 for 64 pages) */
    uint32_t block_size;                    /*!< Block size in bytes */
    uint32_t page_size;                     /*!< Page size in bytes */
    uint32_t num_blocks;                    /*!< Total number of blocks */
    uint32_t read_page_delay_us;            /*!< Read page delay in microseconds */
    uint32_t erase_block_delay_us;          /*!< Erase block delay in microseconds */
    uint32_t program_page_delay_us;         /*!< Program page delay in microseconds */
    uint32_t num_planes;                    /*!< Number of planes in the flash */
    uint32_t flags;                         /*!< Chip-specific flags (see NAND_FLAG_* and SPI_NAND_CHIP_FLAG_*) */
    nand_ecc_data_t ecc_data;              /*!< ECC configuration and status */
    uint8_t has_quad_enable_bit;           /*!< 1 if chip supports QIO/QOUT mode */
    uint8_t quad_enable_bit_pos;           /*!< Position of quad enable bit */
#ifdef CONFIG_IDF_TARGET_LINUX
    uint32_t emulated_page_size;            /*!< Emulated page size for Linux */
    uint32_t emulated_page_oob;             /*!< Emulated OOB size for Linux */
#endif
} nand_flash_geometry_t;

/** @brief NAND Flash device identification information */
typedef struct {
    uint8_t manufacturer_id;                /*!< Manufacturer ID */
    uint16_t device_id;                     /*!< Device ID */
    char chip_name[32];                     /*!< Chip name string */
} nand_device_info_t;

#define NAND_PARAM_PAGE_SIGNATURE_LEN    4   /*!< Length of ONFI signature ("ONFI") */
#define NAND_PARAM_PAGE_MANUFACTURER_LEN 12  /*!< Length of manufacturer name field */
#define NAND_PARAM_PAGE_MODEL_LEN        20  /*!< Length of device model field */
#define NAND_PARAM_PAGE_SIZE             256 /*!< Size of one parameter page copy in bytes */
#define NAND_PARAM_PAGE_COPIES           3   /*!< Number of redundant parameter page copies */

/** @brief ONFI Parameter Page structure (256 bytes)
 *
 * This structure maps the ONFI-compatible parameter page that describes the
 * chip's organization, features, timing and other behavioral parameters.
 */
typedef struct __attribute__((packed))
{
    /* Revision information (bytes 0-9) */
    uint8_t  signature[4];                  /*!< Bytes 0-3: Parameter page signature "ONFI" */
    uint16_t revision;                      /*!< Bytes 4-5: Revision number */
    uint16_t features;                      /*!< Bytes 6-7: Features supported */
    uint16_t reserved_8;                    /*!< Bytes 8-9: Reserved */

    uint8_t  reserved_10_31[22];            /*!< Bytes 10-31: Reserved */

    /* Manufacturer information block (bytes 32-79) */
    char     manufacturer[12];              /*!< Bytes 32-43: Device manufacturer (ASCII) */
    char     model[20];                     /*!< Bytes 44-63: Device model (ASCII) */
    uint8_t  jedec_id;                      /*!< Byte 64: JEDEC manufacturer ID */
    uint16_t date_code;                     /*!< Bytes 65-66: Date code */
    uint8_t  reserved_67_79[13];            /*!< Bytes 67-79: Reserved */

    /* Memory organization block (bytes 80-127) */
    uint32_t data_bytes_per_page;           /*!< Bytes 80-83: Number of data bytes per page */
    uint16_t spare_bytes_per_page;          /*!< Bytes 84-85: Number of spare bytes per page */
    uint32_t data_bytes_per_partial;        /*!< Bytes 86-89: Number of data bytes per partial page */
    uint16_t spare_bytes_per_partial;       /*!< Bytes 90-91: Number of spare bytes per partial page */
    uint32_t pages_per_block;               /*!< Bytes 92-95: Number of pages per block */
    uint32_t blocks_per_lun;                /*!< Bytes 96-99: Number of blocks per logical unit */
    uint8_t  num_luns;                      /*!< Byte 100: Number of logical units */
    uint8_t  reserved_101;                  /*!< Byte 101: Reserved */
    uint8_t  bits_per_cell;                 /*!< Byte 102: Number of bits per cell */
    uint16_t max_bad_blocks_per_lun;        /*!< Bytes 103-104: Bad blocks maximum per LUN */
    uint16_t block_endurance;               /*!< Bytes 105-106: Block endurance */
    uint8_t  guaranteed_valid_blocks;       /*!< Byte 107: Guaranteed valid blocks at beginning */
    uint16_t guaranteed_block_endurance;    /*!< Bytes 108-109: Block endurance for guaranteed valid blocks */
    uint8_t  programs_per_page;             /*!< Byte 110: Number of programs per page */
    uint8_t  partial_prog_attr;             /*!< Byte 111: Partial programming attributes */
    uint8_t  ecc_correctability;            /*!< Byte 112: Number of bits ECC correctability */
    uint8_t  interleaved_addr_bits;         /*!< Byte 113: Number of interleaved address bits */
    uint8_t  interleaved_op_attr;           /*!< Byte 114: Interleaved operation attributes */
    uint8_t  reserved_115_127[13];          /*!< Bytes 115-127: Reserved */

    /* Electrical parameters block (bytes 128-163) */
    uint8_t  io_capacitance;                /*!< Byte 128: I/O capacitance */
    uint16_t io_clock_support;              /*!< Bytes 129-130: I/O clock support */
    uint16_t reserved_131_132;              /*!< Bytes 131-132: Reserved */
    uint16_t t_prog_max_us;                 /*!< Bytes 133-134: Maximum page program time (us) */
    uint16_t t_bers_max_us;                 /*!< Bytes 135-136: Maximum block erase time (us) */
    uint16_t t_r_max_us;                    /*!< Bytes 137-138: Maximum page read time (us) */
    uint16_t reserved_139_140;              /*!< Bytes 139-140: Reserved */
    uint8_t  reserved_141_163[23];          /*!< Bytes 141-163: Reserved */

    /* Vendor block (bytes 164-253) */
    uint16_t vendor_revision;               /*!< Bytes 164-165: Vendor specific revision number */
    uint8_t  vendor_specific[88];           /*!< Bytes 166-253: Vendor specific data */

    /* Integrity (bytes 254-255) */
    uint16_t crc;                           /*!< Bytes 254-255: Integrity CRC-16 */
} nand_parameter_page_t;

#ifdef __cplusplus
}
#endif
