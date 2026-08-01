if(NOT DEFINED EGE_SOURCE_DIR
        OR NOT DEFINED EGE_CONTRACT_BINARY_DIR
        OR NOT DEFINED EGE_EXPECTED_BACKEND)
    message(FATAL_ERROR
        "backend_contract.cmake requires EGE_SOURCE_DIR, "
        "EGE_CONTRACT_BINARY_DIR, and EGE_EXPECTED_BACKEND")
endif()

file(REMOVE_RECURSE "${EGE_CONTRACT_BINARY_DIR}")

set(_ege_common_configure_args
    -DEGE_BUILD_DEMO=OFF
    -DEGE_BUILD_TEST=OFF
    -DEGE_BUILD_TEMP=OFF
    -DEGE_ENABLE_CAMERA_CAPTURE=OFF
    -DEGE_ENABLE_OPENGL=OFF
)

set(_ege_configure_command
    "${CMAKE_COMMAND}"
    -S "${EGE_SOURCE_DIR}"
    -B "${EGE_CONTRACT_BINARY_DIR}"
    ${_ege_common_configure_args}
    -DEGE_DEFAULT_BACKEND=AUTO
)
if(DEFINED EGE_GENERATOR AND NOT EGE_GENERATOR STREQUAL "")
    list(APPEND _ege_configure_command -G "${EGE_GENERATOR}")
endif()

execute_process(
    COMMAND ${_ege_configure_command}
    RESULT_VARIABLE _ege_configure_result
    OUTPUT_VARIABLE _ege_configure_stdout
    ERROR_VARIABLE _ege_configure_stderr
)
if(NOT _ege_configure_result EQUAL 0)
    message(FATAL_ERROR
        "Fresh AUTO configure failed (${_ege_configure_result}).\n"
        "stdout:\n${_ege_configure_stdout}\n"
        "stderr:\n${_ege_configure_stderr}")
endif()

file(READ "${EGE_CONTRACT_BINARY_DIR}/CMakeCache.txt" _ege_cache)
string(REGEX MATCH
    "(^|\n)EGE_RESOLVED_BACKEND:INTERNAL=${EGE_EXPECTED_BACKEND}(\n|$)"
    _ege_backend_match "${_ege_cache}")
if(_ege_backend_match STREQUAL "")
    message(FATAL_ERROR
        "AUTO resolved to an unexpected backend. Expected "
        "${EGE_EXPECTED_BACKEND}.\n${_ege_cache}")
endif()

set(_ege_invalid_binary_dir "${EGE_CONTRACT_BINARY_DIR}-invalid")
file(REMOVE_RECURSE "${_ege_invalid_binary_dir}")
set(_ege_invalid_configure_command
    "${CMAKE_COMMAND}"
    -S "${EGE_SOURCE_DIR}"
    -B "${_ege_invalid_binary_dir}"
    ${_ege_common_configure_args}
    -DEGE_DEFAULT_BACKEND=NOT_A_BACKEND
)
if(DEFINED EGE_GENERATOR AND NOT EGE_GENERATOR STREQUAL "")
    list(APPEND _ege_invalid_configure_command -G "${EGE_GENERATOR}")
endif()

execute_process(
    COMMAND ${_ege_invalid_configure_command}
    RESULT_VARIABLE _ege_invalid_result
    OUTPUT_QUIET
    ERROR_QUIET
)
if(_ege_invalid_result EQUAL 0)
    message(FATAL_ERROR "An invalid backend unexpectedly configured successfully")
endif()
