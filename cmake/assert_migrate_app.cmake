if(NOT DEFINED VULPES_EXE OR NOT DEFINED TEST_DATABASE)
    message(FATAL_ERROR "VULPES_EXE and TEST_DATABASE are required")
endif()

file(REMOVE "${TEST_DATABASE}")
file(WRITE "${TEST_DATABASE}" "")
execute_process(
    COMMAND "${VULPES_EXE}" "${TEST_DATABASE}" --migrate-app
    RESULT_VARIABLE migrate_result
    OUTPUT_VARIABLE migrate_output
    ERROR_VARIABLE migrate_error
)
if(NOT migrate_result EQUAL 0)
    message(FATAL_ERROR "application metadata migration failed: ${migrate_error}")
endif()
if(NOT migrate_output MATCHES "metadata schema 3")
    message(FATAL_ERROR "application metadata migration returned unexpected output: ${migrate_output}")
endif()

execute_process(
    COMMAND "${VULPES_EXE}" "${TEST_DATABASE}" --command reports
    RESULT_VARIABLE reports_result
    OUTPUT_VARIABLE reports_output
    ERROR_VARIABLE reports_error
)
file(REMOVE "${TEST_DATABASE}")
if(NOT reports_result EQUAL 0)
    message(FATAL_ERROR "migrated application metadata could not be loaded: ${reports_error}")
endif()
