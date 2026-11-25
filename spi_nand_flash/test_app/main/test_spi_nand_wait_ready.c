/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * On-target checks that nand_wait_for_ready() pacing does not round erase/program
 * up to a FreeRTOS tick. Only exercises real BUSY waits (erase / program / read).
 */

#include <inttypes.h>
#include <stdlib.h>
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "unity.h"
#include "sdkconfig.h"
#include "spi_nand_flash.h"
#include "nand_private/nand_impl_wrap.h"
#include "spi_nand_flash_test_helpers.h"
#include "test_spi_nand_common.h"

/* Must stay above FreeRTOS tick at default HZ=100 (10 ms) — regression for old vTaskDelay(1). */
#define NAND_WAIT_NO_TICK_QUANTUM_US  8000

static void setup_nand(spi_nand_flash_device_t **out_handle)
{
    spi_device_handle_t spi;
    spi_nand_test_setup_chip(&spi, SPI_DEVICE_HALFDUPLEX);

    spi_nand_flash_config_t cfg = {
        .device_handle = spi,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .io_mode = SPI_NAND_IO_MODE_SIO,
    };
    TEST_ESP_OK(spi_nand_flash_init_device(&cfg, out_handle));
}

static void teardown_nand(spi_nand_flash_device_t *handle)
{
    spi_nand_flash_deinit_device(handle);
    spi_nand_flash_test_teardown();
}

static uint32_t find_good_block(spi_nand_flash_device_t *dev, uint32_t num_blocks, uint32_t start)
{
    for (uint32_t b = start; b < num_blocks; b++) {
        bool bad = true;
        TEST_ESP_OK(nand_wrap_is_bad(dev, b, &bad));
        if (!bad) {
            return b;
        }
    }
    TEST_FAIL_MESSAGE("no good block found");
    return 0;
}

TEST_CASE("wait_ready: single-block erase latency not quantized to FreeRTOS tick", "[spi_nand_flash][wait_ready]")
{
    spi_nand_flash_device_t *dev;
    setup_nand(&dev);

    uint32_t num_blocks = 0;
    TEST_ESP_OK(spi_nand_flash_get_block_num(dev, &num_blocks));
    uint32_t block = find_good_block(dev, num_blocks, 10);

    int64_t t0 = esp_timer_get_time();
    TEST_ESP_OK(nand_wrap_erase_block(dev, block));
    int64_t elapsed = esp_timer_get_time() - t0;

    printf("erase_block elapsed=%" PRId64 " us\n", elapsed);
    /* Old path clustered near ~10 ms; new path should track real tBERS (typically 1–6 ms). */
    TEST_ASSERT_TRUE(elapsed < NAND_WAIT_NO_TICK_QUANTUM_US);
    TEST_ASSERT_TRUE(elapsed > 100);

    teardown_nand(dev);
}

TEST_CASE("wait_ready: several block erases stay below tick quantum each", "[spi_nand_flash][wait_ready]")
{
    spi_nand_flash_device_t *dev;
    setup_nand(&dev);

    uint32_t num_blocks = 0;
    TEST_ESP_OK(spi_nand_flash_get_block_num(dev, &num_blocks));

    const int n = 8;
    uint32_t next = 10;
    for (int i = 0; i < n; i++) {
        uint32_t block = find_good_block(dev, num_blocks, next);
        int64_t t0 = esp_timer_get_time();
        TEST_ESP_OK(nand_wrap_erase_block(dev, block));
        int64_t elapsed = esp_timer_get_time() - t0;
        printf("erase[%d] block=%" PRIu32 " elapsed=%" PRId64 " us\n", i, block, elapsed);
        TEST_ASSERT_TRUE(elapsed < NAND_WAIT_NO_TICK_QUANTUM_US);
        next = block + 1;
    }

    teardown_nand(dev);
}

TEST_CASE("wait_ready: page program then read use short-path waits successfully", "[spi_nand_flash][wait_ready]")
{
    spi_nand_flash_device_t *dev;
    setup_nand(&dev);

    uint32_t page_size = 0;
    uint32_t block_size = 0;
    uint32_t num_blocks = 0;
    TEST_ESP_OK(spi_nand_flash_get_page_size(dev, &page_size));
    TEST_ESP_OK(spi_nand_flash_get_block_size(dev, &block_size));
    TEST_ESP_OK(spi_nand_flash_get_block_num(dev, &num_blocks));

    uint32_t block = find_good_block(dev, num_blocks, 12);
    uint32_t page = block * (block_size / page_size);

    TEST_ESP_OK(nand_wrap_erase_block(dev, block));

    uint8_t *buf = heap_caps_malloc(page_size, MALLOC_CAP_DMA);
    TEST_ASSERT_NOT_NULL(buf);
    spi_nand_flash_fill_buffer(buf, page_size / sizeof(uint32_t));

    int64_t t_prog0 = esp_timer_get_time();
    TEST_ESP_OK(nand_wrap_prog(dev, page, buf));
    int64_t t_prog = esp_timer_get_time() - t_prog0;

    int64_t t_rd0 = esp_timer_get_time();
    TEST_ESP_OK(nand_wrap_read(dev, page, 0, page_size, buf));
    int64_t t_rd = esp_timer_get_time() - t_rd0;

    printf("prog=%" PRId64 " us read=%" PRId64 " us\n", t_prog, t_rd);
    TEST_ASSERT_TRUE(t_prog < NAND_WAIT_NO_TICK_QUANTUM_US);
    TEST_ASSERT_TRUE(t_rd < 2000);
    TEST_ASSERT_EQUAL(0, spi_nand_flash_check_buffer(buf, page_size / sizeof(uint32_t)));

    free(buf);
    teardown_nand(dev);
}
