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
