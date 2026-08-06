if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" LESS "6.0")
    message(FATAL_ERROR "spi_nand_flash_littlefs requires ESP-IDF 6.0 or later (esp_blockdev and LittleFS blockdev support)")
endif()

idf_component_register(
    SRCS "src/vfs_littlefs_spinandflash.c"
    INCLUDE_DIRS "include"
    REQUIRES spi_nand_flash joltwallet__littlefs
    PRIV_REQUIRES vfs
)
