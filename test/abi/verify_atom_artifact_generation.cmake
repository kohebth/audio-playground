foreach(variable PERL_EXECUTABLE GENERATOR SCHEMA SOURCE_ROOT OUTPUT_DIR)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "${variable} is required")
    endif()
endforeach()

set(generated_files
        inc/atom/types/dsp_type_macros.h
        inc/atom/types/generation_types.h
        inc/atom/types/amplitude_types.h
        inc/atom/types/delay_types.h
        inc/atom/types/filter_types.h
        inc/atom/types/detect_types.h
        inc/atom/types/modulation_types.h
        inc/atom/types/interpolation_types.h
        inc/atom/types/src_types.h
        inc/atom/types/frequency_types.h
        inc/atom/types/mix_types.h
        inc/atom/types/nonlinear_types.h
        inc/atom/generated/atom_definitions.generated.h
        inc/atom/generated/dsp_atoms.generated.h
        src/atom/generation/generation_field_descriptors.c
        src/atom/amplitude/amplitude_field_descriptors.c
        src/atom/delay/delay_field_descriptors.c
        src/atom/filter/filter_field_descriptors.c
        src/atom/detect/detect_field_descriptors.c
        src/atom/modulation/modulation_field_descriptors.c
        src/atom/interpolation/interpolation_field_descriptors.c
        src/atom/source/source_field_descriptors.c
        src/atom/frequency/frequency_field_descriptors.c
        src/atom/mix/mix_field_descriptors.c
        src/atom/nonlinear/nonlinear_field_descriptors.c
        src/apgcore/metadata/atom_catalog_contracts.generated.inc
        web-tools/unit-editor/src/atoms/atomCatalog.generated.ts
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

set(stale_file "${first_root}/inc/atom/generated/atom_definitions.generated.h")
file(APPEND "${stale_file}" "\n/* stale marker */\n")
execute_process(
        COMMAND "${PERL_EXECUTABLE}" "${GENERATOR}" --check "${SCHEMA}" "${first_root}"
        RESULT_VARIABLE stale_check_result
        ERROR_VARIABLE stale_check_error
)
if(stale_check_result EQUAL 0)
    message(FATAL_ERROR "Atom artifact stale check accepted modified output")
endif()
string(FIND "${stale_check_error}" "inc/atom/generated/atom_definitions.generated.h" stale_path_position)
if(stale_path_position EQUAL -1)
    message(FATAL_ERROR "Atom artifact stale check did not identify the modified output: ${stale_check_error}")
endif()
