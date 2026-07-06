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

include_directories(
        ${CMAKE_SOURCE_DIR}/inc/
        ${CMAKE_SOURCE_DIR}/inc/rte
        ${CMAKE_SOURCE_DIR}/inc/util
        ${CMAKE_SOURCE_DIR}/inc/yaml
)

file(GLOB_RECURSE APGCORE_SOURCES CONFIGURE_DEPENDS src/apgcore/*.c)
file(GLOB_RECURSE ATOM_SOURCES CONFIGURE_DEPENDS src/atom/*.c)
set(ATOM_REGISTRY_SOURCE src/rte/atom_register.c)
set(ATOM_THUNK_SOURCE src/rte/atom_thunk.c)
file(GLOB UTIL_SOURCES src/util/*.c)
file(GLOB YAML_SOURCES src/yaml/*.c)
list(APPEND UTIL_SOURCES ${APGCORE_SOURCES})
list(APPEND UTIL_SOURCES ${ATOM_SOURCES})
list(APPEND UTIL_SOURCES ${ATOM_REGISTRY_SOURCE})
list(APPEND UTIL_SOURCES ${ATOM_THUNK_SOURCE})
list(APPEND UTIL_SOURCES ${YAML_SOURCES})

add_executable(test_runtime
        src/test_runtime.c
        ${UTIL_SOURCES}
)
target_link_libraries(test_runtime m)

add_executable(apg-v2
        tools/apg_v2_cli.c
        ${UTIL_SOURCES}
)
target_link_libraries(apg-v2 m)

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
        test_unit_v2_validate
        test_unit_v2_compile
        test_unit_v2_runtime
        test_registry_v2
        test_measure_v2
        test_public_headers
        test_offline_chain
        test_chain_guitar_sample
        test_hall_reverb
        test_host_v2_project
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
    add_test(NAME ${TEST_TARGET} COMMAND ${TEST_TARGET})
    set_tests_properties(${TEST_TARGET} PROPERTIES
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    if(TEST_TARGET MATCHES "^test_unit_v2_" OR TEST_TARGET STREQUAL "test_unit_load_all" OR TEST_TARGET STREQUAL "test_offline_chain" OR
            TEST_TARGET STREQUAL "test_hall_reverb" OR TEST_TARGET STREQUAL "test_host_v2_project" OR TEST_TARGET STREQUAL "test_atom_catalog" OR
            TEST_TARGET STREQUAL "test_contract_parser_v2" OR TEST_TARGET STREQUAL "test_contract_validator_v2" OR TEST_TARGET STREQUAL "test_registry_v2" OR
            TEST_TARGET STREQUAL "test_measure_v2" OR TEST_TARGET STREQUAL "test_public_headers" OR TEST_TARGET STREQUAL "test_chain_guitar_sample" OR TEST_TARGET MATCHES "^test_project_v2_" OR TEST_TARGET STREQUAL "test_v2_json_contract")
        set_tests_properties(${TEST_TARGET} PROPERTIES LABELS v2)
    endif()
endforeach()

add_custom_target(check_v2
        COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} -L v2 --output-on-failure
        DEPENDS test_atom_catalog test_contract_parser_v2 test_contract_validator_v2 test_project_v2_validate test_project_v2_compile test_v2_json_contract test_unit_v2_validate test_unit_v2_compile test_unit_v2_runtime test_registry_v2 test_measure_v2 test_public_headers test_host_v2_project test_chain_guitar_sample
)

add_test(
        NAME test_apg_v2_cli_benchmark
        COMMAND apg-v2 benchmark project test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml
)
set_tests_properties(test_apg_v2_cli_benchmark PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        LABELS v2
        PASS_REGULAR_EXPRESSION "\"schema\":\"apg.project.benchmark.v2\".*\"ok\":true.*\"structural\""
)

add_test(
        NAME test_apg_v2_cli_wasm_export_prepare
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test-artifacts/wasm-export
)
set_tests_properties(test_apg_v2_cli_wasm_export_prepare PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        LABELS v2
)

add_test(
        NAME test_apg_v2_cli_wasm_export
        COMMAND /bin/sh -c "$<TARGET_FILE:apg-v2> export --target wasm_realtime test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml ${CMAKE_BINARY_DIR}/test-artifacts/wasm-export"
)
set_tests_properties(test_apg_v2_cli_wasm_export PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        LABELS v2
        DEPENDS test_apg_v2_cli_wasm_export_prepare
        PASS_REGULAR_EXPRESSION "\"schema\":\"apg.project.export.v2\".*\"ok\":true.*\"target\":\"wasm_realtime\".*\"files\":\\[\"apg_project_wasm.json\",\"apg_project_wasm.mjs\",\"apg_project_wasm_adapter.mjs\",\"apg_project_wasm_processor.js\"\\]"
)

add_test(
        NAME test_apg_v2_cli_wasm_export_manifest
        COMMAND ${CMAKE_COMMAND} -E cat ${CMAKE_BINARY_DIR}/test-artifacts/wasm-export/apg_project_wasm.json
)
set_tests_properties(test_apg_v2_cli_wasm_export_manifest PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        LABELS v2
        PASS_REGULAR_EXPRESSION "\"schema\":\"apg.project.wasm_realtime.v2\".*\"project\":\"guitar-pedalboard\".*\"runtime\":\"audio_worklet_stub\".*\"status\":\"generated\".*\"artifacts\":\\{.*\"adapter_js\":\"apg_project_wasm_adapter.mjs\".*\"worklet_processor_js\":\"apg_project_wasm_processor.js\".*\"wasm_module\":\"apg_project_wasm.wasm\".*\"wasm_module_available\":false"
        DEPENDS test_apg_v2_cli_wasm_export
)

add_test(
        NAME test_apg_v2_cli_wasm_export_js
        COMMAND ${CMAKE_COMMAND} -E cat ${CMAKE_BINARY_DIR}/test-artifacts/wasm-export/apg_project_wasm.mjs
)
set_tests_properties(test_apg_v2_cli_wasm_export_js PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        LABELS v2
        PASS_REGULAR_EXPRESSION "audio-worklet-contract.*apg_project_wasm_adapter\\.mjs"
        DEPENDS test_apg_v2_cli_wasm_export
)

add_test(
        NAME test_apg_v2_cli_wasm_export_adapter_js
        COMMAND ${CMAKE_COMMAND} -E cat ${CMAKE_BINARY_DIR}/test-artifacts/wasm-export/apg_project_wasm_adapter.mjs
)
set_tests_properties(test_apg_v2_cli_wasm_export_adapter_js PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        LABELS v2
        PASS_REGULAR_EXPRESSION "runtimeMethods.*compile.*start.*stop.*setParam.*setBypass.*pollMeters.*getLastError.*runtimeNames.*gate1\\.threshold.*drive1\\.drive.*inputMeters.*input.*outputMeters.*output.*audioWorklet\\.addModule.*AudioWorkletNode.*disconnect.*unknown param.*unknown bypass instance"
        DEPENDS test_apg_v2_cli_wasm_export
)

add_test(
        NAME test_apg_v2_cli_wasm_export_processor_js
        COMMAND ${CMAKE_COMMAND} -E cat ${CMAKE_BINARY_DIR}/test-artifacts/wasm-export/apg_project_wasm_processor.js
)
set_tests_properties(test_apg_v2_cli_wasm_export_processor_js PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        LABELS v2
        PASS_REGULAR_EXPRESSION "registerProcessor\\(\"apg-project-wasm-processor\""
        DEPENDS test_apg_v2_cli_wasm_export
)

add_test(
        NAME test_apg_v2_cli_wasm_export_rejects_unsupported_atom
        COMMAND /bin/sh -c "$<TARGET_FILE:apg-v2> export --target wasm_realtime test/fixtures/projects-v2/wasm-unsupported-freq.project.v2.yaml ${CMAKE_BINARY_DIR} || true"
)
set_tests_properties(test_apg_v2_cli_wasm_export_rejects_unsupported_atom PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        LABELS v2
        PASS_REGULAR_EXPRESSION "\"target\":\"wasm_realtime\".*atom 'freq_fft' does not support wasm_realtime"
)

add_test(
        NAME test_apg_v2_cli_m7_export_rejects_unsupported
        COMMAND /bin/sh -c "$<TARGET_FILE:apg-v2> export --target m7_static test/fixtures/projects-v2/m7-unsupported-profile.project.v2.yaml ${CMAKE_BINARY_DIR} || true"
)
set_tests_properties(test_apg_v2_cli_m7_export_rejects_unsupported PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        LABELS v2
        PASS_REGULAR_EXPRESSION "\"target\":\"m7_static\".*unit 'unsupported_profile_unit' does not support m7_static"
)

add_test(
        NAME test_apg_v2_cli_m7_export_rejects_unsupported_atom
        COMMAND /bin/sh -c "$<TARGET_FILE:apg-v2> export --target m7_static test/fixtures/projects-v2/m7-unsupported-src.project.v2.yaml ${CMAKE_BINARY_DIR} || true"
)
set_tests_properties(test_apg_v2_cli_m7_export_rejects_unsupported_atom PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        LABELS v2
        PASS_REGULAR_EXPRESSION "\"target\":\"m7_static\".*atom 'src_downsample' does not support m7_static"
)

add_test(
        NAME test_apg_v2_cli_m7_export_rejects_undeclared_compat
        COMMAND /bin/sh -c "$<TARGET_FILE:apg-v2> export --target m7_static test/fixtures/projects-v2/m7-unsupported-compat.project.v2.yaml ${CMAKE_BINARY_DIR} || true"
)
set_tests_properties(test_apg_v2_cli_m7_export_rejects_undeclared_compat PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        LABELS v2
        PASS_REGULAR_EXPRESSION "\"target\":\"m7_static\".*does not declare m7_static compatibility"
)

add_test(
        NAME test_apg_v2_cli_m7_export_rejects_output_file
        COMMAND /bin/sh -c "$<TARGET_FILE:apg-v2> export --target m7_static test/fixtures/projects-v2/two-gain-chain.project.v2.yaml ${CMAKE_BINARY_DIR}/CMakeCache.txt || true"
)
set_tests_properties(test_apg_v2_cli_m7_export_rejects_output_file PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        LABELS v2
        PASS_REGULAR_EXPRESSION "\"target\":\"m7_static\".*\"APG_IO_ERROR\".*export output path is not a directory"
)

set(APG_M7_CLI_TEST_DIR ${CMAKE_BINARY_DIR}/test-artifacts/m7-export)
add_test(NAME test_apg_v2_cli_m7_export_prepare COMMAND ${CMAKE_COMMAND} -E make_directory ${APG_M7_CLI_TEST_DIR})
set_tests_properties(test_apg_v2_cli_m7_export_prepare PROPERTIES LABELS v2)

add_test(
        NAME test_apg_v2_cli_m7_export_bundle
        COMMAND apg-v2 export --target m7_static test/fixtures/projects-v2/two-gain-chain.project.v2.yaml ${APG_M7_CLI_TEST_DIR}
)
set_tests_properties(test_apg_v2_cli_m7_export_bundle PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        LABELS v2
        DEPENDS test_apg_v2_cli_m7_export_prepare
        PASS_REGULAR_EXPRESSION "\"ok\":true.*\"files\":\\[\"apg_project_m7.h\",\"apg_project_m7.c\"\\].*\"memory\":\\{.*\"atom_call_bytes\":[0-9]+.*\"signal_array_pointer_bytes\":[0-9]+.*\"static_ram_bytes\":[0-9]+.*\"execution\":\\{.*\"atom_calls_per_block\":[0-9]+.*\"atom_calls_per_second\":[0-9]+"
)

add_test(NAME test_apg_v2_cli_m7_export_header COMMAND ${CMAKE_COMMAND} -E cat ${APG_M7_CLI_TEST_DIR}/apg_project_m7.h)
set_tests_properties(test_apg_v2_cli_m7_export_header PROPERTIES
        LABELS v2
        DEPENDS test_apg_v2_cli_m7_export_bundle
        PASS_REGULAR_EXPRESSION "APG_M7_PROJECT_NODE_COUNT 4u.*APG_M7_PROJECT_BLOCK_FRAMES 64u.*APG_M7_PROJECT_SAMPLE_RATE 48000u.*APG_M7_PROJECT_ATOM_CALLS_PER_BLOCK [0-9]+u.*APG_M7_PROJECT_ATOM_CALL_BYTES [0-9]+u.*APG_M7_PROJECT_SIGNAL_ARRAY_POINTER_BYTES [0-9]+u.*APG_M7_PROJECT_STATIC_RAM_BYTES [0-9]+u.*APG_M7_CACHE_LINE_BYTES 32u.*APG_M7_SECTION_SIGNAL_BUFFERS.*APG_M7_SECTION_PARAMS.*APG_M7_SECTION_ATOM_CALLS.*APG_M7_SECTION_SIGNAL_ARRAYS.*APG_M7_SECTION_ATOM_STORAGE.*APG_M7_SECTION_STATE_BUFFERS.*aligned\\(APG_M7_CACHE_LINE_BYTES\\).*APG_M7_PROJECT_USES_RUNTIME_YAML 0u.*APG_M7_PROJECT_USES_DYNAMIC_ALLOCATION 0u.*apg_m7_project_init.*apg_m7_project_refresh_params.*apg_m7_project_process_block"
)

add_test(
        NAME test_apg_v2_cli_m7_export_source_static
        COMMAND /bin/sh -c
                "cat ${APG_M7_CLI_TEST_DIR}/apg_project_m7.c && grep -Eq 'uint8_t apg_m7_project_atom_storage\\[APG_M7_PROJECT_ATOM_STORAGE_BYTES\\]' ${APG_M7_CLI_TEST_DIR}/apg_project_m7.c && grep -Eq 'atom_call_t apg_m7_project_atom_calls\\[APG_M7_PROJECT_NODE_COUNT\\].*apg_m7_project_atom_storage\\[[0-9]+u\\]' ${APG_M7_CLI_TEST_DIR}/apg_project_m7.c && grep -Eq 'generation_dc_params_t.*apg_m7_project_atom_storage\\[[0-9]+u\\]' ${APG_M7_CLI_TEST_DIR}/apg_project_m7.c && grep -Eq 'amplitude_multiply_in_t.*apg_m7_project_atom_storage\\[[0-9]+u\\]' ${APG_M7_CLI_TEST_DIR}/apg_project_m7.c"
)
set_tests_properties(test_apg_v2_cli_m7_export_source_static PROPERTIES
        LABELS v2
        DEPENDS test_apg_v2_cli_m7_export_bundle
        FAIL_REGULAR_EXPRESSION "malloc|calloc|realloc|free|uc_arena|yaml|load_resolved|runtime_init"
)

add_test(
        NAME test_apg_v2_cli_m7_export_object
        COMMAND ${CMAKE_C_COMPILER} -std=c11 -D_GNU_SOURCE -I${CMAKE_SOURCE_DIR}/inc -I${CMAKE_SOURCE_DIR}/inc/rte -I${APG_M7_CLI_TEST_DIR} -c ${APG_M7_CLI_TEST_DIR}/apg_project_m7.c -o ${APG_M7_CLI_TEST_DIR}/apg_project_m7.o
)
set_tests_properties(test_apg_v2_cli_m7_export_object PROPERTIES
        LABELS v2
        DEPENDS test_apg_v2_cli_m7_export_bundle
)

if(CMAKE_OBJDUMP)
    add_test(NAME test_apg_v2_cli_m7_export_sections COMMAND ${CMAKE_OBJDUMP} -h ${APG_M7_CLI_TEST_DIR}/apg_project_m7.o)
    set_tests_properties(test_apg_v2_cli_m7_export_sections PROPERTIES
            LABELS v2
            DEPENDS test_apg_v2_cli_m7_export_object
            PASS_REGULAR_EXPRESSION "\\.apg_m7_signal_buffers.*\\.apg_m7_params.*\\.apg_m7_atom_storage.*\\.apg_m7_atom_calls"
    )
else()
    add_test(
            NAME test_apg_v2_cli_m7_export_sections_unconfigured
            COMMAND ${CMAKE_COMMAND} -E echo "CMAKE_OBJDUMP not configured; skipping generated M7 section gate"
    )
    set_tests_properties(test_apg_v2_cli_m7_export_sections_unconfigured PROPERTIES
            LABELS v2
            PASS_REGULAR_EXPRESSION "skipping generated M7 section gate"
    )
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_test(
            NAME test_apg_v2_cli_m7_export_stack_usage
            COMMAND /bin/sh -c "${CMAKE_C_COMPILER} -std=c11 -D_GNU_SOURCE -I${CMAKE_SOURCE_DIR}/inc -I${CMAKE_SOURCE_DIR}/inc/rte -I${APG_M7_CLI_TEST_DIR} -fstack-usage -c ${APG_M7_CLI_TEST_DIR}/apg_project_m7.c -o ${APG_M7_CLI_TEST_DIR}/apg_project_m7_stack.o && awk '/apg_m7_project_process_block/ { print; found=1; if ($2 > 128) exit 2 } END { exit found ? 0 : 1 }' ${APG_M7_CLI_TEST_DIR}/apg_project_m7_stack.su"
    )
    set_tests_properties(test_apg_v2_cli_m7_export_stack_usage PROPERTIES
            LABELS v2
            DEPENDS test_apg_v2_cli_m7_export_bundle
            PASS_REGULAR_EXPRESSION "apg_m7_project_process_block"
    )
else()
    add_test(
            NAME test_apg_v2_cli_m7_export_stack_usage_unconfigured
            COMMAND ${CMAKE_COMMAND} -E echo "stack usage gate requires GCC or Clang; skipping generated M7 stack gate"
    )
    set_tests_properties(test_apg_v2_cli_m7_export_stack_usage_unconfigured PROPERTIES
            LABELS v2
            PASS_REGULAR_EXPRESSION "skipping generated M7 stack gate"
    )
endif()

add_test(
        NAME test_apg_v2_cli_m7_export_link_smoke
        COMMAND ${CMAKE_C_COMPILER} -std=c11 -D_GNU_SOURCE -I${CMAKE_SOURCE_DIR}/inc -I${CMAKE_SOURCE_DIR}/inc/rte -I${APG_M7_CLI_TEST_DIR} ${APG_M7_CLI_TEST_DIR}/apg_project_m7.c ${CMAKE_SOURCE_DIR}/test/test_m7_static_bundle_smoke.c ${ATOM_SOURCES} ${ATOM_REGISTRY_SOURCE} ${ATOM_THUNK_SOURCE} -lm -o ${APG_M7_CLI_TEST_DIR}/apg_m7_static_smoke
)
set_tests_properties(test_apg_v2_cli_m7_export_link_smoke PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        LABELS v2
        DEPENDS test_apg_v2_cli_m7_export_bundle
)

add_test(NAME test_apg_v2_cli_m7_export_run_smoke COMMAND ${APG_M7_CLI_TEST_DIR}/apg_m7_static_smoke)
set_tests_properties(test_apg_v2_cli_m7_export_run_smoke PROPERTIES
        LABELS v2
        DEPENDS test_apg_v2_cli_m7_export_link_smoke
        PASS_REGULAR_EXPRESSION "m7_static_host_block_us=.*budget_us=1000.000"
)

add_test(
        NAME test_apg_v2_cli_m7_export_static_ram_budget
        COMMAND /bin/sh -c "$<TARGET_FILE:apg-v2> export --target m7_static --max-static-ram 100 test/fixtures/projects-v2/two-gain-chain.project.v2.yaml ${APG_M7_CLI_TEST_DIR} || true"
)
set_tests_properties(test_apg_v2_cli_m7_export_static_ram_budget PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        LABELS v2
        DEPENDS test_apg_v2_cli_m7_export_bundle
        PASS_REGULAR_EXPRESSION "\"target\":\"m7_static\".*\"APG_RANGE_ERROR\".*static RAM budget exceeded"
)

set(APG_M7_CLI_CUSTOM_TEST_DIR ${CMAKE_BINARY_DIR}/test-artifacts/m7-export-custom)
add_test(NAME test_apg_v2_cli_m7_export_custom_prepare COMMAND ${CMAKE_COMMAND} -E make_directory ${APG_M7_CLI_CUSTOM_TEST_DIR})
set_tests_properties(test_apg_v2_cli_m7_export_custom_prepare PROPERTIES LABELS v2)
add_test(
        NAME test_apg_v2_cli_m7_export_custom_timing
        COMMAND apg-v2 export --target m7_static --block-frames 128 --sample-rate 96000 --cache-line-bytes 64 test/fixtures/projects-v2/two-gain-chain.project.v2.yaml ${APG_M7_CLI_CUSTOM_TEST_DIR}
)
set_tests_properties(test_apg_v2_cli_m7_export_custom_timing PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        LABELS v2
        DEPENDS test_apg_v2_cli_m7_export_custom_prepare
        PASS_REGULAR_EXPRESSION "\"target\":\"m7_static\".*\"block_frames\":128.*\"cache_line_bytes\":64.*\"sample_rate\":96000"
)
add_test(NAME test_apg_v2_cli_m7_export_custom_header COMMAND ${CMAKE_COMMAND} -E cat ${APG_M7_CLI_CUSTOM_TEST_DIR}/apg_project_m7.h)
set_tests_properties(test_apg_v2_cli_m7_export_custom_header PROPERTIES
        LABELS v2
        DEPENDS test_apg_v2_cli_m7_export_custom_timing
        PASS_REGULAR_EXPRESSION "APG_M7_PROJECT_BLOCK_FRAMES 128u.*APG_M7_PROJECT_SAMPLE_RATE 96000u.*APG_M7_CACHE_LINE_BYTES 64u"
)

set(APG_M7_C_COMPILER "" CACHE FILEPATH "Optional ARM/M7 C compiler used to syntax-check generated m7_static bundles")
set(APG_M7_LINKER_SCRIPT "" CACHE FILEPATH "Optional STM32H7/M7 linker script used to link generated m7_static bundles")
set(APG_M7_BOARD_TIMING_COMMAND "" CACHE STRING "Optional command that measures a generated m7_static bundle on board; receives export dir as last argument")
if(NOT APG_M7_C_COMPILER)
    find_program(APG_M7_C_COMPILER_DETECTED arm-none-eabi-gcc)
    if(APG_M7_C_COMPILER_DETECTED)
        set(APG_M7_C_COMPILER "${APG_M7_C_COMPILER_DETECTED}" CACHE FILEPATH
                "Optional ARM/M7 C compiler used to syntax-check generated m7_static bundles" FORCE)
    endif()
endif()
if(APG_M7_C_COMPILER AND NOT APG_M7_LINKER_SCRIPT)
    set(APG_M7_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/test/fixtures/m7/stm32h7_smoke.ld" CACHE FILEPATH
            "Optional STM32H7/M7 linker script used to link generated m7_static bundles" FORCE)
endif()

if(APG_M7_C_COMPILER)
    add_test(
            NAME test_apg_v2_cli_m7_export_arm_syntax
            COMMAND ${APG_M7_C_COMPILER} -mcpu=cortex-m7 -mthumb -std=c11 -ffreestanding -I${CMAKE_SOURCE_DIR}/inc -I${CMAKE_SOURCE_DIR}/inc/rte -fsyntax-only ${APG_M7_CLI_TEST_DIR}/apg_project_m7.c
    )
    set_tests_properties(test_apg_v2_cli_m7_export_arm_syntax PROPERTIES
            LABELS v2
            DEPENDS test_apg_v2_cli_m7_export_bundle
    )
    add_test(
            NAME test_apg_v2_cli_m7_export_arm_stack_usage
            COMMAND /bin/sh -c "${APG_M7_C_COMPILER} -mcpu=cortex-m7 -mthumb -std=c11 -ffreestanding -I${CMAKE_SOURCE_DIR}/inc -I${CMAKE_SOURCE_DIR}/inc/rte -fstack-usage -c ${APG_M7_CLI_TEST_DIR}/apg_project_m7.c -o ${APG_M7_CLI_TEST_DIR}/apg_project_m7_arm_stack.o && awk '/apg_m7_project_process_block/ { print; found=1; if ($2 > 128) exit 2 } END { exit found ? 0 : 1 }' ${APG_M7_CLI_TEST_DIR}/apg_project_m7_arm_stack.su"
    )
    set_tests_properties(test_apg_v2_cli_m7_export_arm_stack_usage PROPERTIES
            LABELS v2
            DEPENDS test_apg_v2_cli_m7_export_bundle
            PASS_REGULAR_EXPRESSION "apg_m7_project_process_block"
    )
else()
    add_test(
            NAME test_apg_v2_cli_m7_export_arm_syntax_unconfigured
            COMMAND ${CMAKE_COMMAND} -E echo "APG_M7_C_COMPILER not configured; skipping ARM/M7 syntax gate"
    )
    set_tests_properties(test_apg_v2_cli_m7_export_arm_syntax_unconfigured PROPERTIES
            LABELS v2
            PASS_REGULAR_EXPRESSION "skipping ARM/M7 syntax gate"
    )
endif()

if(APG_M7_C_COMPILER AND APG_M7_LINKER_SCRIPT)
    add_test(
            NAME test_apg_v2_cli_m7_export_arm_link
            COMMAND ${APG_M7_C_COMPILER} -mcpu=cortex-m7 -mthumb -std=c11 -D_GNU_SOURCE -DM_PI=3.14159265358979323846 -DM_SQRT1_2=0.70710678118654752440 -ffreestanding -I${CMAKE_SOURCE_DIR}/inc -I${CMAKE_SOURCE_DIR}/inc/rte -I${APG_M7_CLI_TEST_DIR} -T ${APG_M7_LINKER_SCRIPT} ${APG_M7_CLI_TEST_DIR}/apg_project_m7.c ${CMAKE_SOURCE_DIR}/test/test_m7_static_link_smoke.c ${ATOM_SOURCES} ${ATOM_REGISTRY_SOURCE} ${ATOM_THUNK_SOURCE} -Wl,-Map=${APG_M7_CLI_TEST_DIR}/apg_m7_static.map -Wl,--gc-sections -lm -o ${APG_M7_CLI_TEST_DIR}/apg_m7_static.elf
    )
    set_tests_properties(test_apg_v2_cli_m7_export_arm_link PROPERTIES
            LABELS v2
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            DEPENDS test_apg_v2_cli_m7_export_bundle
    )
else()
    add_test(
            NAME test_apg_v2_cli_m7_export_arm_link_unconfigured
            COMMAND ${CMAKE_COMMAND} -E echo "APG_M7_C_COMPILER or APG_M7_LINKER_SCRIPT not configured; skipping ARM/M7 link gate"
    )
    set_tests_properties(test_apg_v2_cli_m7_export_arm_link_unconfigured PROPERTIES
            LABELS v2
            PASS_REGULAR_EXPRESSION "skipping ARM/M7 link gate"
    )
endif()

if(APG_M7_BOARD_TIMING_COMMAND)
    separate_arguments(APG_M7_BOARD_TIMING_COMMAND_LIST NATIVE_COMMAND "${APG_M7_BOARD_TIMING_COMMAND}")
    add_test(NAME test_apg_v2_cli_m7_export_board_timing COMMAND ${APG_M7_BOARD_TIMING_COMMAND_LIST} ${APG_M7_CLI_TEST_DIR})
    set_tests_properties(test_apg_v2_cli_m7_export_board_timing PROPERTIES
            LABELS v2
            DEPENDS test_apg_v2_cli_m7_export_bundle
            PASS_REGULAR_EXPRESSION "m7_static_board_block_us=.*budget_us="
    )
else()
    add_test(
            NAME test_apg_v2_cli_m7_export_board_timing_unconfigured
            COMMAND ${CMAKE_COMMAND} -E echo "APG_M7_BOARD_TIMING_COMMAND not configured; skipping board timing gate"
    )
    set_tests_properties(test_apg_v2_cli_m7_export_board_timing_unconfigured PROPERTIES
            LABELS v2
            PASS_REGULAR_EXPRESSION "skipping board timing gate"
    )
endif()
