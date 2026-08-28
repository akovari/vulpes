find_package(Catch2 3 CONFIG REQUIRED)

add_executable(vulpes_tests
    core/application_test.cpp
    core/actions_test.cpp
    core/browse_controller_test.cpp
    core/command_test.cpp
    core/localization_test.cpp
    db/database_test.cpp
    db/schema_test.cpp
    examples/inventory_test.cpp
    model/dataset_test.cpp
    ui/button_test.cpp
    ui/form_test.cpp
    ui/confirmation_dialog_test.cpp
    ui/document_session_test.cpp
    ui/focus_ring_test.cpp
    ui/workspace_document_test.cpp
    ui/grid_test.cpp
    ui/schema_document_test.cpp
    ui/sql_console_test.cpp
    ui/status_bar_test.cpp
    ui/terminal_warning_test.cpp
    ui/text_prompt_test.cpp
    ui/theme_test.cpp
    ui/workspace_test.cpp
    ui/workspace_text_test.cpp
    ui/window_manager_test.cpp
    ui/window_frame_test.cpp
    terminal/frame_diff_test.cpp
    terminal/ansi_input_test.cpp
    terminal/cpp_terminal_adapter_test.cpp
    terminal/screen_buffer_test.cpp
    terminal/test_terminal_test.cpp
)
target_link_libraries(vulpes_tests PRIVATE Vulpes::core Catch2::Catch2WithMain)
target_compile_definitions(vulpes_tests PRIVATE VULPES_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

include(Catch)
catch_discover_tests(vulpes_tests)
