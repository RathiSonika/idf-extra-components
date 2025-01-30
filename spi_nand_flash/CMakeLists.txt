idf_build_get_property(target IDF_TARGET)

set(reqs fatfs)
set(inc diskio include)
set(priv_inc priv_include)

if(${target} STREQUAL "linux")

list(APPEND reqs esp_partition)
set(srcs "src/nand_linux.c"
         "src/nand_impl_linux.c"
         "src/dhara_glue.c"
         "diskio/diskio_nand.c")

else()

set(srcs "src/nand.c"
         "src/nand_winbond.c"
         "src/nand_gigadevice.c"
         "src/nand_alliance.c"
         "src/nand_micron.c"
         "src/nand_impl.c"
         "src/nand_impl_wrap.c"
         "src/nand_diag_api.c"
         "src/spi_nand_oper.c"
         "src/dhara_glue.c"
         "vfs/vfs_fat_spinandflash.c"
         "diskio/diskio_nand.c")

set(priv_reqs vfs)
list(APPEND inc vfs)

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER "5.3")
    list(APPEND reqs esp_driver_spi)
else()
    list(APPEND reqs driver)
endif()

endif()


idf_component_register(SRCS ${srcs}
        INCLUDE_DIRS ${inc}
        PRIV_INCLUDE_DIRS ${priv_inc}
        REQUIRES ${reqs}
        PRIV_REQUIRES ${priv_reqs})
