foreach(variable PERL_EXECUTABLE GENERATOR SCHEMA SOURCE_LABEL EXPECTED_HEADER OUTPUT_DIR)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "${variable} is required")
    endif()
endforeach()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(first_output "${OUTPUT_DIR}/src_types.first.h")
set(second_output "${OUTPUT_DIR}/src_types.second.h")
set(generated_body_file "${OUTPUT_DIR}/src_types.body.h")

foreach(output_file IN ITEMS "${first_output}" "${second_output}")
    execute_process(
            COMMAND "${PERL_EXECUTABLE}" "${GENERATOR}" "${SCHEMA}" "${output_file}"
            RESULT_VARIABLE generate_result
            ERROR_VARIABLE generate_error
    )
    if(NOT generate_result EQUAL 0)
        message(FATAL_ERROR "DSP type generation failed: ${generate_error}")
    endif()
endforeach()

execute_process(
        COMMAND "${CMAKE_COMMAND}" -E compare_files "${first_output}" "${second_output}"
        RESULT_VARIABLE deterministic_result
)
if(NOT deterministic_result EQUAL 0)
    message(FATAL_ERROR "DSP type generation is not deterministic")
endif()

file(READ "${first_output}" generated)
string(FIND "${generated}" "\n" banner_end)
if(banner_end EQUAL -1)
    message(FATAL_ERROR "Generated DSP type candidate has no banner")
endif()
string(SUBSTRING "${generated}" 0 ${banner_end} actual_banner)
set(expected_banner "/* Generated candidate from ${SOURCE_LABEL}; do not edit this output. */")
if(NOT actual_banner STREQUAL expected_banner)
    message(FATAL_ERROR "Generated DSP type candidate banner is invalid: ${actual_banner}")
endif()

math(EXPR body_start "${banner_end} + 1")
string(SUBSTRING "${generated}" ${body_start} -1 generated_body)
file(WRITE "${generated_body_file}" "${generated_body}")
execute_process(
        COMMAND "${CMAKE_COMMAND}" -E compare_files "${EXPECTED_HEADER}" "${generated_body_file}"
        RESULT_VARIABLE equivalence_result
)
if(NOT equivalence_result EQUAL 0)
    message(FATAL_ERROR
            "Generated DSP type body differs from ${EXPECTED_HEADER}; inspect ${generated_body_file}"
    )
endif()
