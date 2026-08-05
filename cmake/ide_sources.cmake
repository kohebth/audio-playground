# IDE source registration helper.
#
# Registers sources under the given directory trees as an IDE index target so
# a single root CMake configure exposes every component (apg-core, apg-wasm,
# apg-tui, apg-web) to IDEs that consume the CMake file API (CLion, VS Code,
# Qt Creator). INTERFACE libraries are invisible to the file API, so this
# creates a real STATIC library target that lists the matched files as
# sources and marks them HEADER_FILE_ONLY, which keeps them in the target's
# source list (and therefore in the IDE project) without compiling them. A
# tiny generated stub keeps the target buildable.
#
# Do not pass files that are compiled by another target in the same
# directory: source-file properties are directory-scoped, so HEADER_FILE_ONLY
# would suppress their compilation there too.
#
# Usage:
#   apg_ide_sources(<target_name> <glob>...)
function(apg_ide_sources TARGET_NAME)
    set(_globs ${ARGN})
    if(NOT _globs)
        message(FATAL_ERROR "apg_ide_sources: no source globs provided for ${TARGET_NAME}")
    endif()

    set(_sources "")
    foreach(_glob IN LISTS _globs)
        file(GLOB_RECURSE _matched CONFIGURE_DEPENDS "${_glob}")
        list(APPEND _sources ${_matched})
    endforeach()
    list(FILTER _sources EXCLUDE REGEX "/(node_modules|dist|build|\\.git|public/wasm)/|/CMakeLists\\.txt$")
    list(REMOVE_DUPLICATES _sources)

    set(_stub "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}_stub.c")
    file(WRITE "${_stub}" "int apg_ide_sources_${TARGET_NAME}_stub(void) { return 0; }\n")

    add_library(${TARGET_NAME} STATIC "${_stub}" ${_sources})
    set_source_files_properties(${_sources} PROPERTIES HEADER_FILE_ONLY TRUE)
endfunction()
