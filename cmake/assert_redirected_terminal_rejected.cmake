if(NOT DEFINED VULPES_EXE)
    message(FATAL_ERROR "VULPES_EXE is required")
endif()

execute_process(
    COMMAND "${VULPES_EXE}" --config "${CMAKE_CURRENT_BINARY_DIR}/redirected-terminal-settings.json"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE standard_output
    ERROR_VARIABLE standard_error
)

if(result EQUAL 0)
    message(FATAL_ERROR "Vulpes unexpectedly started an interactive workspace with redirected output")
endif()

if(NOT standard_error MATCHES "interactive terminal requires terminal-connected standard")
    message(FATAL_ERROR "Vulpes did not report a useful redirected-terminal error: ${standard_error}")
endif()

string(ASCII 27 escape_character)
string(FIND "${standard_output}${standard_error}" "${escape_character}" escape_position)
if(NOT escape_position EQUAL -1)
    message(FATAL_ERROR "Vulpes emitted terminal control sequences before rejecting redirected streams")
endif()
