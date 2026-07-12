add_library(apg_wasm_tools STATIC
        src/control.c
        src/processor.c
)
target_include_directories(apg_wasm_tools PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(apg_wasm_tools PUBLIC apgcore)

add_executable(test_wasm_tools_abi test/test_abi.c)
target_link_libraries(test_wasm_tools_abi apg_wasm_tools)
add_test(NAME test_wasm_tools_abi COMMAND test_wasm_tools_abi)
set_tests_properties(test_wasm_tools_abi PROPERTIES LABELS "v2;wasm-tools")

add_test(
        NAME test_wasm_tools_core_boundary
        COMMAND ${CMAKE_COMMAND} -DAPG_ROOT=${CMAKE_SOURCE_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/test/check_core_boundary.cmake
)
set_tests_properties(test_wasm_tools_core_boundary PROPERTIES LABELS "v2;wasm-tools")

if(EMSCRIPTEN)
    add_executable(apg_control src/control.c)
    target_include_directories(apg_control PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(apg_control PRIVATE apgcore)
    target_link_options(apg_control PRIVATE
            "--no-entry"
            "-sEXPORTED_FUNCTIONS=['_apg_wasm_control_abi_version','_apg_wasm_control_capabilities']"
            "-sFILESYSTEM=0"
            "-sMODULARIZE=1"
            "-sEXPORT_ES6=1"
            "-sEXPORT_NAME=createApgControlModule"
    )
    set_target_properties(apg_control PROPERTIES SUFFIX ".mjs")

    add_executable(apg_processor src/processor.c)
    target_include_directories(apg_processor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(apg_processor PRIVATE apgcore)
    target_link_options(apg_processor PRIVATE
            "--no-entry"
            "-sEXPORTED_FUNCTIONS=['_apg_wasm_processor_abi_version','_apg_wasm_processor_capabilities']"
            "-sFILESYSTEM=0"
            "-sMODULARIZE=1"
            "-sEXPORT_ES6=1"
            "-sEXPORT_NAME=createApgProcessorModule"
    )
    set_target_properties(apg_processor PROPERTIES SUFFIX ".mjs")
endif()
