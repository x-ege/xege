if(NOT DEFINED EGE_SOURCE_DIR)
    message(FATAL_ERROR
        "macos_release_contract.cmake requires EGE_SOURCE_DIR")
endif()

function(ege_assert_contains contents expected context)
    string(FIND "${contents}" "${expected}" _ege_match)
    if(_ege_match EQUAL -1)
        message(FATAL_ERROR
            "${context} is missing required text: ${expected}")
    endif()
endfunction()

function(ege_assert_not_contains contents forbidden context)
    string(FIND "${contents}" "${forbidden}" _ege_match)
    if(NOT _ege_match EQUAL -1)
        message(FATAL_ERROR
            "${context} still contains retired text: ${forbidden}")
    endif()
endfunction()

file(READ "${EGE_SOURCE_DIR}/CMakeLists.txt" _ege_root_cmake)
foreach(_ege_required
        "CMAKE_OSX_DEPLOYMENT_TARGET \"11.0\""
        "Minimum macOS version supported by native XEGE builds")
    ege_assert_contains("${_ege_root_cmake}" "${_ege_required}"
        "root CMake configuration")
endforeach()

if(EGE_TARGET_APPLE AND DEFINED EGE_NATIVE_OBJECT_DIR)
    if(NOT DEFINED EGE_EXPECTED_MACOS_DEPLOYMENT_TARGET
            OR EGE_EXPECTED_MACOS_DEPLOYMENT_TARGET STREQUAL "")
        message(FATAL_ERROR
            "Expected macOS deployment target was not provided")
    endif()
    file(GLOB_RECURSE _ege_native_objects
        "${EGE_NATIVE_OBJECT_DIR}/*.o")
    if(NOT _ege_native_objects)
        message(FATAL_ERROR
            "No native object files found below ${EGE_NATIVE_OBJECT_DIR}")
    endif()
    foreach(_ege_native_object IN LISTS _ege_native_objects)
        execute_process(
            COMMAND xcrun vtool -show-build "${_ege_native_object}"
            RESULT_VARIABLE _ege_vtool_result
            OUTPUT_VARIABLE _ege_vtool_output
            ERROR_VARIABLE _ege_vtool_error)
        if(NOT _ege_vtool_result EQUAL 0)
            message(FATAL_ERROR
                "Unable to inspect ${_ege_native_object}: ${_ege_vtool_error}")
        endif()
        if(NOT _ege_vtool_output MATCHES
                "minos[ \t]+([0-9]+([.][0-9]+)*)")
            message(FATAL_ERROR
                "Native object has no readable minos value: "
                "${_ege_native_object}\n${_ege_vtool_output}")
        endif()
        set(_ege_actual_minos "${CMAKE_MATCH_1}")
        if(NOT "${_ege_actual_minos}" VERSION_EQUAL
                "${EGE_EXPECTED_MACOS_DEPLOYMENT_TARGET}")
            message(FATAL_ERROR
                "Native object does not target macOS "
                "${EGE_EXPECTED_MACOS_DEPLOYMENT_TARGET}: "
                "${_ege_native_object} (actual ${_ege_actual_minos})\n"
                "${_ege_vtool_output}")
        endif()
    endforeach()
endif()

file(READ "${EGE_SOURCE_DIR}/.github/workflows/release.yml"
    _ege_release_workflow)
foreach(_ege_required
        "build-linux-cross-library:"
        "-DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64.cmake"
        "build-macos-native-library:"
        "-DCMAKE_OSX_ARCHITECTURES=\"\${arch}\""
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=11.0"
        "xcrun lipo -create"
        "artifacts/macOS/libgraphics.a"
        "xcrun lipo artifacts/macOS/libgraphics.a -verify_arch arm64 x86_64"
        "cmake --build build-release-smoke --target demos"
        "cp README.md \"$RELEASE_DIR/\""
        "cp BUILD.md RELEASE.md LICENSE \"$RELEASE_DIR/\"")
    ege_assert_contains("${_ege_release_workflow}" "${_ege_required}"
        "release workflow")
endforeach()
foreach(_ege_retired
        "macOS-MinGW"
        "brew install mingw-w64")
    ege_assert_not_contains("${_ege_release_workflow}" "${_ege_retired}"
        "release workflow")
endforeach()

file(READ "${EGE_SOURCE_DIR}/.github/workflows/mingw-crosscompile-build.yml"
    _ege_cross_workflow)
ege_assert_contains("${_ege_cross_workflow}" "ubuntu-cross-compile:"
    "MinGW cross-compile workflow")
foreach(_ege_retired
        "macos-cross-compile:"
        "build-mingw-macos"
        "mingw-w64-macos")
    ege_assert_not_contains("${_ege_cross_workflow}" "${_ege_retired}"
        "MinGW cross-compile workflow")
endforeach()

file(READ "${EGE_SOURCE_DIR}/.github/release-assets/CMakeLists.txt"
    _ege_package_cmake)
foreach(_ege_required
        "if(CMAKE_HOST_UNIX AND NOT CMAKE_HOST_APPLE"
        "set(CMAKE_SYSTEM_NAME Windows)"
        "if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES \"AppleClang|Clang\")"
        "set(osLibDir \"macOS\")"
        "AppKit CoreGraphics CoreText ImageIO"
        "elseif(MINGW AND CMAKE_CXX_COMPILER_ID MATCHES \"GNU\")"
        "set(osLibDir \"mingw-w64-debian\")")
    ege_assert_contains("${_ege_package_cmake}" "${_ege_required}"
        "release package CMake")
endforeach()
ege_assert_not_contains("${_ege_package_cmake}"
    "if(CMAKE_HOST_UNIX)\n    set(CMAKE_SYSTEM_NAME Windows)"
    "release package CMake")
ege_assert_not_contains("${_ege_package_cmake}"
    "set(osLibDir \"macos-native\")" "release package CMake")
ege_assert_not_contains("${_ege_package_cmake}" "message(CHECK_"
    "release package CMake 3.13 compatibility")

file(READ "${EGE_SOURCE_DIR}/.github/workflows/msvc-build.yml"
    _ege_msvc_workflow)
foreach(_ege_required
        "VS2026_INSTANCE=\$vs2026"
        "-G \"Visual Studio 18 2026\" -A x64 -T v145"
        "CMAKE_GENERATOR_INSTANCE=\${{ steps.check_vs2026.outputs.VS2026_INSTANCE }}")
    ege_assert_contains("${_ege_msvc_workflow}" "${_ege_required}"
        "MSVC workflow")
endforeach()

file(READ "${EGE_SOURCE_DIR}/.github/workflows/mingw-windows-build.yml"
    _ege_mingw_windows_workflow)
foreach(_ege_required
        "build-mingw/libgraphics.a"
        "build-winlibs-gcc14/libgraphics.a"
        "build-winlibs-gcc13/libgraphics.a")
    ege_assert_contains("${_ege_mingw_windows_workflow}" "${_ege_required}"
        "MinGW Windows artifact workflow")
endforeach()
ege_assert_not_contains("${_ege_mingw_windows_workflow}"
    "-Recurse -Filter \"*.a\"" "MinGW Windows artifact workflow")

file(READ "${EGE_SOURCE_DIR}/.github/workflows/mingw-crosscompile-build.yml"
    _ege_mingw_cross_workflow)
ege_assert_contains("${_ege_mingw_cross_workflow}"
    "build-mingw-ubuntu/libgraphics.a" "MinGW cross artifact workflow")
ege_assert_not_contains("${_ege_mingw_cross_workflow}"
    "-name \"*.a\" -exec cp" "MinGW cross artifact workflow")

file(READ "${EGE_SOURCE_DIR}/demo/ege_release.cmake"
    _ege_local_package_cmake)
ege_assert_contains("${_ege_local_package_cmake}"
    "set(osLibDir \"macOS\")" "local release CMake")
ege_assert_not_contains("${_ege_local_package_cmake}"
    "set(osLibDir \"macos-native\")" "local release CMake")

file(READ "${EGE_SOURCE_DIR}/utils/release.sh" _ege_release_script)
foreach(_ege_required
        "\$RELEASE_DIR/lib/macOS"
        "MACOS_DEPLOYMENT_TARGET=\"\${EGE_MACOS_DEPLOYMENT_TARGET:-11.0}\""
        "xcrun lipo -create"
        ".github/release-assets/CMakeLists.txt"
        "cmake/toolchains/mingw-w64.cmake"
        "\$RELEASE_DIR/lib/mingw-w64-debian")
    ege_assert_contains("${_ege_release_script}" "${_ege_required}"
        "local release script")
endforeach()

foreach(_ege_legacy_release_script
        utils/release-mingw.sh
        utils/release-msvc.sh
        utils/test-release-libs.sh)
    file(READ "${EGE_SOURCE_DIR}/${_ege_legacy_release_script}"
        _ege_legacy_release_contents)
    ege_assert_contains("${_ege_legacy_release_contents}" "Usage:"
        "${_ege_legacy_release_script}")
    ege_assert_not_contains("${_ege_legacy_release_contents}" "git clean"
        "${_ege_legacy_release_script}")
endforeach()
foreach(_ege_force_clean_script
        utils/release-mingw.sh
        utils/release-msvc.sh)
    file(READ "${EGE_SOURCE_DIR}/${_ege_force_clean_script}"
        _ege_force_clean_contents)
    ege_assert_contains("${_ege_force_clean_contents}" "--force-clean"
        "${_ege_force_clean_script}")
    ege_assert_contains("${_ege_force_clean_contents}"
        "cmake -E remove_directory" "${_ege_force_clean_script}")
endforeach()
ege_assert_not_contains("${_ege_release_script}" "Release/lib/macos-native"
    "local release script")
