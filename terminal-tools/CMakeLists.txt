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

add_library(apg_terminal_model STATIC
        src/project_document.cpp
        src/session.cpp
)
target_include_directories(apg_terminal_model PUBLIC include)
target_link_libraries(apg_terminal_model PUBLIC yaml-cpp::yaml-cpp)
target_compile_features(apg_terminal_model PUBLIC cxx_std_20)

add_library(apg_terminal_ui STATIC
        src/pipeline_component.cpp
        src/pipeline_unit_card.cpp
        src/ui.cpp
        src/parameter_row.cpp
        src/parameter_panel.cpp
        src/unit_tray.cpp
)
target_include_directories(apg_terminal_ui PUBLIC include)
target_link_libraries(apg_terminal_ui PUBLIC ftxui::component ftxui::dom ftxui::screen)
target_compile_features(apg_terminal_ui PUBLIC cxx_std_20)

add_executable(apg-tui src/main.cpp)
target_link_libraries(apg-tui PRIVATE apg_terminal_model apg_terminal_ui)
target_compile_features(apg-tui PRIVATE cxx_std_20)

add_executable(test_terminal_model test/test_project_document.cpp)
target_link_libraries(test_terminal_model PRIVATE apg_terminal_model)
target_compile_features(test_terminal_model PRIVATE cxx_std_20)
add_test(NAME test_terminal_model COMMAND test_terminal_model)
set_tests_properties(test_terminal_model PROPERTIES LABELS "terminal-tools" WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_test(NAME test_terminal_cli COMMAND apg-tui --self-test)
set_tests_properties(test_terminal_cli PROPERTIES LABELS "terminal-tools")

add_executable(test_terminal_mouse test/test_mouse_interaction.cpp)
target_link_libraries(test_terminal_mouse PRIVATE apg_terminal_ui)
target_compile_features(test_terminal_mouse PRIVATE cxx_std_20)
add_test(NAME test_terminal_mouse COMMAND test_terminal_mouse)
set_tests_properties(test_terminal_mouse PROPERTIES LABELS "terminal-tools")

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
