/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file nand_oob_xfer.h
 * @brief Scatter/gather between logical packed OOB streams and a raw per-page OOB buffer (private).
 *
 * Logical offsets are **relative to the concatenation** of `spi_nand_oob_xfer_ctx_t::regs[]` in order:
 * the first byte of logical stream offset 0 lands at `regs[0].offset` in `oob_raw`. Physical offsets in
 * @ref spi_nand_oob_region_desc_t are **spare-area-only** indices (see nand_oob_layout_types.h).
 */

#pragma once

#include "sdkconfig.h"

#if CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT

#include <stddef.h>

#include "esp_err.h"
#include "nand_oob_layout_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize xfer context: enumerate `layout->ops->free_region`, cache regions matching @p cls.
 *
 * @param[out] ctx      Cleared and filled; stack-local storage recommended (proposal §2.2).
 * @param layout        Must outlive @p ctx for the duration of scatter/gather use.
 * @param chip_ctx      Opaque handle passed to layout ops (may be NULL if ops ignore it).
 * @param cls           Logical stream: ECC vs non-ECC free spare (see step 03 default: FREE_ECC).
 * @param oob_raw       Raw OOB buffer for one page; must hold at least @p oob_size bytes.
 * @param oob_size      Size of @p oob_raw (bounds-check physical spans).
 *
 * @note No heap allocation. Validates each cached region fits in @p oob_size.
 */
esp_err_t nand_oob_xfer_ctx_init(spi_nand_oob_xfer_ctx_t *ctx,
                                 const spi_nand_oob_layout_t *layout,
                                 const void *chip_ctx,
                                 spi_nand_oob_class_t cls,
                                 uint8_t *oob_raw,
                                 uint16_t oob_size);

/**
 * @brief Gather: copy from `oob_raw` into @p dst for logical range `[logical_off, logical_off + len)`.
 *
 * @note No NAND I/O.
 */
esp_err_t nand_oob_gather(const spi_nand_oob_xfer_ctx_t *ctx,
                          size_t logical_off,
                          void *dst,
                          size_t len);

/**
 * @brief Scatter: copy from @p src into `oob_raw` for logical range `[logical_off, logical_off + len)`.
 *
 * **Single `program_execute` contract (proposal §2.2):** Scatter only prepares bytes in `oob_raw`. Callers
 * that map regions to NAND column programs must issue **all** `program_load` operations implied by the
 * layout for **one** logical page program before **exactly one** `program_execute` /
 * `program_execute_and_wait`. Multiple executes per logical page program are out of scope and unsafe.
 *
 * @note No NAND I/O.
 */
esp_err_t nand_oob_scatter(spi_nand_oob_xfer_ctx_t *ctx,
                           size_t logical_off,
                           const void *src,
                           size_t len);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT */
