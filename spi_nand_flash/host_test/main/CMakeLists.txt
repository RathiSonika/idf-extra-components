set(src "test_app_main.cpp")

if(CONFIG_NAND_FLASH_ENABLE_BDL)
    list(APPEND src "test_nand_flash_bdl.cpp")
else()
    list(APPEND src  "test_nand_flash.cpp" "test_nand_flash_ftl.cpp")
endif()

if(CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT)
    list(APPEND src "test_nand_oob_layout.cpp")
endif()

idf_component_register(SRCS ${src}
                       WHOLE_ARCHIVE
                       )

target_link_libraries(${COMPONENT_LIB} PRIVATE Catch2WithMain)

if(CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT)
    target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../priv_include")
endif()
