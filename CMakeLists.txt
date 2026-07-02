cmake_minimum_required(VERSION 3.15)
project(audio_playground)
enable_testing()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -D_REENTRANT")

option(APG_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer for debug verification" OFF)
if(APG_ENABLE_SANITIZERS)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
        add_link_options(-fsanitize=address,undefined)
    else()
        message(WARNING "APG_ENABLE_SANITIZERS is only configured for Clang/GNU C compilers")
    endif()
endif()

find_package(PkgConfig QUIET)
pkg_check_modules(PIPEWIRE QUIET libpipewire-0.3)

include_directories(
        ${CMAKE_SOURCE_DIR}/inc/
        ${CMAKE_SOURCE_DIR}/inc/rte
        ${CMAKE_SOURCE_DIR}/inc/unit
        ${CMAKE_SOURCE_DIR}/inc/util
        ${CMAKE_SOURCE_DIR}/inc/yaml
        ${PIPEWIRE_INCLUDE_DIRS}
)

file(GLOB APGCORE_SOURCES src/apgcore/*.c)
file(GLOB ATOM_SOURCES src/atom/*.c)
file(GLOB CTRL_SOURCES src/ctrl/*.c)
file(GLOB RUNT_SOURCES src/rte/*.c)
file(GLOB UNIT_SOURCES src/unit/*.c)
file(GLOB UTIL_SOURCES src/util/*.c)
file(GLOB YAML_SOURCES src/yaml/*.c)
file(GLOB WAVE_SOURCES src/wave.c)
list(REMOVE_ITEM UTIL_SOURCES "${CMAKE_SOURCE_DIR}/src/util/fast_chunk.c")
list(APPEND UTIL_SOURCES ${APGCORE_SOURCES})
list(APPEND UTIL_SOURCES ${ATOM_SOURCES})
list(APPEND UTIL_SOURCES ${CTRL_SOURCES})
list(APPEND UTIL_SOURCES ${UNIT_SOURCES})
list(APPEND UTIL_SOURCES ${RUNT_SOURCES})
list(APPEND UTIL_SOURCES ${YAML_SOURCES})
list(APPEND UTIL_SOURCES ${WAVE_SOURCES})

if(PIPEWIRE_FOUND)
    set(PIPEWIRE_SO "${PIPEWIRE_LIBDIR}/libpipewire-0.3.so")
    if(NOT EXISTS "${PIPEWIRE_SO}")
        set(PIPEWIRE_SO "${PIPEWIRE_LIBDIR}/libpipewire-0.3.so.0")
    endif()
    if(EXISTS "${PIPEWIRE_SO}")
        set(PIPEWIRE_LIB "${PIPEWIRE_SO}")
    else()
        set(PIPEWIRE_LIB "pipewire-0.3")
    endif()
    add_executable(live
            src/live.c
            src/util/fast_chunk.c
            ${UTIL_SOURCES}
    )
    target_link_libraries(live "${PIPEWIRE_LIB}" m)

    add_executable(test_runtime
            src/test_runtime.c
            src/util/fast_chunk.c
            ${UTIL_SOURCES}
    )
    target_link_libraries(test_runtime "${PIPEWIRE_LIB}" m)
endif()

add_executable(fast_math_test
        src/fast_math_test.c
        ${UTIL_SOURCES}
        inc/atom/dsp_types.h
)
target_link_libraries(fast_math_test m)

add_executable(apg-v2
        tools/apg_v2_cli.c
        ${UTIL_SOURCES}
)
target_link_libraries(apg-v2 m)
if(PIPEWIRE_FOUND)
    target_link_libraries(apg-v2 "${PIPEWIRE_LIB}")
endif()

set(TEST_TARGETS
        test_atom_basic
        test_atom_catalog
        test_project_v2_validate
        test_contract_parser_v2
        test_contract_validator_v2
        test_project_v2_compile
        test_v2_json_contract
        test_unit_load_all
        test_ctrl_transition
        test_runtime_process_frames
        test_unit_adapters_frame_limits
        test_unit_v2_validate
        test_unit_v2_compile
        test_unit_v2_runtime
        test_runtime_image_v2
        test_measure_v2
        test_offline_chain
        test_hall_reverb
)

foreach(TEST_TARGET ${TEST_TARGETS})
    if(TEST_TARGET STREQUAL "test_atom_basic")
        set(TEST_ATOM_BASIC_SOURCES
                test/test_atom_basic.c
                test/test_atom_basic_basics.c
                test/test_process_info_frame_limits.c
                test/test_process_info_modulation_frame_limits.c
                test/test_process_info_control_frame_limits.c
                test/test_process_info_delay_frame_limits.c
                test/test_process_info_arithmetic_mix_frame_limits.c
                test/test_process_info_amplitude_state_frame_limits.c
                test/test_process_info_generator_frame_limits.c
                test/test_process_info_modulation_remaining_frame_limits.c
                test/test_process_info_remaining_detector_frame_limits.c
                test/test_process_info_remaining_filter_frame_limits.c
                test/test_process_info_remaining_mix_nonlinear_frame_limits.c
                test/test_process_info_remaining_interpolation_src_frame_limits.c
                test/test_process_info_remaining_spectral_frame_limits.c
                test/test_process_info_downsample_frame_limits.c
                test/test_process_info_remaining_overlap_frame_limits.c
                test/test_process_info_freq_shift_frame_limits.c
        )
        add_executable(${TEST_TARGET}
                ${TEST_ATOM_BASIC_SOURCES}
                ${UTIL_SOURCES}
        )
    else()
        add_executable(${TEST_TARGET}
                test/${TEST_TARGET}.c
                ${UTIL_SOURCES}
        )
    endif()
    target_link_libraries(${TEST_TARGET} m)
    if(PIPEWIRE_FOUND)
        target_link_libraries(${TEST_TARGET} "${PIPEWIRE_LIB}")
    endif()
    add_test(NAME ${TEST_TARGET} COMMAND ${TEST_TARGET})
    set_tests_properties(${TEST_TARGET} PROPERTIES
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    if(TEST_TARGET MATCHES "^test_unit_v2_" OR TEST_TARGET STREQUAL "test_atom_catalog" OR TEST_TARGET STREQUAL "test_contract_parser_v2" OR TEST_TARGET STREQUAL "test_contract_validator_v2" OR TEST_TARGET STREQUAL "test_runtime_image_v2" OR TEST_TARGET STREQUAL "test_measure_v2" OR TEST_TARGET MATCHES "^test_project_v2_" OR TEST_TARGET STREQUAL "test_v2_json_contract")
        set_tests_properties(${TEST_TARGET} PROPERTIES LABELS v2)
    endif()
endforeach()

add_custom_target(check_v2
        COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} -L v2 --output-on-failure
        DEPENDS test_atom_catalog test_contract_parser_v2 test_contract_validator_v2 test_project_v2_validate test_project_v2_compile test_v2_json_contract test_unit_v2_validate test_unit_v2_compile test_unit_v2_runtime test_runtime_image_v2 test_measure_v2
)

add_test(
        NAME test_apg_v2_cli_benchmark
        COMMAND apg-v2 benchmark project projects-v2/guitar-pedalboard.project.v2.yaml
)
set_tests_properties(test_apg_v2_cli_benchmark PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        LABELS v2
        PASS_REGULAR_EXPRESSION "\"schema\":\"apg.project.benchmark.v1\".*\"ok\":true.*\"structural\""
)

add_test(
        NAME test_apg_v2_cli_wasm_export_blocked
        COMMAND /bin/sh -c "$<TARGET_FILE:apg-v2> export --target wasm_realtime projects-v2/guitar-pedalboard.project.v2.yaml /tmp/apg-wasm-export || true"
)
set_tests_properties(test_apg_v2_cli_wasm_export_blocked PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        LABELS v2
        PASS_REGULAR_EXPRESSION "\"schema\":\"apg.project.export.v1\".*\"target\":\"wasm_realtime\".*\"APG_EXPORT_BLOCKED\""
)

add_test(
        NAME test_apg_v2_cli_m7_export_rejects_unsupported
        COMMAND /bin/sh -c "$<TARGET_FILE:apg-v2> export --target m7_static projects-v2/guitar-pedalboard.project.v2.yaml /tmp || true"
)
set_tests_properties(test_apg_v2_cli_m7_export_rejects_unsupported PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        LABELS v2
        PASS_REGULAR_EXPRESSION "\"target\":\"m7_static\".*does not support m7_static"
)

set(APG_M7_CLI_TEST_DIR /tmp/audio-playground-apgcore-m7-export)
add_test(NAME test_apg_v2_cli_m7_export_prepare COMMAND ${CMAKE_COMMAND} -E make_directory ${APG_M7_CLI_TEST_DIR})
set_tests_properties(test_apg_v2_cli_m7_export_prepare PROPERTIES LABELS v2)

add_test(
        NAME test_apg_v2_cli_m7_export_bundle
        COMMAND apg-v2 export --target m7_static projects-v2/two-gain-chain.project.v2.yaml ${APG_M7_CLI_TEST_DIR}
)
set_tests_properties(test_apg_v2_cli_m7_export_bundle PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        LABELS v2
        DEPENDS test_apg_v2_cli_m7_export_prepare
        PASS_REGULAR_EXPRESSION "\"ok\":true.*\"files\":\\[\"apg_project_m7.h\",\"apg_project_m7.c\"\\]"
)

add_test(NAME test_apg_v2_cli_m7_export_header COMMAND ${CMAKE_COMMAND} -E cat ${APG_M7_CLI_TEST_DIR}/apg_project_m7.h)
set_tests_properties(test_apg_v2_cli_m7_export_header PROPERTIES
        LABELS v2
        DEPENDS test_apg_v2_cli_m7_export_bundle
        PASS_REGULAR_EXPRESSION "APG_M7_PROJECT_NODE_COUNT 4u"
)
