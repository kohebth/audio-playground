include(FetchContent)

FetchContent_Declare(
        ftxui
        GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui.git
        GIT_TAG v7.0.1
)
FetchContent_MakeAvailable(ftxui)

FetchContent_Declare(
        yaml_cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG yaml-cpp-0.9.0
)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml_cpp)

set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
        miniaudio_source
        GIT_REPOSITORY https://github.com/mackron/miniaudio.git
        GIT_TAG 0.11.25
        GIT_SHALLOW TRUE
)
FetchContent_GetProperties(miniaudio_source)
if(NOT miniaudio_source_POPULATED)
    FetchContent_Populate(miniaudio_source)
endif()

add_library(apg_terminal_model STATIC
        src/project_document.cpp
        src/editor.cpp
)
target_include_directories(apg_terminal_model PUBLIC include)
target_link_libraries(apg_terminal_model PUBLIC apgcore yaml-cpp::yaml-cpp nlohmann_json::nlohmann_json)
target_compile_features(apg_terminal_model PUBLIC cxx_std_20)

find_package(Threads REQUIRED)
add_library(apg_terminal_audio STATIC
        src/session.cpp
        src/miniaudio_impl.cpp
)
target_include_directories(apg_terminal_audio PUBLIC include PRIVATE ${miniaudio_source_SOURCE_DIR})
target_link_libraries(apg_terminal_audio PUBLIC apg_terminal_model apgcore Threads::Threads ${CMAKE_DL_LIBS})
target_compile_features(apg_terminal_audio PUBLIC cxx_std_20)

add_library(apg_terminal_ui STATIC
        src/pipeline_component.cpp
        src/pipeline_unit_card.cpp
        src/ui.cpp
        src/studio.cpp
        src/parameter_row.cpp
        src/parameter_panel.cpp
        src/unit_tray.cpp
)
target_include_directories(apg_terminal_ui PUBLIC include)
target_link_libraries(
        apg_terminal_ui
        PUBLIC
            apg_terminal_model
            apg_terminal_audio
            ftxui::component
            ftxui::dom
            ftxui::screen
)
target_compile_features(apg_terminal_ui PUBLIC cxx_std_20)

add_executable(apg-tui src/main.cpp)
target_link_libraries(apg-tui PRIVATE apg_terminal_model apg_terminal_audio apg_terminal_ui)
target_compile_features(apg-tui PRIVATE cxx_std_20)

add_executable(test_terminal_model test/test_project_document.cpp)
target_link_libraries(test_terminal_model PRIVATE apg_terminal_model)
target_compile_features(test_terminal_model PRIVATE cxx_std_20)
add_test(NAME test_terminal_model COMMAND test_terminal_model)
set_tests_properties(test_terminal_model PROPERTIES LABELS "terminal-tools" WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(test_terminal_audio test/test_audio_session.cpp)
target_link_libraries(test_terminal_audio PRIVATE apg_terminal_audio)
target_compile_features(test_terminal_audio PRIVATE cxx_std_20)
add_test(NAME test_terminal_audio COMMAND test_terminal_audio)
set_tests_properties(
    test_terminal_audio
    PROPERTIES
        LABELS "terminal-tools"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_test(
    NAME test_terminal_audio_realtime_boundary
    COMMAND
        ${CMAKE_COMMAND}
        -DAUDIO_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/session.cpp
        -P
        ${CMAKE_CURRENT_SOURCE_DIR}/test/check_audio_realtime.cmake
)
set_tests_properties(
    test_terminal_audio_realtime_boundary
    PROPERTIES LABELS "terminal-tools"
)

add_test(NAME test_terminal_cli COMMAND apg-tui --self-test)
set_tests_properties(test_terminal_cli PROPERTIES LABELS "terminal-tools")

add_test(NAME test_terminal_cli_version COMMAND apg-tui --version)
set_tests_properties(
    test_terminal_cli_version
    PROPERTIES
        LABELS "terminal-tools"
        PASS_REGULAR_EXPRESSION "apg-tui 0.2.0"
)

add_test(NAME test_terminal_cli_requires_package COMMAND apg-tui)
set_tests_properties(
    test_terminal_cli_requires_package
    PROPERTIES
        LABELS "terminal-tools"
        WILL_FAIL TRUE
)

add_executable(test_terminal_mouse test/test_mouse_interaction.cpp)
target_link_libraries(test_terminal_mouse PRIVATE apg_terminal_ui)
target_compile_features(test_terminal_mouse PRIVATE cxx_std_20)
add_test(NAME test_terminal_mouse COMMAND test_terminal_mouse)
set_tests_properties(test_terminal_mouse PROPERTIES LABELS "terminal-tools")

add_executable(test_terminal_studio test/test_studio_layout.cpp)
target_link_libraries(test_terminal_studio PRIVATE apg_terminal_ui)
target_compile_features(test_terminal_studio PRIVATE cxx_std_20)
add_test(NAME test_terminal_studio COMMAND test_terminal_studio)
set_tests_properties(
    test_terminal_studio
    PROPERTIES
        LABELS "terminal-tools"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(test_terminal_card_shape test/test_pipeline_card_shape.cpp)
target_link_libraries(test_terminal_card_shape PRIVATE apg_terminal_model apg_terminal_ui)
target_compile_features(test_terminal_card_shape PRIVATE cxx_std_20)
add_test(NAME test_terminal_card_shape COMMAND test_terminal_card_shape)
set_tests_properties(
    test_terminal_card_shape
    PROPERTIES
        LABELS "terminal-tools"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
