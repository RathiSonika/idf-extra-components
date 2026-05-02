/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"

#if CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT

#include "nand_oob_xfer.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "nand_oob_layout_default.h"

static bool region_matches_class(const spi_nand_oob_region_desc_t *desc, spi_nand_oob_class_t cls)
{
    switch (cls) {
    case SPI_NAND_OOB_CLASS_FREE_ECC:
        return desc->ecc_protected;
    case SPI_NAND_OOB_CLASS_FREE_NOECC:
        return !desc->ecc_protected;
    default:
        return false;
    }
}

/**
 * Optional debug invariant: default layout + FREE_ECC → one region {offset 2, length 2}, total logical len 2.
 */
static void xfer_assert_default_layout(const spi_nand_oob_xfer_ctx_t *ctx,
                                       const spi_nand_oob_layout_t *layout,
                                       spi_nand_oob_class_t cls)
{
#ifndef NDEBUG
    if (layout != nand_oob_layout_get_default() || cls != SPI_NAND_OOB_CLASS_FREE_ECC) {
        return;
    }
    assert(ctx->total_logical_len == 2);
    assert(ctx->reg_count == 1);
    assert(ctx->regs[0].offset == 2 && ctx->regs[0].length == 2);
#else
    (void)ctx;
    (void)layout;
    (void)cls;
#endif
}

esp_err_t nand_oob_xfer_ctx_init(spi_nand_oob_xfer_ctx_t *ctx,
                                 const spi_nand_oob_layout_t *layout,
                                 const void *chip_ctx,
                                 spi_nand_oob_class_t cls,
                                 uint8_t *oob_raw,
                                 uint16_t oob_size)
{
    if (ctx == NULL || layout == NULL || layout->ops == NULL || layout->ops->free_region == NULL ||
            oob_raw == NULL || oob_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->layout = layout;
    ctx->cls = cls;
    ctx->oob_raw = oob_raw;
    ctx->oob_size = oob_size;

    for (int section = 0;; section++) {
        spi_nand_oob_region_desc_t desc;
        esp_err_t err = layout->ops->free_region(chip_ctx, section, &desc);
        if (err == ESP_ERR_NOT_FOUND) {
            break;
        }
        if (err != ESP_OK) {
            memset(ctx, 0, sizeof(*ctx));
            return err;
        }
        if (desc.length == 0) {
            continue;
        }
        if (!region_matches_class(&desc, cls)) {
            continue;
        }
        if ((uint32_t)desc.offset + desc.length > oob_size) {
            memset(ctx, 0, sizeof(*ctx));
            return ESP_ERR_INVALID_SIZE;
        }
        if (ctx->reg_count >= SPI_NAND_OOB_MAX_REGIONS) {
            memset(ctx, 0, sizeof(*ctx));
            return ESP_ERR_INVALID_SIZE;
        }
        ctx->regs[ctx->reg_count] = desc;
        ctx->reg_count++;
        ctx->total_logical_len += desc.length;
    }

    xfer_assert_default_layout(ctx, layout, cls);
    return ESP_OK;
}

static esp_err_t xfer_bounds_and_span_check(const spi_nand_oob_xfer_ctx_t *ctx,
        size_t logical_off,
        size_t len,
        const void *buf)
{
    if (ctx == NULL || ctx->oob_raw == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (len == 0) {
        return ESP_OK;
    }
    if (buf == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (logical_off > ctx->total_logical_len || len > ctx->total_logical_len - logical_off) {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

esp_err_t nand_oob_gather(const spi_nand_oob_xfer_ctx_t *ctx,
                          size_t logical_off,
                          void *dst,
                          size_t len)
{
    esp_err_t err = xfer_bounds_and_span_check(ctx, logical_off, len, dst);
    if (err != ESP_OK || len == 0) {
        return err;
    }

    size_t abs_end = logical_off + len;
    size_t stream_base = 0;
    uint8_t *dst_bytes = (uint8_t *)dst;

    for (uint8_t i = 0; i < ctx->reg_count; i++) {
        const spi_nand_oob_region_desc_t *r = &ctx->regs[i];
        size_t reg_lo = stream_base;
        size_t reg_hi = stream_base + r->length;
        stream_base = reg_hi;

        size_t seg_lo = logical_off > reg_lo ? logical_off : reg_lo;
        size_t seg_hi = abs_end < reg_hi ? abs_end : reg_hi;
        if (seg_lo >= seg_hi) {
            continue;
        }

        size_t within_region = seg_lo - reg_lo;
        size_t phys = (size_t)r->offset + within_region;
        size_t buf_off = seg_lo - logical_off;
        size_t seg_len = seg_hi - seg_lo;

        if (phys + seg_len > ctx->oob_size) {
            return ESP_ERR_INVALID_SIZE;
        }

        memcpy(dst_bytes + buf_off, ctx->oob_raw + phys, seg_len);
    }

    return ESP_OK;
}

esp_err_t nand_oob_scatter(spi_nand_oob_xfer_ctx_t *ctx,
                           size_t logical_off,
                           const void *src,
                           size_t len)
{
    esp_err_t err = xfer_bounds_and_span_check(ctx, logical_off, len, src);
    if (err != ESP_OK || len == 0) {
        return err;
    }

    size_t abs_end = logical_off + len;
    size_t stream_base = 0;
    const uint8_t *src_bytes = (const uint8_t *)src;

    for (uint8_t i = 0; i < ctx->reg_count; i++) {
        const spi_nand_oob_region_desc_t *r = &ctx->regs[i];
        size_t reg_lo = stream_base;
        size_t reg_hi = stream_base + r->length;
        stream_base = reg_hi;

        size_t seg_lo = logical_off > reg_lo ? logical_off : reg_lo;
        size_t seg_hi = abs_end < reg_hi ? abs_end : reg_hi;
        if (seg_lo >= seg_hi) {
            continue;
        }

        size_t within_region = seg_lo - reg_lo;
        size_t phys = (size_t)r->offset + within_region;
        size_t buf_off = seg_lo - logical_off;
        size_t seg_len = seg_hi - seg_lo;

        if (phys + seg_len > ctx->oob_size) {
            return ESP_ERR_INVALID_SIZE;
        }

        memcpy(ctx->oob_raw + phys, src_bytes + buf_off, seg_len);
    }

    return ESP_OK;
}

#endif /* CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT */
