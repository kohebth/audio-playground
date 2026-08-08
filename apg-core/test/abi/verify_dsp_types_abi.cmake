foreach(variable SNAPSHOT_EXE EXPECTED_FILE ACTUAL_FILE)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "${variable} is required")
    endif()
endforeach()

get_filename_component(actual_dir "${ACTUAL_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${actual_dir}")

execute_process(
        COMMAND "${SNAPSHOT_EXE}" "${ACTUAL_FILE}"
        RESULT_VARIABLE snapshot_result
)
if(NOT snapshot_result EQUAL 0)
    message(FATAL_ERROR "DSP ABI snapshot failed with exit code ${snapshot_result}")
endif()

execute_process(
        COMMAND "${CMAKE_COMMAND}" -E compare_files "${EXPECTED_FILE}" "${ACTUAL_FILE}"
        RESULT_VARIABLE compare_result
)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "DSP ABI differs: expected ${EXPECTED_FILE}, actual ${ACTUAL_FILE}")
endif()

if(DEFINED BASELINE_FILE)
    if(NOT DEFINED EMPTY_EXPECTED_FILE)
        set(EMPTY_EXPECTED_FILE "${EXPECTED_FILE}")
    endif()
    file(STRINGS "${BASELINE_FILE}" baseline_lines)
    file(STRINGS "${EMPTY_EXPECTED_FILE}" current_lines)
    set(transition_count 0)

    foreach(line IN LISTS baseline_lines)
        list(FIND current_lines "${line}" current_index)
        if(NOT current_index EQUAL -1)
            continue()
        endif()

        if(NOT line MATCHES "^type,([^,]+),0,1,,,$")
            message(FATAL_ERROR "Unexpected baseline ABI removal: ${line}")
        endif()

        set(type_name "${CMAKE_MATCH_1}")
        set(current_type "type,${type_name},1,1,,,")
        set(reserved_field "field,${type_name},,,_reserved,0,")
        list(FIND current_lines "${current_type}" current_type_index)
        list(FIND current_lines "${reserved_field}" reserved_field_index)
        if(current_type_index EQUAL -1 OR reserved_field_index EQUAL -1)
            message(FATAL_ERROR "Empty DSP type did not become a one-byte reserved layout: ${type_name}")
        endif()
        math(EXPR transition_count "${transition_count} + 1")
    endforeach()

    foreach(line IN LISTS current_lines)
        list(FIND baseline_lines "${line}" baseline_index)
        if(NOT baseline_index EQUAL -1)
            continue()
        endif()

        if(line MATCHES "^type,([^,]+),1,1,,,$")
            set(type_name "${CMAKE_MATCH_1}")
        elseif(line MATCHES "^field,([^,]+),,,_reserved,0,$")
            set(type_name "${CMAKE_MATCH_1}")
        else()
            message(FATAL_ERROR "Unexpected C11 ABI addition: ${line}")
        endif()

        list(FIND baseline_lines "type,${type_name},0,1,,," empty_baseline_index)
        if(NOT empty_baseline_index EQUAL -1)
            continue()
        endif()
        message(FATAL_ERROR "Unexpected C11 ABI addition: ${line}")
    endforeach()

    if(DEFINED EXPECTED_EMPTY_TRANSITIONS AND NOT transition_count EQUAL EXPECTED_EMPTY_TRANSITIONS)
        message(FATAL_ERROR
                "Expected ${EXPECTED_EMPTY_TRANSITIONS} empty DSP ABI transitions, found ${transition_count}"
        )
    endif()
endif()
