if(NOT DEFINED CMAKE_COMMAND OR NOT DEFINED VULPES_BUILD_DIR OR NOT DEFINED VULPES_CONFIGURATION OR
   NOT DEFINED VULPES_EXECUTABLE_SUFFIX)
    message(FATAL_ERROR "install-tree test is missing required arguments")
endif()

set(VULPES_INSTALL_PREFIX "${VULPES_BUILD_DIR}/install-check/${VULPES_CONFIGURATION}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${VULPES_BUILD_DIR}" --config "${VULPES_CONFIGURATION}" --prefix
            "${VULPES_INSTALL_PREFIX}"
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "install tree failed:\n${install_output}\n${install_error}")
endif()

set(VULPES_INSTALLED_EXE "${VULPES_INSTALL_PREFIX}/bin/vulpes${VULPES_EXECUTABLE_SUFFIX}")
if(NOT EXISTS "${VULPES_INSTALLED_EXE}")
    message(FATAL_ERROR "installed Vulpes executable is missing: ${VULPES_INSTALLED_EXE}")
endif()
execute_process(
    COMMAND "${VULPES_INSTALLED_EXE}" --version
    RESULT_VARIABLE version_result
    OUTPUT_VARIABLE version_output
    ERROR_VARIABLE version_error)
if(NOT version_result EQUAL 0 OR NOT version_output MATCHES "Vulpes [0-9]+\\.[0-9]+\\.[0-9]+")
    message(FATAL_ERROR "installed Vulpes executable did not run:\n${version_output}\n${version_error}")
endif()

foreach(document LICENSE THIRD_PARTY_NOTICES.md operations.md releasing.md release-notes.md)
    if(NOT EXISTS "${VULPES_INSTALL_PREFIX}/share/doc/Vulpes/${document}")
        message(FATAL_ERROR "installed release document is missing: ${document}")
    endif()
endforeach()
foreach(license sqlite3.txt utf8proc.txt icu.txt cpp-terminal.txt pdfio-apache-2.0.txt roboto-ofl-1.1.txt)
    if(NOT EXISTS "${VULPES_INSTALL_PREFIX}/share/doc/Vulpes/licenses/${license}")
        message(FATAL_ERROR "installed third-party license is missing: ${license}")
    endif()
endforeach()
if(NOT EXISTS "${VULPES_INSTALL_PREFIX}/share/vulpes/translations/cs.json")
    message(FATAL_ERROR "installed Czech catalog is missing")
endif()
