#!/usr/bin/env bash

cd "$(dirname "$0")/.." || exit 1

EGE_DIR=$(pwd)

git clean -ffdx build Release
declare -a FAILED_TASKS=()

# 环境变量默认值
HAS_ARCH_X86_64="${HAS_ARCH_X86_64:-true}"
HAS_ARCH_X86="${HAS_ARCH_X86:-true}"
BUILD_MSVC2022="${BUILD_MSVC2022:-true}"
BUILD_MSVC2019="${BUILD_MSVC2019:-true}"
BUILD_MSVC2017="${BUILD_MSVC2017:-true}"
BUILD_MSVC2015="${BUILD_MSVC2015:-true}"
BUILD_MSVC2010="${BUILD_MSVC2010:-true}"

function hasX64() {
    [[ "$HAS_ARCH_X86_64" == "true" ]]
}

function hasX86() {
    [[ "$HAS_ARCH_X86" == "true" ]]
}

# 构建函数 - 减少重复代码
function msvcBuild() {
    local vs_version="$1"
    local toolset="$2"
    local build_flag="$3"

    if [[ "$build_flag" != "true" ]]; then
        echo "Skipping $vs_version build (BUILD_$vs_version != true)"
        return 0
    fi

    echo "Building $vs_version..."

    # 64bit
    if hasX64; then
        echo "Building $vs_version x64..."
        if ./tasks.sh --clean --release --toolset "$toolset" --arch x64 --target xege --load --build; then
            mkdir -p "Release/lib/$vs_version/x64"
            cp -rf build/Release/*.lib "Release/lib/$vs_version/x64/" || {
                echo "Error: Failed to copy $vs_version x64 libs"
                exit 1
            }
            git clean -ffdx build/Release
            echo "Copy $vs_version x64 libs done: $(pwd)/Release/lib/$vs_version/x64"

            ./utils/test-release-libs.sh --toolset "$toolset" --arch x64 --build-dir "build-${vs_version/vs/msvc}"
        else
            echo "Error: Failed to build $vs_version x64"
            FAILED_TASKS+=("$vs_version-x64")
        fi
    fi

    # 32bit
    if hasX86; then
        echo "Building $vs_version x86..."
        if ./tasks.sh --clean --release --toolset "$toolset" --arch Win32 --target xege --load --build; then
            mkdir -p "Release/lib/$vs_version/x86"
            cp -rf build/Release/*.lib "Release/lib/$vs_version/x86/" || {
                echo "Error: Failed to copy $vs_version x86 libs"
                exit 1
            }
            git clean -ffdx build/Release
            echo "Copy $vs_version x86 libs done: $(pwd)/Release/lib/$vs_version/x86"

            ./utils/test-release-libs.sh --toolset "$toolset" --arch Win32 --build-dir "build-${vs_version/vs/msvc}"
        else
            echo "Error: Failed to build $vs_version x86"
            FAILED_TASKS+=("$vs_version-x86")
        fi
    fi
}

# 构建各个版本
msvcBuild "vs2022" "v143" "$BUILD_MSVC2022"
msvcBuild "vs2019" "v142" "$BUILD_MSVC2019"
msvcBuild "vs2017" "v141" "$BUILD_MSVC2017"
msvcBuild "vs2015" "v140" "$BUILD_MSVC2015"

if [[ "$BUILD_MSVC2010" == "true" ]]; then
    echo "Building vs2010 (x64 and x86, handling UTF-8 encoding for old MSVC)..."
    ./utils/handle_utf8_encoding.sh v100 add_bom
    msvcBuild "vs2010" "v100" "$BUILD_MSVC2010"
    ./utils/handle_utf8_encoding.sh v100 remove_bom
else
    echo "Skipping vs2010 build (BUILD_MSVC2010 != true)"
fi

if [[ ${#FAILED_TASKS[@]} -gt 0 ]]; then
    echo "Failed tasks:" >&2
    printf "  %s\n" "${FAILED_TASKS[@]}" >&2
    exit ${#FAILED_TASKS[@]}
else
    echo "All tasks completed successfully."
fi
