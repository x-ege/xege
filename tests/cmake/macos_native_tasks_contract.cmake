if(NOT DEFINED EGE_SOURCE_DIR OR NOT DEFINED EGE_TASKS_CONTRACT_BINARY_DIR)
    message(FATAL_ERROR
        "macos_native_tasks_contract.cmake requires EGE_SOURCE_DIR and "
        "EGE_TASKS_CONTRACT_BINARY_DIR")
endif()

file(READ "${EGE_SOURCE_DIR}/.vscode/tasks.json" _ege_vscode_tasks)
string(FIND "${_ege_vscode_tasks}" ".exe\"" _ege_exe_reference)
if(NOT _ege_exe_reference EQUAL -1)
    message(FATAL_ERROR
        ".vscode/tasks.json must pass logical target names, not Windows .exe names")
endif()

string(REGEX MATCHALL "\"label\"[ \t]*:" _ege_task_labels
    "${_ege_vscode_tasks}")
string(REGEX MATCHALL "\"command\"[ \t]*:[ \t]*\"bash\"" _ege_bash_commands
    "${_ege_vscode_tasks}")
list(LENGTH _ege_task_labels _ege_task_count)
list(LENGTH _ege_bash_commands _ege_bash_task_count)
if(_ege_task_count EQUAL 0 OR NOT _ege_task_count EQUAL _ege_bash_task_count)
    message(FATAL_ERROR
        "Every repository VS Code task must route through the platform-aware tasks.sh")
endif()

execute_process(
    COMMAND bash tasks.sh --debug --show-config
    WORKING_DIRECTORY "${EGE_SOURCE_DIR}"
    RESULT_VARIABLE _ege_tasks_result
    OUTPUT_VARIABLE _ege_tasks_stdout
    ERROR_VARIABLE _ege_tasks_stderr)
if(NOT _ege_tasks_result EQUAL 0)
    message(FATAL_ERROR
        "tasks.sh macOS configuration probe failed (${_ege_tasks_result}).\n"
        "stdout:\n${_ege_tasks_stdout}\n"
        "stderr:\n${_ege_tasks_stderr}")
endif()

foreach(_ege_expected_text
        "/build/macos/Debug"
        "macOS native build: AppleClang/CoreGraphics (headless tests by default)"
        "-DEGE_DEFAULT_BACKEND=COREGRAPHICS"
        "-DEGE_ENABLE_OPENGL=OFF"
        "-DEGE_ENABLE_WINDOW_TESTS=OFF")
    string(FIND "${_ege_tasks_stdout}" "${_ege_expected_text}" _ege_text_match)
    if(_ege_text_match EQUAL -1)
        message(FATAL_ERROR
            "tasks.sh did not report required native setting: ${_ege_expected_text}\n"
            "${_ege_tasks_stdout}")
    endif()
endforeach()

# Exercise the same three-level platform/configuration directory shape used by
# the VS Code tasks. This guards against passing '../..' from
# build/macos/Debug, which resolves to build/ instead of the source checkout.
file(REMOVE_RECURSE "${EGE_TASKS_CONTRACT_BINARY_DIR}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "EGE_BUILD_ROOT=${EGE_TASKS_CONTRACT_BINARY_DIR}"
        bash tasks.sh --debug --target xege --load --
        -DEGE_BUILD_DEMO=OFF
        -DEGE_BUILD_TEST=OFF
        -DEGE_BUILD_TEMP=OFF
        -DEGE_ENABLE_CAMERA_CAPTURE=OFF
    WORKING_DIRECTORY "${EGE_SOURCE_DIR}"
    RESULT_VARIABLE _ege_nested_configure_result
    OUTPUT_VARIABLE _ege_nested_configure_stdout
    ERROR_VARIABLE _ege_nested_configure_stderr)
if(NOT _ege_nested_configure_result EQUAL 0)
    message(FATAL_ERROR
        "tasks.sh failed from its nested macOS build directory "
        "(${_ege_nested_configure_result}).\n"
        "stdout:\n${_ege_nested_configure_stdout}\n"
        "stderr:\n${_ege_nested_configure_stderr}")
endif()

set(_ege_nested_cache
    "${EGE_TASKS_CONTRACT_BINARY_DIR}/macos/Debug/CMakeCache.txt")
if(NOT EXISTS "${_ege_nested_cache}")
    message(FATAL_ERROR
        "tasks.sh did not create the native Debug CMake cache")
endif()
file(READ "${_ege_nested_cache}" _ege_nested_cache_contents)
foreach(_ege_expected_cache_entry
        "CMAKE_HOME_DIRECTORY:INTERNAL=${EGE_SOURCE_DIR}"
        "EGE_RESOLVED_BACKEND:INTERNAL=COREGRAPHICS"
        "EGE_ENABLE_WINDOW_TESTS:BOOL=OFF")
    string(FIND "${_ege_nested_cache_contents}" "${_ege_expected_cache_entry}"
        _ege_nested_cache_match)
    if(_ege_nested_cache_match EQUAL -1)
        message(FATAL_ERROR
            "Nested task cache omitted: ${_ege_expected_cache_entry}")
    endif()
endforeach()

execute_process(
    COMMAND bash tasks.sh --release --show-config
    WORKING_DIRECTORY "${EGE_SOURCE_DIR}"
    RESULT_VARIABLE _ege_release_tasks_result
    OUTPUT_VARIABLE _ege_release_tasks_stdout
    ERROR_VARIABLE _ege_release_tasks_stderr)
if(NOT _ege_release_tasks_result EQUAL 0)
    message(FATAL_ERROR
        "tasks.sh release configuration probe failed (${_ege_release_tasks_result}).\n"
        "stdout:\n${_ege_release_tasks_stdout}\n"
        "stderr:\n${_ege_release_tasks_stderr}")
endif()
foreach(_ege_expected_text
        "/build/macos/Release"
        "macOS native build: AppleClang/CoreGraphics (headless tests by default)"
        "-DEGE_DEFAULT_BACKEND=COREGRAPHICS"
        "-DEGE_ENABLE_OPENGL=OFF"
        "-DEGE_ENABLE_WINDOW_TESTS=OFF")
    string(FIND "${_ege_release_tasks_stdout}" "${_ege_expected_text}"
        _ege_release_text_match)
    if(_ege_release_text_match EQUAL -1)
        message(FATAL_ERROR
            "tasks.sh release mode omitted: ${_ege_expected_text}\n"
            "${_ege_release_tasks_stdout}")
    endif()
endforeach()
