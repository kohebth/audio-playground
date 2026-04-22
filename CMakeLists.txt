cmake_minimum_required(VERSION 3.15)
project(audio_playground)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -D_REENTRANT")

find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)

include_directories(
    ${CMAKE_SOURCE_DIR}/inc/
    ${CMAKE_SOURCE_DIR}/inc/util
    ${CMAKE_SOURCE_DIR}/inc/runtime
    ${CMAKE_SOURCE_DIR}/inc/unit
    ${CMAKE_SOURCE_DIR}/inc/util
    ${PIPEWIRE_INCLUDE_DIRS}
)

file(GLOB ATOM_SOURCES src/atom/*.c)
file(GLOB RUNTIME_SOURCES src/runtime/*.c)
file(GLOB UNIT_SOURCES src/unit/*.c)
file(GLOB UTIL_SOURCES src/util/*.c)
list(APPEND UTIL_SOURCES ${ATOM_SOURCES})
list(APPEND UTIL_SOURCES ${UNIT_SOURCES})
list(APPEND UTIL_SOURCES ${RUNTIME_SOURCES})

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
)
target_link_libraries(fast_math_test
    m
)