/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Logical wear-leveled volume dump over UART (see README.md).
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "driver/spi_master.h"
#include "driver/uart.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "soc/spi_pins.h"

#include "esp_blockdev.h"
#include "esp_stdio.h"
#include "spi_nand_flash.h"

static const char *TAG = "nand_vol_dump";

/** ASCII line on dump UART so the host can confirm port/baud before sending DMP. */
static esp_err_t uart_write_line(uart_port_t port, const char *line)
{
    const int len = (int)strlen(line);
    int w = uart_write_bytes(port, line, len);
    if (w != len) {
        return ESP_FAIL;
    }
    return uart_wait_tx_done(port, pdMS_TO_TICKS(5000));
}

/* SPI NAND wiring (same defaults as spi_nand_flash test_app) */
#ifdef CONFIG_IDF_TARGET_ESP32
#define HOST_ID      SPI3_HOST
#define PIN_MOSI     SPI3_IOMUX_PIN_NUM_MOSI
#define PIN_MISO     SPI3_IOMUX_PIN_NUM_MISO
#define PIN_CLK      SPI3_IOMUX_PIN_NUM_CLK
#define PIN_CS       SPI3_IOMUX_PIN_NUM_CS
#define PIN_WP       SPI3_IOMUX_PIN_NUM_WP
#define PIN_HD       SPI3_IOMUX_PIN_NUM_HD
#define SPI_DMA_CHAN SPI_DMA_CH_AUTO
#else
#define HOST_ID      SPI2_HOST
#define PIN_MOSI     SPI2_IOMUX_PIN_NUM_MOSI
#define PIN_MISO     SPI2_IOMUX_PIN_NUM_MISO
#define PIN_CLK      SPI2_IOMUX_PIN_NUM_CLK
#define PIN_CS       SPI2_IOMUX_PIN_NUM_CS
#define PIN_WP       SPI2_IOMUX_PIN_NUM_WP
#define PIN_HD       SPI2_IOMUX_PIN_NUM_HD
#define SPI_DMA_CHAN SPI_DMA_CH_AUTO
#endif

#define DUMP_HANDSHAKE "DMP\n"
#define DUMP_MAGIC_RAW "NDLV"
#if CONFIG_EXAMPLE_DUMP_SPARSE_FF
#define DUMP_MAGIC_SPA "NDS1"
#define REC_TAG_DATA "DATA"
#define REC_TAG_SKIP "SKIP"
#endif

static esp_err_t setup_spi_nand(spi_device_handle_t *spi_out, esp_blockdev_handle_t *wl_bdl_out)
{
    spi_bus_config_t spi_bus_cfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_CLK,
        .quadhd_io_num = PIN_HD,
        .quadwp_io_num = PIN_WP,
        .max_transfer_sz = 8192,
    };
    esp_err_t ret = spi_bus_initialize(HOST_ID, &spi_bus_cfg, SPI_DMA_CHAN);
    ESP_RETURN_ON_ERROR(ret, TAG, "spi_bus_initialize");

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 40 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 4,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };
    ret = spi_bus_add_device(HOST_ID, &devcfg, spi_out);
    if (ret != ESP_OK) {
        spi_bus_free(HOST_ID);
        return ret;
    }

    spi_nand_flash_config_t nand_cfg = {
        .device_handle = *spi_out,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .io_mode = SPI_NAND_IO_MODE_SIO,
        .gc_factor = 45,
    };

    esp_blockdev_handle_t wl_bdl = NULL;
    ret = spi_nand_flash_init_with_layers(&nand_cfg, &wl_bdl);
    if (ret != ESP_OK) {
        spi_bus_remove_device(*spi_out);
        spi_bus_free(HOST_ID);
        *spi_out = NULL;
        return ret;
    }

    *wl_bdl_out = wl_bdl;
    return ESP_OK;
}

static void teardown_spi_nand(spi_device_handle_t spi, esp_blockdev_handle_t wl_bdl)
{
    if (wl_bdl) {
        wl_bdl->ops->release(wl_bdl);
    }
    if (spi) {
        spi_bus_remove_device(spi);
        spi_bus_free(HOST_ID);
    }
}

