find_package(Catch2 3 CONFIG REQUIRED)

add_executable(vulpes_tests
    core/application_test.cpp
    core/browse_controller_test.cpp
    core/command_test.cpp
    core/localization_test.cpp
    db/database_test.cpp
    db/schema_test.cpp
    examples/inventory_test.cpp
    model/dataset_test.cpp
    ui/grid_test.cpp
    terminal/frame_diff_test.cpp
    terminal/screen_buffer_test.cpp
    terminal/test_terminal_test.cpp
)
target_link_libraries(vulpes_tests PRIVATE Vulpes::core Catch2::Catch2WithMain)
target_compile_definitions(vulpes_tests PRIVATE VULPES_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

include(Catch)
catch_discover_tests(vulpes_tests)
