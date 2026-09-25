/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"
#include <inttypes.h>
#include <string.h>
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "nand.h"
#include "nand_onfi.h"
#include "nand_generic_geometry.h"
#include "nand_generic_detect.h"
#include "nand_impl.h"
#include "spi_nand_oper.h"

static const char *TAG = "nand_generic";

/* Common CFG bit for on-die ECC enable across many SPI NAND vendors. */
#define REG_CONFIG_ECC_EN   (1 << 4)

static void nand_generic_apply_io_limits(spi_nand_flash_device_t *dev)
{
    if (dev->config.io_mode == SPI_NAND_IO_MODE_QOUT
            || dev->config.io_mode == SPI_NAND_IO_MODE_QIO
            || dev->config.io_mode == SPI_NAND_IO_MODE_DOUT
            || dev->config.io_mode == SPI_NAND_IO_MODE_DIO) {
        ESP_LOGW(TAG, "Generic chip uses SIO only; forcing SPI_NAND_IO_MODE_SIO");
    }
    dev->config.io_mode = SPI_NAND_IO_MODE_SIO;
}

static void nand_generic_apply_profile(spi_nand_flash_device_t *dev)
{
    dev->chip.num_planes = CONFIG_NAND_FLASH_GENERIC_NUM_PLANES;
    dev->chip.flags &= ~(NAND_FLAG_HAS_PROG_PLANE_SELECT | NAND_FLAG_HAS_READ_PLANE_SELECT
                         | NAND_FLAG_IDM_SAME_PARITY_REQUIRED);
    /* Multi-plane SPI NAND uses the block%num_planes column bit; enable both
     * select flags together (no separate Kconfig). Wrong num_planes corrupts
     * addressing — verify against the datasheet. */
    if (dev->chip.num_planes > 1) {
        dev->chip.flags |= NAND_FLAG_HAS_PROG_PLANE_SELECT | NAND_FLAG_HAS_READ_PLANE_SELECT;
    }
    dev->chip.has_quad_enable_bit = 0;
    dev->chip.quad_enable_bit_pos = 0;
    /* Do not decode STATUS ECC bits on this path (vendor encodings differ). */
    dev->chip.ecc_data.ecc_status_reg_len_in_bits = 0;
    dev->chip.ecc_data.ecc_corrected_bits_status = NAND_ECC_UNKNOWN;
    nand_generic_apply_io_limits(dev);
}

/** Read CFG ECC-EN (GET FEATURE only). Does not SET FEATURE. */
static bool nand_generic_read_on_die_ecc_enabled(spi_nand_flash_device_t *dev)
{
    uint8_t cfg = 0;
    esp_err_t ret = spi_nand_read_register(dev, REG_CONFIG, &cfg);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read CFG for on-die ECC status: %s", esp_err_to_name(ret));
        return false;
    }
    return (cfg & REG_CONFIG_ECC_EN) != 0;
}

#if CONFIG_NAND_FLASH_GENERIC_WRITE_ERASE_ENABLE
static esp_err_t nand_generic_enable_on_die_ecc(spi_nand_flash_device_t *dev)
{
    uint8_t cfg = 0;
    esp_err_t ret = spi_nand_read_register(dev, REG_CONFIG, &cfg);
    if (ret != ESP_OK) {
        return ret;
    }
    if (cfg & REG_CONFIG_ECC_EN) {
        return ESP_OK;
    }
    cfg |= REG_CONFIG_ECC_EN;
    return spi_nand_write_register(dev, REG_CONFIG, cfg);
}
#endif

static void nand_generic_fill_report_geometry(spi_nand_flash_device_t *dev, bool otp_valid)
{
    spi_nand_generic_probe_report_t *r = &dev->generic_report;
    memset(r, 0, sizeof(*r));
    r->manufacturer_id = dev->device_info.manufacturer_id;
    r->device_id = dev->device_info.device_id;
    r->otp_valid = otp_valid;
    r->page_size = 1U << dev->chip.log2_page_size;
    r->pages_per_block = 1U << dev->chip.log2_ppb;
    r->num_blocks = dev->chip.num_blocks;
#if CONFIG_NAND_FLASH_GENERIC_WRITE_ERASE_ENABLE
    r->write_erase_enabled = true;
#else
    r->write_erase_enabled = false;
#endif
    r->on_die_ecc_enabled = nand_generic_read_on_die_ecc_enabled(dev);
    strncpy(r->chip_name, dev->device_info.chip_name, sizeof(r->chip_name) - 1);
}

esp_err_t nand_generic_detect_init(spi_nand_flash_device_t *dev)
{
    ESP_LOGW(TAG, "Generic chip detection active: vendor database bypassed (including known JEDEC IDs)");

    esp_err_t ret = nand_onfi_try_init(dev);
    bool otp_valid = (ret == ESP_OK);
    if (!otp_valid) {
        ESP_LOGW(TAG, "ONFI/OTP parameter page unavailable (%s); trying Kconfig geometry",
                 esp_err_to_name(ret));
        ret = nand_generic_geometry_try_init(dev);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    nand_generic_apply_profile(dev);

#if CONFIG_NAND_FLASH_GENERIC_WRITE_ERASE_ENABLE
    /* Write/erase path: SET FEATURE to enable on-die ECC before program/erase. */
    ret = nand_generic_enable_on_die_ecc(dev);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set on-die ECC enable bit: %s", esp_err_to_name(ret));
        /* Continue: some parts power up with ECC already on. */
    }
#endif

    nand_generic_fill_report_geometry(dev, otp_valid);
    return ESP_OK;
}

esp_err_t nand_generic_detect_probe(spi_nand_flash_device_t *dev)
{
    uint8_t status = 0;
    esp_err_t ret = spi_nand_read_register(dev, REG_STATUS, &status);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GET FEATURE (status) probe failed: %s", esp_err_to_name(ret));
        dev->generic_report.page0_read_ok = false;
        return ESP_ERR_NOT_SUPPORTED;
    }

    /* Non-destructive: page 0 / plane 0 smoke read only. */
    uint8_t *probe_buf = heap_caps_malloc(dev->chip.page_size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(probe_buf != NULL, ESP_ERR_NO_MEM, TAG, "probe buffer alloc failed");

    ret = nand_read(dev, 0, 0, dev->chip.page_size, probe_buf);
    free(probe_buf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Page 0 read probe failed: %s", esp_err_to_name(ret));
        dev->generic_report.page0_read_ok = false;
        return ESP_ERR_NOT_SUPPORTED;
    }

    dev->generic_report.page0_read_ok = true;
    ESP_LOGI(TAG,
             "Generic probe OK: JEDEC MI=0x%02x DI=0x%04x otp=%d page=%" PRIu32
             " ppb=%" PRIu32 " blocks=%" PRIu32 " write_erase=%d on_die_ecc=%d",
             (unsigned)dev->generic_report.manufacturer_id,
             (unsigned)dev->generic_report.device_id,
             (int)dev->generic_report.otp_valid,
             dev->generic_report.page_size,
             dev->generic_report.pages_per_block,
             dev->generic_report.num_blocks,
             (int)dev->generic_report.write_erase_enabled,
             (int)dev->generic_report.on_die_ecc_enabled);
    return ESP_OK;
}
