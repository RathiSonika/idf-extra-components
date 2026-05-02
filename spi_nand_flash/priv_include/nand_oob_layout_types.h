/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file nand_oob_layout_types.h
 * @brief Private types for configurable SPI NAND OOB layout (see openspec/configurable_oob_layout_proposal.md).
 *
 * This header is internal to the spi_nand_flash component. Nothing here is part of the stable public API.
 *
 * Offset convention: all `offset` fields in spi_nand_oob_region_desc_t and BBM descriptors are **byte
 * offsets within the OOB (spare) area only**, starting at 0 for the first spare byte. When issuing SPI
 * NAND column reads/programs, the implementation composes the device column address as
 * `page_size + offset` (plus any plane bit folding), not `offset` alone — layout ops never expose main-array
 * addresses.
 *
 * Strategy: types are always defined (whether CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT is enabled or not)
 * so later steps can share one shape without ifdef churn. No static definitions are introduced here.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Max cached physical regions in spi_nand_oob_xfer_ctx_t::regs (stack-sized). TODO(step 12): validate against real datasheets / worst-case interleaved layouts. */
#define SPI_NAND_OOB_MAX_REGIONS 8

/**
 * @brief Which physical page(s) of a block consult the BBM pattern (ONFI-style factory/marked bad checks).
 *
 * The legacy driver reads BBM on the **first** page of the block only; that corresponds to
 * @ref SPI_NAND_BBM_CHECK_FIRST_PAGE in the default layout.
 */
typedef enum {
    SPI_NAND_BBM_CHECK_FIRST_PAGE = 1 << 0,
    SPI_NAND_BBM_CHECK_LAST_PAGE = 1 << 1,
} spi_nand_bbm_check_pages_t;

/**
 * @brief One contiguous region within the per-page OOB area.
 *
 * @note offset is relative to the start of OOB only (not main array).
 */
typedef struct {
    uint16_t offset;
    uint16_t length;
    bool programmable;
    /** True if writes to this region are covered by the device's internal ECC policy for metadata (RFC / proposal). */
    bool ecc_protected;
} spi_nand_oob_region_desc_t;

/**
 * @brief Callback table describing a chip's physical OOB layout.
 *
 * Enumeration: callers probe @p section starting at 0; implementations return one region per successful call.
 * When no region exists for that index, return ESP_ERR_NOT_FOUND (do not use Linux-style -ERANGE here).
 */
typedef struct spi_nand_ooblayout_ops {
    /**
     * User-writable “free” spare regions (excluding BBM and non-programmable vendor/ECC parity unless policy says otherwise).
     */
    esp_err_t (*free_region)(const void *chip_ctx, int section, spi_nand_oob_region_desc_t *out);
    /**
     * Optional: enumerate ECC parity / reserved non-programmable regions for tooling; may be NULL until used.
     */
    esp_err_t (*ecc_region)(const void *chip_ctx, int section, spi_nand_oob_region_desc_t *out);
} spi_nand_ooblayout_ops_t;

/**
 * @brief Logical stream class for packing named fields (driver metadata).
 */
typedef enum {
    SPI_NAND_OOB_CLASS_FREE_ECC,
    SPI_NAND_OOB_CLASS_FREE_NOECC,
} spi_nand_oob_class_t;

/**
 * @brief Named metadata slots carved from packed logical OOB (policy layer).
 */
typedef enum {
    SPI_NAND_OOB_FIELD_PAGE_USED = 0,
    SPI_NAND_OOB_FIELD_MAX,
} spi_nand_oob_field_id_t;

/**
 * @brief Per-field placement after init resolves layout → logical stream.
 */
typedef struct {
    spi_nand_oob_field_id_t id;
    uint8_t length;
    /** Logical stream (ECC vs non-ECC free spare); proposal/RFC name `class`. */
    spi_nand_oob_class_t oob_class;
    /** Byte offset within the packed logical stream for @ref oob_class; assigned during device/layout init. */
    uint16_t logical_offset;
    bool assigned;
} spi_nand_oob_field_spec_t;

/**
 * @brief Static OOB layout descriptor for a chip (hardware-facing).
 *
 * @note oob_bytes duplicates spare size from geometry in many tables; keep in sync with chip page spare size.
 */
typedef struct spi_nand_oob_layout {
    uint8_t oob_bytes;
    struct {
        uint16_t bbm_offset;
        uint16_t bbm_length;
        /** Good-block pattern (e.g. 0xFF, 0xFF for legacy 2-byte BBM). */
        uint8_t good_pattern[2];
        /** Bitmask of @ref spi_nand_bbm_check_pages_t values. */
        uint8_t check_pages_mask;
    } bbm;
    const spi_nand_ooblayout_ops_t *ops;
} spi_nand_oob_layout_t;

/**
 * @brief Cached state for one OOB read/modify/program transfer (implementation in step 04).
 *
 * Holds filtered physical regions for the selected logical class and bounds metadata for scatter/gather.
 */
typedef struct spi_nand_oob_xfer_ctx {
    const spi_nand_oob_layout_t *layout;
    spi_nand_oob_class_t cls;
    uint8_t *oob_raw;
    uint16_t oob_size;
    spi_nand_oob_region_desc_t regs[SPI_NAND_OOB_MAX_REGIONS];
    uint8_t reg_count;
    /** Sum of regs[i].length for i in [0, reg_count); used for bounds checks. */
    size_t total_logical_len;
} spi_nand_oob_xfer_ctx_t;

#ifdef __cplusplus
}
#endif
