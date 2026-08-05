file(READ "${PROCESSOR_SOURCE}" PROCESSOR_CONTENT)

string(FIND "${PROCESSOR_CONTENT}" "apg_wasm_processor_process(apg_wasm_processor_t *processor" PROCESS_START)
string(FIND "${PROCESSOR_CONTENT}" "apg_wasm_processor_set_param" PROCESS_END)
if(PROCESS_START EQUAL -1 OR PROCESS_END EQUAL -1 OR PROCESS_END LESS PROCESS_START)
    message(FATAL_ERROR "Could not isolate apg_wasm_processor_process")
endif()

math(EXPR PROCESS_LENGTH "${PROCESS_END} - ${PROCESS_START}")
string(SUBSTRING "${PROCESSOR_CONTENT}" ${PROCESS_START} ${PROCESS_LENGTH} PROCESS_BODY)

string(FIND "${PROCESSOR_CONTENT}" "runtime_failure(apg_wasm_processor_t *processor" FAILURE_START)
string(FIND "${PROCESSOR_CONTENT}" "uint32_t apg_wasm_processor_abi_version" FAILURE_END)
if(FAILURE_START EQUAL -1 OR FAILURE_END EQUAL -1 OR FAILURE_END LESS FAILURE_START)
    message(FATAL_ERROR "Could not isolate the real-time diagnostic helper")
endif()
math(EXPR FAILURE_LENGTH "${FAILURE_END} - ${FAILURE_START}")
string(SUBSTRING "${PROCESSOR_CONTENT}" ${FAILURE_START} ${FAILURE_LENGTH} FAILURE_BODY)

set(FORBIDDEN_REALTIME_CALLS
        malloc
        calloc
        realloc
        free
        slot_destroy
        snprintf
        strlen
        strcmp
        apg_wasm_image_hydrate
        apg_v2_measure_get
        parse
        compile
)

foreach(REALTIME_BODY IN ITEMS PROCESS_BODY FAILURE_BODY)
    foreach(FORBIDDEN_CALL IN LISTS FORBIDDEN_REALTIME_CALLS)
        if("${${REALTIME_BODY}}" MATCHES "${FORBIDDEN_CALL}[	 ]*\\(")
            message(FATAL_ERROR "Real-time processor calls forbidden operation: ${FORBIDDEN_CALL}")
        endif()
    endforeach()
endforeach()

message(STATUS "WASM processor real-time boundary is clean")
