/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <inttypes.h>
#include "esp_check.h"
#include "esp_log.h"
#include "esp_blockdev.h"
#include "esp_nand_blockdev.h"
#include "esp_blockdev/generic_partition.h"
#include "esp_nand_partition.h"

static const char *TAG = "nand_partition";

typedef struct {
    char name[SPI_NAND_PARTITION_NAME_MAX_LEN];
    size_t offset;
    size_t size;
    esp_blockdev_handle_t handle;
} nand_partition_entry_t;

typedef struct {
    esp_blockdev_handle_t wl_bdl;
    nand_partition_entry_t entries[SPI_NAND_MAX_PARTITIONS];
    size_t num_partitions;
} nand_partition_table_t;

static nand_partition_table_t s_tables[SPI_NAND_MAX_INSTANCES];

static nand_partition_table_t *find_table(esp_blockdev_handle_t wl_bdl)
{
    for (int i = 0; i < SPI_NAND_MAX_INSTANCES; i++) {
        if (s_tables[i].wl_bdl == wl_bdl) {
            return &s_tables[i];
        }
    }
    return NULL;
}

static nand_partition_table_t *alloc_table(void)
{
    for (int i = 0; i < SPI_NAND_MAX_INSTANCES; i++) {
        if (s_tables[i].wl_bdl == NULL) {
            return &s_tables[i];
        }
    }
    return NULL;
}