static esp_err_t setup_dump_uart(uart_port_t port)
{
    uart_config_t cfg = {
        .baud_rate = CONFIG_EXAMPLE_DUMP_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(uart_param_config(port, &cfg), TAG, "uart_param_config");
    ESP_RETURN_ON_ERROR(uart_set_pin(port, CONFIG_EXAMPLE_DUMP_UART_TX_PIN, CONFIG_EXAMPLE_DUMP_UART_RX_PIN,
                                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE),
                        TAG, "uart_set_pin");
    const int rx_buf = 512;
    /* Large TX ring so the 16-byte header is not overrun when page-sized writes queue immediately after. */
    const int tx_buf = 32 * 1024;
    ESP_RETURN_ON_ERROR(uart_driver_install(port, rx_buf * 2, tx_buf, 0, NULL, 0), TAG, "uart_driver_install");
    uart_flush_input(port);
    uart_flush(port);
    return ESP_OK;
}

static esp_err_t wait_dump_handshake(uart_port_t port)
{
    const char *expect = DUMP_HANDSHAKE;
    size_t n = strlen(expect);
    size_t got = 0;
    uint8_t ch;

    ESP_LOGI(TAG, "Send handshake on dump UART: %s (no quotes)", DUMP_HANDSHAKE);

    while (got < n) {
        int r = uart_read_bytes(port, &ch, 1, portMAX_DELAY);
        if (r != 1) {
            return ESP_FAIL;
        }
        if (ch == (uint8_t)expect[got]) {
            got++;
        } else {
            got = (ch == (uint8_t)expect[0]) ? 1 : 0;
        }
    }
    return ESP_OK;
}

static esp_err_t write_dump_header(uart_port_t port, uint64_t disk_size, uint32_t page_size, bool sparse_ff)
{
#if CONFIG_EXAMPLE_DUMP_SPARSE_FF
    if (sparse_ff) {
        uint8_t hdr[20];
        const uint32_t ver = 1;
        memcpy(hdr, DUMP_MAGIC_SPA, 4);
        memcpy(hdr + 4, &ver, sizeof(ver));
        memcpy(hdr + 8, &disk_size, sizeof(disk_size));
        memcpy(hdr + 16, &page_size, sizeof(page_size));
        int w = uart_write_bytes(port, (const char *)hdr, sizeof(hdr));
        if (w != sizeof(hdr)) {
            return ESP_FAIL;
        }
        return uart_wait_tx_done(port, pdMS_TO_TICKS(10000));
    }
#else
    (void)sparse_ff;
#endif
    uint8_t hdr[16];
    memcpy(hdr, DUMP_MAGIC_RAW, 4);
    memcpy(hdr + 4, &disk_size, sizeof(disk_size));
    memcpy(hdr + 12, &page_size, sizeof(page_size));
    int w = uart_write_bytes(port, (const char *)hdr, sizeof(hdr));
    if (w != sizeof(hdr)) {
        return ESP_FAIL;
    }
    return uart_wait_tx_done(port, pdMS_TO_TICKS(10000));
}

#if CONFIG_EXAMPLE_DUMP_SPARSE_FF
static bool buf_all_ff(const uint8_t *p, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (p[i] != 0xff) {
            return false;
        }
    }
    return true;
}

static esp_err_t uart_write_record_skip(uart_port_t port, uint64_t off, uint32_t len)
{
    uint8_t rec[16];
    memcpy(rec, REC_TAG_SKIP, 4);
    memcpy(rec + 4, &off, sizeof(off));
    memcpy(rec + 12, &len, sizeof(len));
    int w = uart_write_bytes(port, (const char *)rec, sizeof(rec));
    return (w == sizeof(rec)) ? ESP_OK : ESP_FAIL;
}

static esp_err_t uart_write_record_data(uart_port_t port, uint64_t off, uint32_t len, const uint8_t *data)
{
    uint8_t head[16];
    memcpy(head, REC_TAG_DATA, 4);
    memcpy(head + 4, &off, sizeof(off));
    memcpy(head + 12, &len, sizeof(len));
    int w = uart_write_bytes(port, (const char *)head, sizeof(head));
    if (w != sizeof(head)) {
        return ESP_FAIL;
    }
    w = uart_write_bytes(port, (const char *)data, len);
    return (w == (int)len) ? ESP_OK : ESP_FAIL;
}

/** Emit coalesced SKIP/DATA runs for [buf, buf+chunk_len) in steps of read_size. */
static esp_err_t emit_sparse_runs(uart_port_t port, uint64_t base_off, const uint8_t *buf, size_t chunk_len, size_t read_size)
{
    size_t i = 0;
    while (i < chunk_len) {
        bool is_ff = buf_all_ff(buf + i, read_size);
        size_t j = i + read_size;
        while (j < chunk_len && buf_all_ff(buf + j, read_size) == is_ff) {
            j += read_size;
        }
        const size_t run_len = j - i;
        const uint64_t run_off = base_off + (uint64_t)i;
        esp_err_t wr;
        if (is_ff) {
            wr = uart_write_record_skip(port, run_off, (uint32_t)run_len);
        } else {
            wr = uart_write_record_data(port, run_off, (uint32_t)run_len, buf + i);
        }
        if (wr != ESP_OK) {
            return wr;
        }
        i = j;
    }
    return ESP_OK;
}
#endif /* CONFIG_EXAMPLE_DUMP_SPARSE_FF */

