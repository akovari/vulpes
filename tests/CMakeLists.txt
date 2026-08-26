find_package(Catch2 3 CONFIG REQUIRED)

add_executable(vulpes_tests
    db/database_test.cpp
    db/schema_test.cpp
    model/dataset_test.cpp
    ui/grid_test.cpp
    terminal/frame_diff_test.cpp
    terminal/screen_buffer_test.cpp
    terminal/test_terminal_test.cpp
)
target_link_libraries(vulpes_tests PRIVATE Vulpes::core Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(vulpes_tests)
