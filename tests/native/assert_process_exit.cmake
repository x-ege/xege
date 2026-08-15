if(NOT DEFINED EGE_EXIT_PROGRAM)
    message(FATAL_ERROR "EGE_EXIT_PROGRAM is required")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env EGE_HEADLESS=1 "${EGE_EXIT_PROGRAM}"
    RESULT_VARIABLE _ege_exit_result)
if(NOT "${_ege_exit_result}" STREQUAL "42")
    message(FATAL_ERROR
        "EGE replaced the application's exit status; expected 42, got ${_ege_exit_result}")
endif()