esp_err_t spi_nand_flash_register_partitions(esp_blockdev_handle_t wl_bdl,
        const spi_nand_partition_config_t *partitions,
        size_t num_partitions)
{
    ESP_RETURN_ON_FALSE(wl_bdl && partitions, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    ESP_RETURN_ON_FALSE(num_partitions > 0 && num_partitions <= SPI_NAND_MAX_PARTITIONS,
                        ESP_ERR_INVALID_ARG, TAG, "Invalid number of partitions (1-%d)",
                        SPI_NAND_MAX_PARTITIONS);
    ESP_RETURN_ON_FALSE(find_table(wl_bdl) == NULL, ESP_ERR_INVALID_STATE, TAG,
                        "Partitions already registered for this device");

    nand_partition_table_t *table = alloc_table();
    ESP_RETURN_ON_FALSE(table != NULL, ESP_ERR_NO_MEM, TAG, "No free partition table slots");

    uint64_t disk_size = wl_bdl->geometry.disk_size;
    size_t read_size = wl_bdl->geometry.read_size;
    size_t erase_size = wl_bdl->geometry.erase_size;

    for (size_t i = 0; i < num_partitions; i++) {
        ESP_RETURN_ON_FALSE(partitions[i].name != NULL, ESP_ERR_INVALID_ARG, TAG,
                            "Partition %d: name is NULL", (int)i);
        ESP_RETURN_ON_FALSE(strlen(partitions[i].name) < SPI_NAND_PARTITION_NAME_MAX_LEN,
                            ESP_ERR_INVALID_ARG, TAG, "Partition %d: name too long", (int)i);
        ESP_RETURN_ON_FALSE(partitions[i].size > 0, ESP_ERR_INVALID_ARG, TAG,
                            "Partition '%s': size must be > 0", partitions[i].name);

        if (read_size > 0) {
            ESP_RETURN_ON_FALSE((partitions[i].offset % read_size) == 0, ESP_ERR_INVALID_ARG, TAG,
                                "Partition '%s': offset %zu not aligned to read_size %zu",
                                partitions[i].name, partitions[i].offset, read_size);
            ESP_RETURN_ON_FALSE((partitions[i].size % read_size) == 0, ESP_ERR_INVALID_ARG, TAG,
                                "Partition '%s': size %zu not aligned to read_size %zu",
                                partitions[i].name, partitions[i].size, read_size);
        }

        if (erase_size > 0 && read_size > 0 && erase_size > read_size) {
            if ((partitions[i].offset % erase_size) != 0) {
                ESP_LOGW(TAG, "Partition '%s': offset %zu not aligned to erase_size %zu",
                         partitions[i].name, partitions[i].offset, erase_size);
            }
            if ((partitions[i].size % erase_size) != 0) {
                ESP_LOGW(TAG, "Partition '%s': size %zu not aligned to erase_size %zu",
                         partitions[i].name, partitions[i].size, erase_size);
            }
        }

        uint64_t end = (uint64_t)partitions[i].offset + (uint64_t)partitions[i].size;
        ESP_RETURN_ON_FALSE(end <= disk_size, ESP_ERR_INVALID_ARG, TAG,
                            "Partition '%s' exceeds disk (end=%" PRIu64 ", disk=%" PRIu64 ")",
                            partitions[i].name, end, disk_size);

        for (size_t j = 0; j < i; j++) {
            size_t i_start = partitions[i].offset;
            size_t i_end = i_start + partitions[i].size;
            size_t j_start = partitions[j].offset;
            size_t j_end = j_start + partitions[j].size;
            ESP_RETURN_ON_FALSE(i_end <= j_start || i_start >= j_end, ESP_ERR_INVALID_ARG, TAG,
                                "Partitions '%s' and '%s' overlap", partitions[i].name, partitions[j].name);
        }

        for (size_t j = 0; j < i; j++) {
            ESP_RETURN_ON_FALSE(strcmp(partitions[i].name, partitions[j].name) != 0,
                                ESP_ERR_INVALID_ARG, TAG, "Duplicate partition name '%s'",
                                partitions[i].name);
        }
    }

    table->wl_bdl = wl_bdl;
    table->num_partitions = num_partitions;
    esp_err_t ret = ESP_OK;

    for (size_t i = 0; i < num_partitions; i++) {
        snprintf(table->entries[i].name, SPI_NAND_PARTITION_NAME_MAX_LEN, "%s", partitions[i].name);

        ret = esp_blockdev_generic_partition_get(wl_bdl, partitions[i].offset, partitions[i].size,
                &table->entries[i].handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create partition '%s': %s", partitions[i].name, esp_err_to_name(ret));
            goto cleanup;
        }
        table->entries[i].offset = partitions[i].offset;
        table->entries[i].size = partitions[i].size;

        ESP_LOGI(TAG, "Partition '%s': offset=%zu, size=%zu bytes (%zu sectors)",
                 partitions[i].name, partitions[i].offset, partitions[i].size,
                 read_size > 0 ? partitions[i].size / read_size : 0);
    }

    return ESP_OK;

cleanup:
    for (size_t j = 0; j < num_partitions; j++) {
        if (table->entries[j].handle) {
            table->entries[j].handle->ops->release(table->entries[j].handle);
            table->entries[j].handle = NULL;
        }
    }
    memset(table, 0, sizeof(*table));
    return ret;
}

esp_err_t spi_nand_flash_init_with_partitions(spi_nand_flash_config_t *config,
        const spi_nand_partition_config_t *partitions,
        size_t num_partitions,
        esp_blockdev_handle_t *wl_bdl)
{
    ESP_RETURN_ON_FALSE(config && partitions && wl_bdl, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    esp_err_t ret = spi_nand_flash_init_with_layers(config, wl_bdl);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = spi_nand_flash_register_partitions(*wl_bdl, partitions, num_partitions);
    if (ret != ESP_OK) {
        (*wl_bdl)->ops->release(*wl_bdl);
        *wl_bdl = NULL;
        return ret;
    }

    return ESP_OK;
}

esp_err_t spi_nand_flash_get_partition(esp_blockdev_handle_t wl_bdl,
                                       const char *name,
                                       esp_blockdev_handle_t *partition)
{
    ESP_RETURN_ON_FALSE(wl_bdl && name && partition, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    nand_partition_table_t *table = find_table(wl_bdl);
    ESP_RETURN_ON_FALSE(table != NULL, ESP_ERR_NOT_FOUND, TAG,
                        "No partitions registered for this device");

    for (size_t i = 0; i < table->num_partitions; i++) {
        if (strcmp(table->entries[i].name, name) == 0) {
            *partition = table->entries[i].handle;
            return ESP_OK;
        }
    }

    ESP_LOGD(TAG, "Partition '%s' not found", name);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t spi_nand_flash_release_partitions(esp_blockdev_handle_t wl_bdl)
{
    ESP_RETURN_ON_FALSE(wl_bdl != NULL, ESP_ERR_INVALID_ARG, TAG, "wl_bdl is NULL");

    nand_partition_table_t *table = find_table(wl_bdl);
    ESP_RETURN_ON_FALSE(table != NULL, ESP_ERR_NOT_FOUND, TAG,
                        "No partitions registered for this device");

    for (size_t i = 0; i < table->num_partitions; i++) {
        if (table->entries[i].handle) {
            table->entries[i].handle->ops->release(table->entries[i].handle);
            table->entries[i].handle = NULL;
        }
    }

    memset(table, 0, sizeof(*table));

    return ESP_OK;
}