static esp_err_t dump_volume(uart_port_t port, esp_blockdev_handle_t wl_bdl)
{
    uint64_t disk_size = wl_bdl->geometry.disk_size;
    size_t read_size = wl_bdl->geometry.read_size;

    if (read_size == 0 || (disk_size % read_size) != 0) {
        ESP_LOGE(TAG, "Invalid geometry: disk_size=%" PRIu64 " read_size=%zu", disk_size, read_size);
        return ESP_ERR_INVALID_STATE;
    }

    const size_t max_buf = 256 * 1024;
    size_t chunk_pages = (size_t)CONFIG_EXAMPLE_DUMP_READ_CHUNK_PAGES;
    if (chunk_pages < 1) {
        chunk_pages = 1;
    }
    size_t chunk_len = chunk_pages * read_size;
    if (chunk_len > max_buf) {
        chunk_pages = max_buf / read_size;
        chunk_len = chunk_pages * read_size;
    }

    uint8_t *buf = heap_caps_malloc(chunk_len, MALLOC_CAP_DMA);
    if (buf == NULL) {
        buf = malloc(chunk_len);
    }
    ESP_RETURN_ON_FALSE(buf != NULL, ESP_ERR_NO_MEM, TAG, "alloc read buffer");

    ESP_LOGI(TAG, "Dumping %" PRIu64 " bytes (chunk %zu bytes)...", disk_size, chunk_len);

    for (uint64_t off = 0; off < disk_size; off += chunk_len) {
        esp_task_wdt_reset();

        const uint64_t remain = disk_size - off;
        const size_t this_len = (remain < (uint64_t)chunk_len) ? (size_t)remain : chunk_len;
        esp_err_t ret = wl_bdl->ops->read(wl_bdl, buf, this_len, off, this_len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "read failed at offset %" PRIu64 ": %s", off, esp_err_to_name(ret));
            free(buf);
            return ret;
        }

        esp_err_t wr;
#if CONFIG_EXAMPLE_DUMP_SPARSE_FF
        wr = emit_sparse_runs(port, off, buf, this_len, read_size);
#else
        int w = uart_write_bytes(port, (const char *)buf, this_len);
        wr = (w == (int)this_len) ? ESP_OK : ESP_FAIL;
#endif
        if (wr != ESP_OK) {
            ESP_LOGE(TAG, "UART write failed at offset %" PRIu64, off);
            free(buf);
            return wr;
        }
    }

    free(buf);
    return ESP_OK;
}

void app_main(void)
{
    uart_port_t dump_port = (uart_port_t)CONFIG_EXAMPLE_DUMP_UART_PORT_NUM;

    ESP_LOGI(TAG, "NAND logical volume UART dumper");
    ESP_LOGI(TAG, "Dump UART: port=%d baud=%d TX=%d RX=%d",
             (int)dump_port, CONFIG_EXAMPLE_DUMP_UART_BAUD,
             CONFIG_EXAMPLE_DUMP_UART_TX_PIN, CONFIG_EXAMPLE_DUMP_UART_RX_PIN);

#if CONFIG_ESP_CONSOLE_UART
    if (dump_port == (uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM) {
        /* Same UART as stdio console (often UART0 = flash/monitor cable): release driver so we can re-install. */
        fflush(stdout);
        esp_stdio_uninstall_io_driver();
        esp_log_level_set("*", ESP_LOG_NONE);
    }
#endif

    ESP_ERROR_CHECK(setup_dump_uart(dump_port));
    /* Sent before NAND init so the host can confirm TX/RX/baud within seconds. */
    ESP_ERROR_CHECK(uart_write_line(dump_port, "WAIT\n"));

    spi_device_handle_t spi = NULL;
    esp_blockdev_handle_t wl_bdl = NULL;
    esp_err_t err = setup_spi_nand(&spi, &wl_bdl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NAND init failed: %s", esp_err_to_name(err));
        if (spi != NULL) {
            spi_bus_remove_device(spi);
            spi_bus_free(HOST_ID);
        }
        (void)uart_write_line(dump_port, "FAIL nand_init\n");
        uart_driver_delete(dump_port);
        return;
    }

    uint64_t disk_size = wl_bdl->geometry.disk_size;
    uint32_t page_size = (uint32_t)wl_bdl->geometry.read_size;
    ESP_LOGI(TAG, "Wear-leveled volume disk_size=%" PRIu64 " read_size=%" PRIu32, disk_size, page_size);

    ESP_ERROR_CHECK(uart_write_line(dump_port, "READY\n"));
    ESP_ERROR_CHECK(wait_dump_handshake(dump_port));
    /* Drop any stray RX after handshake (noise / line glitches). */
    uint8_t drain[64];
    while (uart_read_bytes(dump_port, drain, sizeof(drain), 0) > 0) {
    }

#if CONFIG_EXAMPLE_DUMP_SPARSE_FF
    const bool sparse = true;
#else
    const bool sparse = false;
#endif
    ESP_ERROR_CHECK(write_dump_header(dump_port, disk_size, page_size, sparse));

    err = dump_volume(dump_port, wl_bdl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Dump failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Dump complete.");
    }

    teardown_spi_nand(spi, wl_bdl);
    uart_driver_delete(dump_port);
}
