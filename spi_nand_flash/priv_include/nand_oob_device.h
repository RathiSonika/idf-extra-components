/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file nand_oob_device.h
 * @brief Attach default OOB layout to a device handle (private, Kconfig-gated).
 *
 * Layout callbacks receive @p chip_ctx as the owning `spi_nand_flash_device_t *` (same as
 * @ref nand_oob_xfer_ctx_init). Not ISR-safe — call only from task context during init.
 */

#pragma once

#include "sdkconfig.h"

#if CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT

#include "esp_err.h"

struct spi_nand_flash_device_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief After chip geometry is known, attach default layout and init cached metadata.
 *
 * Resolves effective spare size when `layout->oob_bytes == 0` (Linux: `chip.emulated_page_oob`;
 * target: same 512→16 / 2048→64 / 4096→128 table as the mmap emulator).
 *
 * @param[in,out] handle  Device handle (calloc'd); fields written are read-only after success.
 */
esp_err_t nand_oob_attach_default_layout(struct spi_nand_flash_device_t *handle);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT */
