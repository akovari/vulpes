cmake_minimum_required(VERSION 3.31)

project(Vulpes VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CTest)
include(FetchContent)
include(cmake/VulpesVersion.cmake)

option(VULPES_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
set(VULPES_BUILD_VERSION_OVERRIDE "" CACHE STRING "Build version for source archives without Git metadata")

find_package(Git QUIET)
vulpes_detect_version("${PROJECT_SOURCE_DIR}" "${PROJECT_VERSION}" VULPES_BUILD_VERSION VULPES_GIT_COMMIT
    VULPES_GIT_DIRTY VULPES_GIT_DESCRIPTION)
vulpes_watch_git("${PROJECT_SOURCE_DIR}")
if(VULPES_GIT_DIRTY)
    set(VULPES_GIT_DIRTY_CPP true)
else()
    set(VULPES_GIT_DIRTY_CPP false)
endif()
message(STATUS "Vulpes build version: ${VULPES_BUILD_VERSION}")

find_program(VULPES_CLANG_FORMAT NAMES clang-format clang-format-22)
find_program(VULPES_CLANG_TIDY NAMES clang-tidy clang-tidy-22
    HINTS "$ENV{VSINSTALLDIR}/VC/Tools/Llvm/x64/bin" "$ENV{VCToolsInstallDir}/../../Llvm/x64/bin")

find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(utf8proc CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(clip CONFIG REQUIRED)
find_package(ICU 78.3 REQUIRED COMPONENTS data i18n uc)
find_package(ZLIB REQUIRED)

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

# PDFio is a small C PDF writer without an upstream CMake project. Fetch the
# reviewed release commit, then expose only a private target to Vulpes.
FetchContent_Declare(
    pdfio_source
    GIT_REPOSITORY https://github.com/michaelrsweet/pdfio.git
    GIT_TAG 48b185c650e2e732eed260d63909f4ec68ac9353
    SOURCE_SUBDIR cmake-not-provided
)
FetchContent_MakeAvailable(pdfio_source)
add_library(vulpes_pdfio STATIC
    ${pdfio_source_SOURCE_DIR}/pdfio-aes.c
    ${pdfio_source_SOURCE_DIR}/pdfio-array.c
    ${pdfio_source_SOURCE_DIR}/pdfio-common.c
    ${pdfio_source_SOURCE_DIR}/pdfio-content.c
    ${pdfio_source_SOURCE_DIR}/pdfio-crypto.c
    ${pdfio_source_SOURCE_DIR}/pdfio-dict.c
    ${pdfio_source_SOURCE_DIR}/pdfio-file.c
    ${pdfio_source_SOURCE_DIR}/pdfio-md5.c
    ${pdfio_source_SOURCE_DIR}/pdfio-object.c
    ${pdfio_source_SOURCE_DIR}/pdfio-page.c
    ${pdfio_source_SOURCE_DIR}/pdfio-rc4.c
    ${pdfio_source_SOURCE_DIR}/pdfio-sha256.c
    ${pdfio_source_SOURCE_DIR}/pdfio-stream.c
    ${pdfio_source_SOURCE_DIR}/pdfio-string.c
    ${pdfio_source_SOURCE_DIR}/pdfio-token.c
    ${pdfio_source_SOURCE_DIR}/pdfio-value.c
    ${pdfio_source_SOURCE_DIR}/ttf.c
)
target_include_directories(vulpes_pdfio PUBLIC ${pdfio_source_SOURCE_DIR})
target_link_libraries(vulpes_pdfio PRIVATE ZLIB::ZLIB)
if(WIN32)
    target_link_libraries(vulpes_pdfio PRIVATE advapi32)
endif()

file(READ "${pdfio_source_SOURCE_DIR}/examples/Roboto-Regular.ttf" VULPES_PDF_FONT_HEX HEX)
string(LENGTH "${VULPES_PDF_FONT_HEX}" VULPES_PDF_FONT_HEX_LENGTH)
math(EXPR VULPES_PDF_FONT_HEX_LENGTH "${VULPES_PDF_FONT_HEX_LENGTH} / 2")
string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "0x\\1," VULPES_PDF_FONT_BYTES "${VULPES_PDF_FONT_HEX}")
configure_file(cmake/pdf_font_data.cpp.in generated/pdf_font_data.cpp @ONLY)
unset(VULPES_PDF_FONT_HEX)
unset(VULPES_PDF_FONT_HEX_LENGTH)
unset(VULPES_PDF_FONT_BYTES)

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
    src/appmeta/definition.cpp
    src/appmeta/loader.cpp
    src/appmeta/metadata.cpp
    src/core/actions.cpp
    src/core/application.cpp
    src/core/browse_controller.cpp
    src/core/clipboard.cpp
    src/core/command.cpp
    src/core/formatting.cpp
    src/core/localization.cpp
    src/core/workspace_preferences.cpp
    src/db/database.cpp
    src/db/identifier.cpp
    src/db/row.cpp
    src/db/schema.cpp
    src/db/statement.cpp
    src/db/transaction.cpp
    src/db/value.cpp
    src/model/dataset.cpp
    src/report/export.cpp
    src/report/pdf_export.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated/pdf_font_data.cpp
    src/terminal/ansi_input.cpp
    src/terminal/ansi_encoder.cpp
    src/terminal/capabilities.cpp
    src/terminal/cpp_terminal_adapter.cpp
    src/terminal/console_terminal.cpp
    src/terminal/frame_diff.cpp
    src/terminal/screen_buffer.cpp
    src/terminal/test_terminal.cpp
    src/terminal/unicode.cpp
    src/ui/button.cpp
    src/ui/confirmation_dialog.cpp
    src/ui/container.cpp
    src/ui/directory_browser.cpp
    src/ui/browse_document.cpp
    src/ui/document_session.cpp
    src/ui/focus_ring.cpp
    src/ui/form.cpp
    src/ui/grid.cpp
    src/ui/label.cpp
    src/ui/line_editor.cpp
    src/ui/multiline_editor.cpp
    src/ui/relationship_lookup.cpp
    src/ui/report_document.cpp
    src/ui/schema_document.cpp
    src/ui/sql_console.cpp
    src/ui/sql_document.cpp
    src/ui/status_bar.cpp
    src/ui/terminal_diagnostics.cpp
    src/ui/terminal_warning.cpp
    src/ui/text_prompt.cpp
    src/ui/theme.cpp
    src/ui/workspace.cpp
    src/ui/workspace_text.cpp
    src/ui/window_frame.cpp
    src/ui/window_manager.cpp
    src/ui/widget.cpp
)
add_library(Vulpes::core ALIAS vulpes_core)

target_include_directories(vulpes_core
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(vulpes_core
    PUBLIC vulpes_options
    PRIVATE unofficial::sqlite3::sqlite3 utf8proc::utf8proc nlohmann_json::nlohmann_json cpp-terminal::cpp-terminal
            clip::clip ICU::data ICU::i18n ICU::uc vulpes_pdfio)
target_compile_definitions(vulpes_core PRIVATE VULPES_BUILDING_LIBRARY)

configure_file(cmake/version.hpp.in generated/vulpes/version.hpp @ONLY)
target_include_directories(vulpes_core PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>)

add_executable(vulpes src/main.cpp)
target_link_libraries(vulpes PRIVATE Vulpes::core CLI11::CLI11)

if(BUILD_TESTING)
    add_test(NAME vulpes_version COMMAND vulpes --version)
    set_tests_properties(vulpes_version PROPERTIES PASS_REGULAR_EXPRESSION "Vulpes [0-9]+\\.[0-9]+\\.[0-9]+")
    add_test(
        NAME vulpes_version_metadata
        COMMAND ${CMAKE_COMMAND}
            -DVULPES_VERSION_MODULE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/VulpesVersion.cmake
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/assert_version_metadata.cmake
    )
    add_test(NAME vulpes_terminal_capabilities COMMAND vulpes --terminal-capabilities)
    set_tests_properties(vulpes_terminal_capabilities PROPERTIES PASS_REGULAR_EXPRESSION "Interactive TUI available")
    add_test(
        NAME vulpes_redirected_terminal_rejected
        COMMAND ${CMAKE_COMMAND}
            -DVULPES_EXE=$<TARGET_FILE:vulpes>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/assert_redirected_terminal_rejected.cmake
    )
    add_test(
        NAME vulpes_missing_database_does_not_create
        COMMAND ${CMAKE_COMMAND}
            -DVULPES_EXE=$<TARGET_FILE:vulpes>
            -DTEST_DATABASE=${CMAKE_CURRENT_BINARY_DIR}/vulpes-missing-database.sqlite
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/assert_missing_database_does_not_create.cmake
    )
    add_test(
        NAME vulpes_migrate_application_metadata
        COMMAND ${CMAKE_COMMAND}
            -DVULPES_EXE=$<TARGET_FILE:vulpes>
            -DTEST_DATABASE=${CMAKE_CURRENT_BINARY_DIR}/vulpes-migrate-app.sqlite
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/assert_migrate_app.cmake
    )
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
