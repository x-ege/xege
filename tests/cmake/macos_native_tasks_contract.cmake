if(NOT DEFINED EGE_SOURCE_DIR OR NOT DEFINED EGE_TASKS_CONTRACT_BINARY_DIR)
    message(FATAL_ERROR
        "macos_native_tasks_contract.cmake requires EGE_SOURCE_DIR and "
        "EGE_TASKS_CONTRACT_BINARY_DIR")
endif()

# Help probes must be true no-op entry points. This also prevents regressions
# where a release helper cleans generated directories before parsing options.
foreach(_ege_help_script
        tasks.sh
        utils/release-mingw.sh
        utils/release-msvc.sh
        utils/test-release-libs.sh)
    execute_process(
        COMMAND bash "${_ege_help_script}" --help
        WORKING_DIRECTORY "${EGE_SOURCE_DIR}"
        RESULT_VARIABLE _ege_help_result
        OUTPUT_VARIABLE _ege_help_stdout
        ERROR_VARIABLE _ege_help_stderr)
    if(NOT _ege_help_result EQUAL 0
            OR NOT _ege_help_stdout MATCHES "[Uu]sage:")
        message(FATAL_ERROR
            "${_ege_help_script} --help must succeed without starting a build.\n"
            "stdout:\n${_ege_help_stdout}\n"
            "stderr:\n${_ege_help_stderr}")
    endif()
endforeach()

file(READ "${EGE_SOURCE_DIR}/.vscode/tasks.json" _ege_vscode_tasks)
string(FIND "${_ege_vscode_tasks}" ".exe\"" _ege_exe_reference)
if(NOT _ege_exe_reference EQUAL -1)
    message(FATAL_ERROR
        ".vscode/tasks.json must pass logical target names, not Windows .exe names")
endif()

file(READ "${EGE_SOURCE_DIR}/tasks.sh" _ege_tasks_script)
string(FIND "${_ege_tasks_script}" "ctest --test-dir" _ege_new_ctest_option)
if(NOT _ege_new_ctest_option EQUAL -1)
    message(FATAL_ERROR
        "tasks.sh must remain compatible with the documented CTest 3.13 minimum")
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
        "-DEGE_ENABLE_WINDOW_TESTS=OFF"
        "-DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=11.0")
    string(FIND "${_ege_tasks_stdout}" "${_ege_expected_text}" _ege_text_match)
    if(_ege_text_match EQUAL -1)
        message(FATAL_ERROR
            "tasks.sh did not report required native setting: ${_ege_expected_text}\n"
            "${_ege_tasks_stdout}")
    endif()
endforeach()

# Explicit definitions may use either CMake's untyped -DNAME=value form or its
# typed -DNAME:TYPE=value form. In both cases tasks.sh must recognise the
# caller's value and avoid appending a second platform default.
foreach(_ege_definition_style untyped typed)
    if(_ege_definition_style STREQUAL "typed")
        set(_ege_backend_definition "-DEGE_DEFAULT_BACKEND:STRING=COREGRAPHICS")
        set(_ege_opengl_definition "-DEGE_ENABLE_OPENGL:BOOL=OFF")
        set(_ege_window_definition "-DEGE_ENABLE_WINDOW_TESTS:BOOL=OFF")
    else()
        set(_ege_backend_definition "-DEGE_DEFAULT_BACKEND=COREGRAPHICS")
        set(_ege_opengl_definition "-DEGE_ENABLE_OPENGL=OFF")
        set(_ege_window_definition "-DEGE_ENABLE_WINDOW_TESTS=OFF")
    endif()

    execute_process(
        COMMAND bash -l tasks.sh --debug --show-config --
            "${_ege_backend_definition}"
            "${_ege_opengl_definition}"
            "${_ege_window_definition}"
        WORKING_DIRECTORY "${EGE_SOURCE_DIR}"
        RESULT_VARIABLE _ege_definition_probe_result
        OUTPUT_VARIABLE _ege_definition_probe_stdout
        ERROR_VARIABLE _ege_definition_probe_stderr)
    if(NOT _ege_definition_probe_result EQUAL 0)
        message(FATAL_ERROR
            "tasks.sh ${_ege_definition_style} -D probe failed "
            "(${_ege_definition_probe_result}).\n"
            "stdout:\n${_ege_definition_probe_stdout}\n"
            "stderr:\n${_ege_definition_probe_stderr}")
    endif()

    foreach(_ege_definition_name
            EGE_DEFAULT_BACKEND EGE_ENABLE_OPENGL EGE_ENABLE_WINDOW_TESTS
            CMAKE_OSX_DEPLOYMENT_TARGET)
        string(REGEX MATCHALL
            "-D${_ege_definition_name}(:[A-Za-z_]+)?=[^ \t\r\n]+"
            _ege_definition_matches "${_ege_definition_probe_stdout}")
        list(LENGTH _ege_definition_matches _ege_definition_count)
        if(NOT _ege_definition_count EQUAL 1)
            message(FATAL_ERROR
                "tasks.sh must preserve exactly one ${_ege_definition_name} "
                "for ${_ege_definition_style} -D input, found "
                "${_ege_definition_count}.\n${_ege_definition_probe_stdout}")
        endif()
    endforeach()
endforeach()

execute_process(
    COMMAND bash -l tasks.sh --debug --show-config --
        -DCMAKE_SYSTEM_NAME:STRING=Windows
    WORKING_DIRECTORY "${EGE_SOURCE_DIR}"
    RESULT_VARIABLE _ege_typed_system_probe_result
    OUTPUT_VARIABLE _ege_typed_system_probe_stdout
    ERROR_VARIABLE _ege_typed_system_probe_stderr)
if(NOT _ege_typed_system_probe_result EQUAL 0)
    message(FATAL_ERROR
        "tasks.sh typed CMAKE_SYSTEM_NAME probe failed "
        "(${_ege_typed_system_probe_result}).\n"
        "stdout:\n${_ege_typed_system_probe_stdout}\n"
        "stderr:\n${_ege_typed_system_probe_stderr}")
endif()
foreach(_ege_forbidden_native_text
        "/build/macos/Debug"
        "macOS native build: AppleClang/CoreGraphics"
        "-DEGE_DEFAULT_BACKEND=COREGRAPHICS")
    string(FIND "${_ege_typed_system_probe_stdout}"
        "${_ege_forbidden_native_text}" _ege_forbidden_native_match)
    if(NOT _ege_forbidden_native_match EQUAL -1)
        message(FATAL_ERROR
            "A typed Windows CMAKE_SYSTEM_NAME must disable native macOS "
            "defaults, but tasks.sh reported: ${_ege_forbidden_native_text}\n"
            "${_ege_typed_system_probe_stdout}")
    endif()
endforeach()

# Exercise the same three-level platform/configuration directory shape used by
# the VS Code tasks. This guards against passing '../..' from
# build/macos/Debug, which resolves to build/ instead of the source checkout.
file(REMOVE_RECURSE "${EGE_TASKS_CONTRACT_BINARY_DIR}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "EGE_BUILD_ROOT=${EGE_TASKS_CONTRACT_BINARY_DIR}"
        "EGE_SOURCE_PATH=."
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
        "EGE_ENABLE_WINDOW_TESTS:BOOL=OFF"
        "CMAKE_OSX_DEPLOYMENT_TARGET:STRING=11.0")
    string(FIND "${_ege_nested_cache_contents}" "${_ege_expected_cache_entry}"
        _ege_nested_cache_match)
    if(_ege_nested_cache_match EQUAL -1)
        message(FATAL_ERROR
            "Nested task cache omitted: ${_ege_expected_cache_entry}")
    endif()
endforeach()

# tasks.sh resolves the source to a physical path before invoking CMake. Its
# cleanup ownership check must therefore accept the same checkout reached
# through a symlink instead of rejecting the cache it just created.
set(_ege_source_symlink "${EGE_TASKS_CONTRACT_BINARY_DIR}-source-link")
file(REMOVE "${_ege_source_symlink}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E create_symlink
        "${EGE_SOURCE_DIR}" "${_ege_source_symlink}"
    RESULT_VARIABLE _ege_symlink_result
    ERROR_VARIABLE _ege_symlink_error)
if(NOT _ege_symlink_result EQUAL 0)
    message(FATAL_ERROR
        "Unable to create tasks.sh symlink probe: ${_ege_symlink_error}")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "EGE_BUILD_ROOT=${EGE_TASKS_CONTRACT_BINARY_DIR}"
        bash "${_ege_source_symlink}/tasks.sh" --debug --clean
    WORKING_DIRECTORY "${_ege_source_symlink}"
    RESULT_VARIABLE _ege_symlink_clean_result
    OUTPUT_VARIABLE _ege_symlink_clean_stdout
    ERROR_VARIABLE _ege_symlink_clean_stderr)
file(REMOVE "${_ege_source_symlink}")
if(NOT _ege_symlink_clean_result EQUAL 0
        OR EXISTS "${EGE_TASKS_CONTRACT_BINARY_DIR}/macos/Debug")
    message(FATAL_ERROR
        "tasks.sh rejected its cache through a source symlink "
        "(${_ege_symlink_clean_result}).\n"
        "stdout:\n${_ege_symlink_clean_stdout}\n"
        "stderr:\n${_ege_symlink_clean_stderr}")
endif()

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
        "-DEGE_ENABLE_WINDOW_TESTS=OFF"
        "-DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=11.0")
    string(FIND "${_ege_release_tasks_stdout}" "${_ege_expected_text}"
        _ege_release_text_match)
    if(_ege_release_text_match EQUAL -1)
        message(FATAL_ERROR
            "tasks.sh release mode omitted: ${_ege_expected_text}\n"
            "${_ege_release_tasks_stdout}")
    endif()
endforeach()
