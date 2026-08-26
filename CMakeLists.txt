cmake_minimum_required(VERSION 3.31)

project(Vulpes VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CTest)

option(VULPES_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(utf8proc CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)

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
    src/terminal/ansi_encoder.cpp
    src/terminal/frame_diff.cpp
    src/terminal/screen_buffer.cpp
    src/terminal/test_terminal.cpp
    src/terminal/unicode.cpp
    src/ui/grid.cpp
)
add_library(Vulpes::core ALIAS vulpes_core)

target_include_directories(vulpes_core
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(vulpes_core PUBLIC vulpes_options PRIVATE unofficial::sqlite3::sqlite3 utf8proc::utf8proc)
target_compile_definitions(vulpes_core PRIVATE VULPES_BUILDING_LIBRARY)

configure_file(cmake/version.hpp.in generated/vulpes/version.hpp @ONLY)
target_include_directories(vulpes_core PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>)

add_executable(vulpes src/main.cpp)
target_link_libraries(vulpes PRIVATE Vulpes::core CLI11::CLI11)

if(BUILD_TESTING)
    add_test(NAME vulpes_version COMMAND vulpes --version)
endif()

install(TARGETS vulpes RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
