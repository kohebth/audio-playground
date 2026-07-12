add_library(apg_wasm_tools STATIC
        src/control.c
        src/image.c
        src/processor.c
)
target_include_directories(apg_wasm_tools PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(apg_wasm_tools PUBLIC apgcore)

add_executable(test_wasm_tools_abi test/test_abi.c)
target_link_libraries(test_wasm_tools_abi apg_wasm_tools)
add_test(NAME test_wasm_tools_abi COMMAND test_wasm_tools_abi)
set_tests_properties(test_wasm_tools_abi PROPERTIES LABELS "v2;wasm-tools")

add_executable(test_wasm_tools_workspace test/test_workspace.c)
target_link_libraries(test_wasm_tools_workspace apg_wasm_tools)
target_compile_definitions(test_wasm_tools_workspace PRIVATE APG_TEST_ROOT="${CMAKE_SOURCE_DIR}")
add_test(NAME test_wasm_tools_workspace COMMAND test_wasm_tools_workspace)
set_tests_properties(test_wasm_tools_workspace PROPERTIES LABELS "v2;wasm-tools")

add_executable(test_wasm_tools_processor test/test_processor.c)
target_link_libraries(test_wasm_tools_processor apg_wasm_tools)
target_compile_definitions(test_wasm_tools_processor PRIVATE APG_TEST_ROOT="${CMAKE_SOURCE_DIR}")
add_test(NAME test_wasm_tools_processor COMMAND test_wasm_tools_processor)
set_tests_properties(test_wasm_tools_processor PROPERTIES LABELS "v2;wasm-tools")

add_test(
        NAME test_wasm_tools_core_boundary
        COMMAND ${CMAKE_COMMAND} -DAPG_ROOT=${CMAKE_SOURCE_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/test/check_core_boundary.cmake
)
set_tests_properties(test_wasm_tools_core_boundary PROPERTIES LABELS "v2;wasm-tools")

if(EMSCRIPTEN)
    add_executable(apg_control src/control.c src/image.c)
    target_include_directories(apg_control PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(apg_control PRIVATE apgcore)
    target_link_options(apg_control PRIVATE
            "--no-entry"
            "-sEXPORTED_FUNCTIONS=['_malloc','_free','_apg_wasm_control_abi_version','_apg_wasm_control_capabilities','_apg_wasm_control_create','_apg_wasm_control_destroy','_apg_wasm_control_begin_workspace','_apg_wasm_control_put_file','_apg_wasm_control_validate_workspace','_apg_wasm_control_compile_workspace','_apg_wasm_control_prepare_workspace','_apg_wasm_control_prepared_image','_apg_wasm_control_last_diagnostic','_apg_wasm_control_workspace_summary']"
            "-sFILESYSTEM=0"
            "-sMODULARIZE=1"
            "-sEXPORT_ES6=1"
            "-sEXPORT_NAME=createApgControlModule"
    )
    set_target_properties(apg_control PROPERTIES SUFFIX ".mjs")

    add_executable(apg_processor src/processor.c src/image.c)
    target_include_directories(apg_processor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(apg_processor PRIVATE apgcore)
    target_link_options(apg_processor PRIVATE
            "--no-entry"
            "-sEXPORTED_FUNCTIONS=['_malloc','_free','_apg_wasm_processor_abi_version','_apg_wasm_processor_capabilities','_apg_wasm_processor_create','_apg_wasm_processor_destroy','_apg_wasm_processor_stage_image','_apg_wasm_processor_commit_staged','_apg_wasm_processor_input_buffer','_apg_wasm_processor_output_buffer','_apg_wasm_processor_frame_capacity','_apg_wasm_processor_active_revision','_apg_wasm_processor_process','_apg_wasm_processor_set_param','_apg_wasm_processor_set_bypass','_apg_wasm_processor_set_mute','_apg_wasm_processor_reset','_apg_wasm_processor_output_meter','_apg_wasm_processor_last_diagnostic']"
            "-sFILESYSTEM=0"
            "-sMODULARIZE=1"
            "-sEXPORT_ES6=1"
            "-sEXPORT_NAME=createApgProcessorModule"
    )
    set_target_properties(apg_processor PROPERTIES SUFFIX ".mjs")
endif()
