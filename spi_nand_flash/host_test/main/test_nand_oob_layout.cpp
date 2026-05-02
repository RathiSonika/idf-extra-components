/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Unit tests for configurable OOB scatter/gather (proposal §2.2 / step 11).
 * Built only when CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT=y.
 */

#include "sdkconfig.h"

#ifdef CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT

#include <cstring>

#include "esp_err.h"
#include "nand_oob_layout_default.h"
#include "nand_oob_xfer.h"

#include <catch2/catch_test_macros.hpp>

namespace {

static esp_err_t layout_region_oob_too_large(const void *chip_ctx, int section, spi_nand_oob_region_desc_t *out)
{
    (void)chip_ctx;
    if (section != 0) {
        return ESP_ERR_NOT_FOUND;
    }
    out->offset = 30;
    out->length = 10;
    out->programmable = true;
    out->ecc_protected = true;
    return ESP_OK;
}

static const spi_nand_ooblayout_ops_t s_ops_oob_too_large = {
    .free_region = layout_region_oob_too_large,
    .ecc_region = nullptr,
};

static const spi_nand_oob_layout_t s_layout_oob_too_large = {
    .oob_bytes = 32,
    .bbm = {.bbm_offset = 0, .bbm_length = 2, .good_pattern = {0xFF, 0xFF}, .check_pages_mask = 1},
    .ops = &s_ops_oob_too_large,
};

static esp_err_t layout_two_regions(const void *chip_ctx, int section, spi_nand_oob_region_desc_t *out)
{
    (void)chip_ctx;
    if (section == 0) {
        out->offset = 4;
        out->length = 1;
        out->programmable = true;
        out->ecc_protected = true;
        return ESP_OK;
    }
    if (section == 1) {
        out->offset = 10;
        out->length = 3;
        out->programmable = true;
        out->ecc_protected = true;
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

static const spi_nand_ooblayout_ops_t s_ops_two_regions = {
    .free_region = layout_two_regions,
    .ecc_region = nullptr,
};

static const spi_nand_oob_layout_t s_layout_two_regions = {
    .oob_bytes = 16,
    .bbm = {.bbm_offset = 0, .bbm_length = 2, .good_pattern = {0xFF, 0xFF}, .check_pages_mask = 1},
    .ops = &s_ops_two_regions,
};

static esp_err_t layout_skip_zero_then_one(const void *chip_ctx, int section, spi_nand_oob_region_desc_t *out)
{
    (void)chip_ctx;
    if (section == 0) {
        out->offset = 0;
        out->length = 0;
        out->programmable = true;
        out->ecc_protected = true;
        return ESP_OK;
    }
    if (section == 1) {
        out->offset = 6;
        out->length = 2;
        out->programmable = true;
        out->ecc_protected = true;
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

static const spi_nand_ooblayout_ops_t s_ops_skip_zero = {
    .free_region = layout_skip_zero_then_one,
    .ecc_region = nullptr,
};

static const spi_nand_oob_layout_t s_layout_skip_zero = {
    .oob_bytes = 16,
    .bbm = {.bbm_offset = 0, .bbm_length = 2, .good_pattern = {0xFF, 0xFF}, .check_pages_mask = 1},
    .ops = &s_ops_skip_zero,
};

static esp_err_t layout_only_no_ecc(const void *chip_ctx, int section, spi_nand_oob_region_desc_t *out)
{
    (void)chip_ctx;
    if (section != 0) {
        return ESP_ERR_NOT_FOUND;
    }
    out->offset = 2;
    out->length = 4;
    out->programmable = true;
    out->ecc_protected = false;
    return ESP_OK;
}

static const spi_nand_ooblayout_ops_t s_ops_only_no_ecc = {
    .free_region = layout_only_no_ecc,
    .ecc_region = nullptr,
};

static const spi_nand_oob_layout_t s_layout_only_no_ecc = {
    .oob_bytes = 16,
    .bbm = {.bbm_offset = 0, .bbm_length = 2, .good_pattern = {0xFF, 0xFF}, .check_pages_mask = 1},
    .ops = &s_ops_only_no_ecc,
};

static esp_err_t layout_too_many_sections(const void *chip_ctx, int section, spi_nand_oob_region_desc_t *out)
{
    (void)chip_ctx;
    if (section < 0 || section >= SPI_NAND_OOB_MAX_REGIONS + 1) {
        return ESP_ERR_NOT_FOUND;
    }
    out->offset = (uint16_t)(section * 2);
    out->length = 1;
    out->programmable = true;
    out->ecc_protected = true;
    return ESP_OK;
}

static const spi_nand_ooblayout_ops_t s_ops_too_many = {
    .free_region = layout_too_many_sections,
    .ecc_region = nullptr,
};

static const spi_nand_oob_layout_t s_layout_too_many = {
    .oob_bytes = 64,
    .bbm = {.bbm_offset = 0, .bbm_length = 2, .good_pattern = {0xFF, 0xFF}, .check_pages_mask = 1},
    .ops = &s_ops_too_many,
};

} // namespace

TEST_CASE("OOB xfer_ctx_init rejects invalid arguments", "[oob_layout]")
{
    spi_nand_oob_xfer_ctx_t ctx;
    uint8_t raw[8];
    const spi_nand_oob_layout_t *def = nand_oob_layout_get_default();
    REQUIRE(def != nullptr);

    REQUIRE(nand_oob_xfer_ctx_init(nullptr, def, nullptr, SPI_NAND_OOB_CLASS_FREE_ECC, raw, 8) == ESP_ERR_INVALID_ARG);
    REQUIRE(nand_oob_xfer_ctx_init(&ctx, nullptr, nullptr, SPI_NAND_OOB_CLASS_FREE_ECC, raw, 8) == ESP_ERR_INVALID_ARG);
    REQUIRE(nand_oob_xfer_ctx_init(&ctx, def, nullptr, SPI_NAND_OOB_CLASS_FREE_ECC, nullptr, 8) == ESP_ERR_INVALID_ARG);
    REQUIRE(nand_oob_xfer_ctx_init(&ctx, def, nullptr, SPI_NAND_OOB_CLASS_FREE_ECC, raw, 0) == ESP_ERR_INVALID_ARG);
}

TEST_CASE("OOB xfer_ctx_init rejects region extending past oob_size", "[oob_layout]")
{
    spi_nand_oob_xfer_ctx_t ctx;
    uint8_t raw[32];
    REQUIRE(nand_oob_xfer_ctx_init(&ctx, &s_layout_oob_too_large, nullptr, SPI_NAND_OOB_CLASS_FREE_ECC, raw, 32) ==
            ESP_ERR_INVALID_SIZE);
}

TEST_CASE("default layout: PAGE_USED gather/scatter round-trip at logical offset 0", "[oob_layout]")
{
    const spi_nand_oob_layout_t *layout = nand_oob_layout_get_default();
    REQUIRE(layout != nullptr);

    uint8_t raw[16];
    memset(raw, 0xA5, sizeof(raw));
    raw[0] = 0xFF;
    raw[1] = 0xFF;
    raw[2] = 0x11;
    raw[3] = 0x22;

    spi_nand_oob_xfer_ctx_t ctx;
    REQUIRE(nand_oob_xfer_ctx_init(&ctx, layout, nullptr, SPI_NAND_OOB_CLASS_FREE_ECC, raw, 16) == ESP_OK);
    REQUIRE(ctx.reg_count == 1);
    REQUIRE(ctx.total_logical_len == 2);

    uint8_t got[2] = {0};
    REQUIRE(nand_oob_gather(&ctx, 0, got, 2) == ESP_OK);
    REQUIRE(got[0] == 0x11);
    REQUIRE(got[1] == 0x22);

    const uint8_t wr[2] = {0xEE, 0xDD};
    REQUIRE(nand_oob_scatter(&ctx, 0, wr, 2) == ESP_OK);
    REQUIRE(raw[0] == 0xFF);
    REQUIRE(raw[1] == 0xFF);
    REQUIRE(raw[2] == 0xEE);
    REQUIRE(raw[3] == 0xDD);
}

TEST_CASE("default layout: partial gather/scatter within PAGE_USED region", "[oob_layout]")
{
    const spi_nand_oob_layout_t *layout = nand_oob_layout_get_default();
    uint8_t raw[16];
    memset(raw, 0, sizeof(raw));
    raw[2] = 0xAA;
    raw[3] = 0xBB;

    spi_nand_oob_xfer_ctx_t ctx;
    REQUIRE(nand_oob_xfer_ctx_init(&ctx, layout, nullptr, SPI_NAND_OOB_CLASS_FREE_ECC, raw, 16) == ESP_OK);

    uint8_t one = 0;
    REQUIRE(nand_oob_gather(&ctx, 1, &one, 1) == ESP_OK);
    REQUIRE(one == 0xBB);

    const uint8_t rep = 0x55;
    REQUIRE(nand_oob_scatter(&ctx, 1, &rep, 1) == ESP_OK);
    REQUIRE(raw[2] == 0xAA);
    REQUIRE(raw[3] == 0x55);
}

TEST_CASE("default layout: scatter length beyond logical stream fails without touching BBM bytes", "[oob_layout]")
{
    const spi_nand_oob_layout_t *layout = nand_oob_layout_get_default();
    uint8_t raw[16];
    memset(raw, 0xFF, sizeof(raw));
    raw[2] = raw[3] = 0x00;

    spi_nand_oob_xfer_ctx_t ctx;
    REQUIRE(nand_oob_xfer_ctx_init(&ctx, layout, nullptr, SPI_NAND_OOB_CLASS_FREE_ECC, raw, 16) == ESP_OK);

    const uint8_t poison[3] = {0x01, 0x02, 0x03};
    REQUIRE(nand_oob_scatter(&ctx, 0, poison, 3) == ESP_ERR_INVALID_SIZE);
    REQUIRE(raw[0] == 0xFF);
    REQUIRE(raw[1] == 0xFF);
    REQUIRE(raw[2] == 0x00);
    REQUIRE(raw[3] == 0x00);
}

TEST_CASE("default layout: gather/scatter out of logical bounds", "[oob_layout]")
{
    const spi_nand_oob_layout_t *layout = nand_oob_layout_get_default();
    uint8_t raw[16] = {0};
    spi_nand_oob_xfer_ctx_t ctx;
    REQUIRE(nand_oob_xfer_ctx_init(&ctx, layout, nullptr, SPI_NAND_OOB_CLASS_FREE_ECC, raw, 16) == ESP_OK);

    uint8_t buf[4];
    REQUIRE(nand_oob_gather(&ctx, 2, buf, 1) == ESP_ERR_INVALID_SIZE);
    REQUIRE(nand_oob_gather(&ctx, 0, buf, 3) == ESP_ERR_INVALID_SIZE);

    REQUIRE(nand_oob_scatter(&ctx, 1, buf, 2) == ESP_ERR_INVALID_SIZE);
}

TEST_CASE("two-region layout: gather/scatter spanning concatenated logical stream", "[oob_layout]")
{
    uint8_t raw[16];
    memset(raw, 0xCC, sizeof(raw));
    raw[4] = 0x10;
    raw[10] = 0x21;
    raw[11] = 0x22;
    raw[12] = 0x23;

    spi_nand_oob_xfer_ctx_t ctx;
    REQUIRE(nand_oob_xfer_ctx_init(&ctx, &s_layout_two_regions, nullptr, SPI_NAND_OOB_CLASS_FREE_ECC, raw, 16) ==
            ESP_OK);
    REQUIRE(ctx.total_logical_len == 4);
    REQUIRE(ctx.reg_count == 2);

    uint8_t got[4];
    REQUIRE(nand_oob_gather(&ctx, 0, got, 4) == ESP_OK);
    REQUIRE(got[0] == 0x10);
    REQUIRE(got[1] == 0x21);
    REQUIRE(got[2] == 0x22);
    REQUIRE(got[3] == 0x23);

    const uint8_t wr[4] = {0xA1, 0xA2, 0xA3, 0xA4};
    REQUIRE(nand_oob_scatter(&ctx, 0, wr, 4) == ESP_OK);
    REQUIRE(raw[4] == 0xA1);
    REQUIRE(raw[10] == 0xA2);
    REQUIRE(raw[11] == 0xA3);
    REQUIRE(raw[12] == 0xA4);
}

TEST_CASE("xfer_ctx_init skips zero-length regions then caches following section", "[oob_layout]")
{
    uint8_t raw[16];
    memset(raw, 0, sizeof(raw));

    spi_nand_oob_xfer_ctx_t ctx;
    REQUIRE(nand_oob_xfer_ctx_init(&ctx, &s_layout_skip_zero, nullptr, SPI_NAND_OOB_CLASS_FREE_ECC, raw, 16) ==
            ESP_OK);
    REQUIRE(ctx.reg_count == 1);
    REQUIRE(ctx.total_logical_len == 2);
    REQUIRE(ctx.regs[0].offset == 6);
}

TEST_CASE("FREE_ECC class filters out non-ecc_protected regions", "[oob_layout]")
{
    uint8_t raw[16];
    spi_nand_oob_xfer_ctx_t ctx;
    REQUIRE(nand_oob_xfer_ctx_init(&ctx, &s_layout_only_no_ecc, nullptr, SPI_NAND_OOB_CLASS_FREE_ECC, raw, 16) ==
            ESP_OK);
    REQUIRE(ctx.reg_count == 0);
    REQUIRE(ctx.total_logical_len == 0);

    uint8_t b = 0;
    REQUIRE(nand_oob_gather(&ctx, 0, &b, 1) == ESP_ERR_INVALID_SIZE);
}

TEST_CASE("FREE_NOECC class selects only non-ecc_protected regions", "[oob_layout]")
{
    uint8_t raw[16];
    memset(raw, 0xBB, sizeof(raw));
    spi_nand_oob_xfer_ctx_t ctx;
    REQUIRE(nand_oob_xfer_ctx_init(&ctx, &s_layout_only_no_ecc, nullptr, SPI_NAND_OOB_CLASS_FREE_NOECC, raw, 16) ==
            ESP_OK);
    REQUIRE(ctx.total_logical_len == 4);

    uint8_t got[4];
    REQUIRE(nand_oob_gather(&ctx, 0, got, 4) == ESP_OK);
    for (int i = 0; i < 4; i++) {
        REQUIRE(got[i] == 0xBB);
    }
}

TEST_CASE("xfer_ctx_init fails when free_region count exceeds SPI_NAND_OOB_MAX_REGIONS", "[oob_layout]")
{
    uint8_t raw[64];
    spi_nand_oob_xfer_ctx_t ctx;
    REQUIRE(nand_oob_xfer_ctx_init(&ctx, &s_layout_too_many, nullptr, SPI_NAND_OOB_CLASS_FREE_ECC, raw, 64) ==
            ESP_ERR_INVALID_SIZE);
}

TEST_CASE("gather and scatter with len==0 succeed without touching buffer", "[oob_layout]")
{
    const spi_nand_oob_layout_t *layout = nand_oob_layout_get_default();
    uint8_t raw[16];
    memset(raw, 0x7E, sizeof(raw));
    spi_nand_oob_xfer_ctx_t ctx;
    REQUIRE(nand_oob_xfer_ctx_init(&ctx, layout, nullptr, SPI_NAND_OOB_CLASS_FREE_ECC, raw, 16) == ESP_OK);

    REQUIRE(nand_oob_gather(&ctx, 0, nullptr, 0) == ESP_OK);
    REQUIRE(nand_oob_scatter(&ctx, 0, nullptr, 0) == ESP_OK);
}

#endif /* CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT */
