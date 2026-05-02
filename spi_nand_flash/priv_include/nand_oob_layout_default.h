/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file nand_oob_layout_default.h
 * @brief Default §1.2-equivalent OOB layout (private). Linked only when CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT=y.
 */

#pragma once

#include "sdkconfig.h"

#if CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT

#include "nand_oob_layout_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Global default layout: BBM at spare bytes 0–1, single FREE_ECC region at 2–2 (page-used marker).
 *
 * @note `oob_bytes` is 0 (“derive from chip geometry at init”, step 05).
 */
const spi_nand_oob_layout_t *nand_oob_layout_get_default(void);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT */
