cmake_minimum_required(VERSION 3.15)
project(audio_playground)
enable_testing()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -D_REENTRANT")

find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)

include_directories(
        ${CMAKE_SOURCE_DIR}/inc/
        ${CMAKE_SOURCE_DIR}/inc/rte
        ${CMAKE_SOURCE_DIR}/inc/unit
        ${CMAKE_SOURCE_DIR}/inc/util
        ${CMAKE_SOURCE_DIR}/inc/yaml
        ${PIPEWIRE_INCLUDE_DIRS}
)

file(GLOB ATOM_SOURCES src/atom/*.c)
file(GLOB CTRL_SOURCES src/ctrl/*.c)
file(GLOB RUNT_SOURCES src/rte/*.c)
file(GLOB UNIT_SOURCES src/unit/*.c)
file(GLOB UTIL_SOURCES src/util/*.c)
file(GLOB YAML_SOURCES src/yaml/*.c)
list(APPEND UTIL_SOURCES ${ATOM_SOURCES})
list(APPEND UTIL_SOURCES ${CTRL_SOURCES})
list(APPEND UTIL_SOURCES ${UNIT_SOURCES})
list(APPEND UTIL_SOURCES ${RUNT_SOURCES})
list(APPEND UTIL_SOURCES ${YAML_SOURCES})

add_executable(live
        src/live.c
        ${UTIL_SOURCES}
)
target_link_libraries(live
        ${PIPEWIRE_LIBRARIES}
        m
)

add_executable(fast_math_test
        src/fast_math_test.c
        ${UTIL_SOURCES}
        inc/atom/dsp_types.h
)
target_link_libraries(fast_math_test
        m
)

add_executable(test_runtime
        src/test_runtime.c
        ${UTIL_SOURCES}
)
target_link_libraries(test_runtime
        ${PIPEWIRE_LIBRARIES}
        m
)

set(TEST_TARGETS
        test_atom_basic
        test_unit_load_all
        test_ctrl_transition
        test_offline_chain
)

foreach(TEST_TARGET ${TEST_TARGETS})
    add_executable(${TEST_TARGET}
            test/${TEST_TARGET}.c
            ${UTIL_SOURCES}
    )
    target_link_libraries(${TEST_TARGET}
            ${PIPEWIRE_LIBRARIES}
            m
    )
    add_test(NAME ${TEST_TARGET} COMMAND ${TEST_TARGET})
    set_tests_properties(${TEST_TARGET} PROPERTIES
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endforeach()
