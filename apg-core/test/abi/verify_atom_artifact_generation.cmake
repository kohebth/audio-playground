foreach(variable PERL_EXECUTABLE GENERATOR SCHEMA SOURCE_ROOT OUTPUT_DIR)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "${variable} is required")
    endif()
endforeach()

set(generated_files
        apg-core/inc/atom/types/dsp_type_macros.h
        apg-core/inc/atom/types/generation_types.h
        apg-core/inc/atom/types/amplitude_types.h
        apg-core/inc/atom/types/delay_types.h
        apg-core/inc/atom/types/filter_types.h
        apg-core/inc/atom/types/detect_types.h
        apg-core/inc/atom/types/modulation_types.h
        apg-core/inc/atom/types/interpolation_types.h
        apg-core/inc/atom/types/math_types.h
        apg-core/inc/atom/types/src_types.h
        apg-core/inc/atom/types/frequency_types.h
        apg-core/inc/atom/types/mix_types.h
        apg-core/inc/atom/types/nonlinear_types.h
        apg-core/inc/atom/generated/atom_definitions.generated.h
        apg-core/inc/atom/generated/dsp_atoms.generated.h
        apg-core/src/atom/generation/generation_field_descriptors.c
        apg-core/src/atom/amplitude/amplitude_field_descriptors.c
        apg-core/src/atom/delay/delay_field_descriptors.c
        apg-core/src/atom/filter/filter_field_descriptors.c
        apg-core/src/atom/detect/detect_field_descriptors.c
        apg-core/src/atom/modulation/modulation_field_descriptors.c
        apg-core/src/atom/interpolation/interpolation_field_descriptors.c
        apg-core/src/atom/math/math_field_descriptors.c
        apg-core/src/atom/source/source_field_descriptors.c
        apg-core/src/atom/frequency/frequency_field_descriptors.c
        apg-core/src/atom/mix/mix_field_descriptors.c
        apg-core/src/atom/nonlinear/nonlinear_field_descriptors.c
        apg-core/src/apgcore/metadata/atom_catalog_contracts.generated.inc
        apg-web/src/atoms/atomCatalog.generated.ts
        schema/atoms/atom.schema.json
)

set(first_root "${OUTPUT_DIR}/first")
set(second_root "${OUTPUT_DIR}/second")
file(MAKE_DIRECTORY "${first_root}" "${second_root}")

foreach(root IN ITEMS "${first_root}" "${second_root}")
    execute_process(
            COMMAND "${PERL_EXECUTABLE}" "${GENERATOR}" "${SCHEMA}" "${root}"
            RESULT_VARIABLE generate_result
            ERROR_VARIABLE generate_error
    )
    if(NOT generate_result EQUAL 0)
        message(FATAL_ERROR "Atom artifact generation failed: ${generate_error}")
    endif()
endforeach()

foreach(relative IN LISTS generated_files)
    execute_process(
            COMMAND "${CMAKE_COMMAND}" -E compare_files
                    "${first_root}/${relative}"
                    "${second_root}/${relative}"
            RESULT_VARIABLE deterministic_result
    )
    if(NOT deterministic_result EQUAL 0)
        message(FATAL_ERROR "Atom artifact generation is not deterministic for ${relative}")
    endif()
endforeach()

execute_process(
        COMMAND "${PERL_EXECUTABLE}" "${GENERATOR}" --check "${SCHEMA}" "${SOURCE_ROOT}"
        RESULT_VARIABLE source_check_result
        ERROR_VARIABLE source_check_error
)
if(NOT source_check_result EQUAL 0)
    message(FATAL_ERROR "Checked-in atom artifacts are stale: ${source_check_error}")
endif()

set(stale_file "${first_root}/apg-core/inc/atom/generated/atom_definitions.generated.h")
file(APPEND "${stale_file}" "\n/* stale marker */\n")
execute_process(
        COMMAND "${PERL_EXECUTABLE}" "${GENERATOR}" --check "${SCHEMA}" "${first_root}"
        RESULT_VARIABLE stale_check_result
        ERROR_VARIABLE stale_check_error
)
if(stale_check_result EQUAL 0)
    message(FATAL_ERROR "Atom artifact stale check accepted modified output")
endif()
string(FIND "${stale_check_error}" "apg-core/inc/atom/generated/atom_definitions.generated.h" stale_path_position)
if(stale_path_position EQUAL -1)
    message(FATAL_ERROR "Atom artifact stale check did not identify the modified output: ${stale_check_error}")
endif()

file(READ "${SCHEMA}" schema_text)

string(REPLACE
        "\"generation_dc.value\""
        "\"generation_dc.unknown\""
        missing_metadata_schema
        "${schema_text}"
)
if(missing_metadata_schema STREQUAL schema_text)
    message(FATAL_ERROR "Failed to construct missing-metadata atom schema fixture")
endif()
set(missing_metadata_path "${OUTPUT_DIR}/missing-metadata.json")
file(WRITE "${missing_metadata_path}" "${missing_metadata_schema}")
execute_process(
        COMMAND "${PERL_EXECUTABLE}" "${GENERATOR}" "${missing_metadata_path}" "${OUTPUT_DIR}/invalid-metadata"
        RESULT_VARIABLE missing_metadata_result
        ERROR_VARIABLE missing_metadata_error
)
if(missing_metadata_result EQUAL 0 OR NOT missing_metadata_error MATCHES "generation_dc.value has no parameter metadata")
    message(FATAL_ERROR "Generator accepted missing parameter metadata: ${missing_metadata_error}")
endif()

string(REPLACE
        "         \"generation_dc\",\n         \"generation_envelope\","
        "         \"generation_envelope\","
        missing_visibility_schema
        "${schema_text}"
)
if(missing_visibility_schema STREQUAL schema_text)
    message(FATAL_ERROR "Failed to construct missing-visibility atom schema fixture")
endif()
set(missing_visibility_path "${OUTPUT_DIR}/missing-visibility.json")
file(WRITE "${missing_visibility_path}" "${missing_visibility_schema}")
execute_process(
        COMMAND "${PERL_EXECUTABLE}" "${GENERATOR}" "${missing_visibility_path}" "${OUTPUT_DIR}/invalid-visibility"
        RESULT_VARIABLE missing_visibility_result
        ERROR_VARIABLE missing_visibility_error
)
if(missing_visibility_result EQUAL 0 OR NOT missing_visibility_error MATCHES "generation_dc has no visibility")
    message(FATAL_ERROR "Generator accepted an incomplete visibility partition: ${missing_visibility_error}")
endif()
