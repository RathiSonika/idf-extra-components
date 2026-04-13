# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

idf_component_register(
    SRCS "nand_volume_uart_dumper.c"
    INCLUDE_DIRS "."
    REQUIRES spi_nand_flash esp_driver_uart esp_driver_spi esp_blockdev esp_stdio
)
