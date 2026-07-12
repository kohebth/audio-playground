file(GLOB_RECURSE APG_CORE_FILES
        "${APG_ROOT}/inc/apgcore/*.h"
        "${APG_ROOT}/src/apgcore/*.c"
)

foreach(APG_CORE_FILE IN LISTS APG_CORE_FILES)
    file(READ "${APG_CORE_FILE}" APG_CORE_CONTENT)
    if(APG_CORE_CONTENT MATCHES "emscripten|AudioWorklet|WebAssembly|apg/wasm|wasm-tools")
        message(FATAL_ERROR "APGCore browser dependency found in ${APG_CORE_FILE}")
    endif()
endforeach()

message(STATUS "APGCore browser dependency boundary is clean")
