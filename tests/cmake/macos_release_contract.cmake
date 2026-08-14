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

file(READ "${EGE_SOURCE_DIR}/.github/workflows/release.yml"
    _ege_release_workflow)
foreach(_ege_required
        "build-linux-cross-library:"
        "-DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64.cmake"
        "build-macos-native-library:"
        "-DCMAKE_OSX_ARCHITECTURES=\"\${arch}\""
        "xcrun lipo -create"
        "artifacts/macOS/libgraphics.a"
        "xcrun lipo artifacts/macOS/libgraphics.a -verify_arch arm64 x86_64"
        "cmake --build build-release-smoke --target demos")
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

file(READ "${EGE_SOURCE_DIR}/demo/ege_release.cmake"
    _ege_local_package_cmake)
ege_assert_contains("${_ege_local_package_cmake}"
    "set(osLibDir \"macOS\")" "local release CMake")
ege_assert_not_contains("${_ege_local_package_cmake}"
    "set(osLibDir \"macos-native\")" "local release CMake")

file(READ "${EGE_SOURCE_DIR}/utils/release.sh" _ege_release_script)
foreach(_ege_required
        "Release/lib/macOS"
        "cmake/toolchains/mingw-w64.cmake"
        "Release/lib/mingw-w64-debian")
    ege_assert_contains("${_ege_release_script}" "${_ege_required}"
        "local release script")
endforeach()
ege_assert_not_contains("${_ege_release_script}" "Release/lib/macos-native"
    "local release script")
