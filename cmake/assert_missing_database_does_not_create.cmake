if(NOT DEFINED VULPES_EXE OR NOT DEFINED TEST_DATABASE)
    message(FATAL_ERROR "VULPES_EXE and TEST_DATABASE are required")
endif()

if(EXISTS "${TEST_DATABASE}")
    message(FATAL_ERROR "test database unexpectedly exists before the CLI safety test: ${TEST_DATABASE}")
endif()

execute_process(
    COMMAND "${VULPES_EXE}" "${TEST_DATABASE}" --command tables
    RESULT_VARIABLE result
    OUTPUT_QUIET
    ERROR_QUIET
)

if(result EQUAL 0)
    message(FATAL_ERROR "opening a missing database unexpectedly succeeded")
endif()
if(EXISTS "${TEST_DATABASE}")
    message(FATAL_ERROR "opening a missing database unexpectedly created ${TEST_DATABASE}")
endif()
