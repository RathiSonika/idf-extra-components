/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Default OOB layout matching openspec configurable_oob_layout_proposal.md §1.2:
 * - BBM: spare offset 0, length 2, good pattern 0xFF 0xFF, first page only.
 * - User “free” spare (RFC): one region at offset 2, length 2 — BBM bytes are not enumerated here.
 *
 * @note oob_bytes == 0 means “fill from chip spare size at init” (step 05), Pattern A.
 */

#include "nand_oob_layout_default.h"

#if CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT

#include "esp_err.h"

static esp_err_t default_free_region(const void *chip_ctx, int section, spi_nand_oob_region_desc_t *out)
{
    (void)chip_ctx;

    if (section == 0) {
        out->offset = 2;
        out->length = 2;
        out->programmable = true;
        out->ecc_protected = true;
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

static const spi_nand_ooblayout_ops_t s_nand_ooblayout_ops_default = {
    .free_region = default_free_region,
    .ecc_region = NULL,
};

static const spi_nand_oob_layout_t s_nand_oob_layout_default = {
    .oob_bytes = 0,
    .bbm =
    {
        .bbm_offset = 0,
        .bbm_length = 2,
        .good_pattern = {0xFF, 0xFF},
        .check_pages_mask = (uint8_t)SPI_NAND_BBM_CHECK_FIRST_PAGE,
    },
    .ops = &s_nand_ooblayout_ops_default,
};

const spi_nand_oob_layout_t *nand_oob_layout_get_default(void)
{
    return &s_nand_oob_layout_default;
}

#endif /* CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT */
