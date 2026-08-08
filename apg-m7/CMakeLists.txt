add_library(apg_m7_firmware STATIC
        src/bsp/bsp_hardware.c
        src/usb/usb_host.c
        src/usb/usb_dfu.c
        src/fs/preset_fs.c
        src/flash/flash_flasher.c
        src/flash/crc32.c
        src/audio/audio_driver.c
        src/audio/audio_engine.c
)

target_include_directories(apg_m7_firmware PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/apg-core/include
        ${CMAKE_SOURCE_DIR}/apg-core/inc
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(apg_m7_firmware PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Generic" OR CMAKE_C_COMPILER_TARGET MATCHES "arm")
    target_compile_options(apg_m7_firmware PRIVATE -mcpu=cortex-m7 -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb)
endif()

target_link_libraries(apg_m7_firmware PUBLIC apgcore)

add_executable(apg-m7-app src/main.c)
target_link_libraries(apg-m7-app PRIVATE apg_m7_firmware)

add_executable(test_m7_firmware_backbone test/test_m7_firmware_backbone.c)
target_link_libraries(test_m7_firmware_backbone PRIVATE apg_m7_firmware)
add_test(NAME test_m7_firmware_backbone COMMAND test_m7_firmware_backbone)
set_tests_properties(test_m7_firmware_backbone PROPERTIES LABELS "v2;apg-m7")

apg_ide_sources(apg_m7_sources C
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*
        ${CMAKE_CURRENT_SOURCE_DIR}/include/apg_m7/*
        ${CMAKE_CURRENT_SOURCE_DIR}/include/apg_m7/domain/*
        ${CMAKE_CURRENT_SOURCE_DIR}/*.ld
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
)
