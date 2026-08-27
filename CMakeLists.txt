cmake_minimum_required(VERSION 3.31)

project(Vulpes VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CTest)
include(FetchContent)

option(VULPES_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

find_program(VULPES_CLANG_FORMAT NAMES clang-format clang-format-22)
find_program(VULPES_CLANG_TIDY NAMES clang-tidy clang-tidy-22
    HINTS "$ENV{VSINSTALLDIR}/VC/Tools/Llvm/x64/bin" "$ENV{VCToolsInstallDir}/../../Llvm/x64/bin")

find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(utf8proc CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# CPP-Terminal owns host-specific raw-mode, keyboard, and resize handling.
# Keep it behind Vulpes' Terminal interface so widgets and the runtime remain
# independent of a particular terminal library.
set(CPPTERMINAL_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CPPTERMINAL_ENABLE_DOCS OFF CACHE BOOL "" FORCE)
set(CPPTERMINAL_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(CPPTERMINAL_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    cpp_terminal
    GIT_REPOSITORY https://github.com/jupyter-xeus/cpp-terminal.git
    GIT_TAG 945f4931ff3974850401693d13ad8bd9bbd0619e
)
FetchContent_MakeAvailable(cpp_terminal)

add_library(vulpes_options INTERFACE)
target_compile_features(vulpes_options INTERFACE cxx_std_23)

if(MSVC)
    target_compile_options(vulpes_options INTERFACE /W4 /permissive- /utf-8 /Zc:preprocessor)
    if(VULPES_WARNINGS_AS_ERRORS)
        target_compile_options(vulpes_options INTERFACE /WX)
    endif()
else()
    target_compile_options(vulpes_options INTERFACE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    if(VULPES_WARNINGS_AS_ERRORS)
        target_compile_options(vulpes_options INTERFACE -Werror)
    endif()
endif()

add_library(vulpes_core
    src/core/actions.cpp
    src/core/application.cpp
    src/core/browse_controller.cpp
    src/core/command.cpp
    src/core/localization.cpp
    src/db/database.cpp
    src/db/identifier.cpp
    src/db/row.cpp
    src/db/schema.cpp
    src/db/statement.cpp
    src/db/transaction.cpp
    src/db/value.cpp
    src/model/dataset.cpp
    src/terminal/ansi_input.cpp
    src/terminal/ansi_encoder.cpp
    src/terminal/cpp_terminal_adapter.cpp
    src/terminal/console_terminal.cpp
    src/terminal/frame_diff.cpp
    src/terminal/screen_buffer.cpp
    src/terminal/test_terminal.cpp
    src/terminal/unicode.cpp
    src/ui/confirmation_dialog.cpp
    src/ui/form.cpp
    src/ui/grid.cpp
    src/ui/sql_console.cpp
    src/ui/text_prompt.cpp
    src/ui/workspace.cpp
    src/ui/window_manager.cpp
)
add_library(Vulpes::core ALIAS vulpes_core)

target_include_directories(vulpes_core
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(vulpes_core
    PUBLIC vulpes_options
    PRIVATE unofficial::sqlite3::sqlite3 utf8proc::utf8proc nlohmann_json::nlohmann_json cpp-terminal::cpp-terminal)
target_compile_definitions(vulpes_core PRIVATE VULPES_BUILDING_LIBRARY)

configure_file(cmake/version.hpp.in generated/vulpes/version.hpp @ONLY)
target_include_directories(vulpes_core PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>)

add_executable(vulpes src/main.cpp)
target_link_libraries(vulpes PRIVATE Vulpes::core CLI11::CLI11)

if(BUILD_TESTING)
    add_test(NAME vulpes_version COMMAND vulpes --version)
endif()

install(TARGETS vulpes RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY translations DESTINATION ${CMAKE_INSTALL_DATADIR}/vulpes)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(VULPES_CLANG_FORMAT)
    file(GLOB_RECURSE VULPES_FORMAT_FILES CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/include/*.hpp"
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/tests/*.cpp")
    add_custom_target(format
        COMMAND ${VULPES_CLANG_FORMAT} -i --style=file ${VULPES_FORMAT_FILES}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Formatting Vulpes C++ sources")
    add_custom_target(format-check
        COMMAND ${VULPES_CLANG_FORMAT} --dry-run --Werror --style=file ${VULPES_FORMAT_FILES}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Checking Vulpes C++ formatting")
else()
    message(STATUS "clang-format not found; format targets are unavailable")
endif()

if(VULPES_CLANG_TIDY)
    file(GLOB_RECURSE VULPES_TIDY_FILES CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/tests/*.cpp")
    add_custom_target(tidy
        COMMAND ${VULPES_CLANG_TIDY} -p ${CMAKE_BINARY_DIR} --config-file=${PROJECT_SOURCE_DIR}/.clang-tidy ${VULPES_TIDY_FILES}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running Vulpes clang-tidy checks")
else()
    message(STATUS "clang-tidy not found; tidy target is unavailable")
endif()
