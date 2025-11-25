#!/usr/bin/env bash

cd "$(dirname "$0")/.." || exit 1

EGE_DIR=$(pwd)

git clean -ffdx build Release
set -e
set -x

# 环境变量默认值
BUILD_ARCH_X64="${BUILD_ARCH_X64:-true}"
BUILD_ARCH_X86="${BUILD_ARCH_X86:-false}"

function findMingwPath() {
    # 找几个默认的路径
    if [[ -d "/c/TDM-GCC-64" ]]; then
        echo "/c/TDM-GCC-64"
    elif [[ -d "/c/Program Files/RedPanda-Cpp/mingw64" ]]; then
        echo "/c/Program Files/RedPanda-Cpp/mingw64"
    elif [[ -d "/c/Program Files (x86)/Dev-Cpp/MinGW64" ]]; then
        echo "/c/Program Files (x86)/Dev-Cpp/MinGW64"
    elif [[ -d "/c/MinGW64" ]]; then
        echo "/c/MinGW64"
    elif [[ -d "/c/MinGW" ]]; then
        echo "/c/MinGW"
    else
        echo ""
    fi
}

if [[ -z "$MINGW_PATH" ]] && [[ -n "$MINGW64_PATH" ]]; then
    # 如果 MINGW_PATH 未设置, 但 MINGW64_PATH 已设置, 则使用 MINGW64_PATH
    export MINGW_PATH="$MINGW64_PATH"
fi

# 未设置 MINGW 路径, 尝试找到一个合适的
if [[ -z "$MINGW_PATH" ]]; then
    MINGW_PATH=$(findMingwPath)

    if [[ ! -d "$MINGW_PATH" ]]; then
        echo "Error: MINGW_PATH is not set and no default path found."
        echo "Please set MINGW_PATH to your MinGW installation directory."
        exit 1
    fi
else
    if command -v cygpath &>/dev/null; then
        echo "Converting MINGW_PATH to Unix style..."
        MINGW_PATH=$(cygpath -u "$MINGW_PATH")
    fi
fi

MINGW_BIN="$MINGW_PATH/bin"

if [[ $(uname -s) == MINGW* ]] || [[ "$OSTYPE" == "msys" ]]; then
    echo "Building in Windows MINGW environment..."

    # 检查 MINGW 路径是否存在
    if [[ ! -d "$MINGW_BIN" ]]; then
        echo "Error: MINGW not found at $MINGW_PATH"
        echo "Please check if MinGW is installed and configured correctly."
        exit 1
    fi

    # 添加 MINGW 到 PATH
    export PATH="$MINGW_BIN:$PATH"

    export CC="gcc"
    export CXX="g++"
    export RC="windres"
else
    echo "Unsupported environment for MINGW build"
    exit 1
fi

# 验证编译器是否可用
if ! command -v $CC &>/dev/null; then
    echo "Error: $CC could not be found"
    echo "PATH: $PATH"
    exit 1
fi

echo "Found compiler: $(which $CC)"
echo "Compiler version: $($CC --version | head -n1)"

# 验证其他工具
echo "C++ compiler: $(which $CXX)"
echo "Resource compiler: $(which $RC)"

declare -a FAILED_TASKS=()

# 验证库文件架构
function verifyLibArchitecture() {
    local lib_file="$1"
    local expected_arch="$2" # "x64" 或 "x86"

    if [[ ! -f "$lib_file" ]]; then
        echo "Error: Library file not found: $lib_file"
        return 1
    fi

    # 使用 objdump 检查架构
    local file_info
    if command -v objdump &>/dev/null; then
        file_info=$(objdump -f "$lib_file" 2>/dev/null | head -20)

        if [[ "$expected_arch" == "x64" ]]; then
            if echo "$file_info" | grep -qi "x86-64\|pe-x86-64"; then
                echo "✓ Verified: $lib_file is 64-bit (x86-64)"
                return 0
            else
                echo "✗ Error: $lib_file is NOT 64-bit!"
                echo "  File info: $file_info"
                return 1
            fi
        elif [[ "$expected_arch" == "x86" ]]; then
            # 32位应该是 i386 或 pe-i386，但不能是 x86-64
            if echo "$file_info" | grep -qi "x86-64\|pe-x86-64"; then
                echo "✗ Error: $lib_file is 64-bit, expected 32-bit!"
                echo "  File info: $file_info"
                return 1
            elif echo "$file_info" | grep -qi "i386\|pe-i386"; then
                echo "✓ Verified: $lib_file is 32-bit (i386)"
                return 0
            else
                echo "? Warning: Cannot determine architecture for $lib_file"
                echo "  File info: $file_info"
                return 1
            fi
        fi
    elif command -v file &>/dev/null; then
        # 备用方案：使用 file 命令
        file_info=$(file "$lib_file")
        echo "File info: $file_info"

        if [[ "$expected_arch" == "x64" ]] && echo "$file_info" | grep -qi "x86-64"; then
            echo "✓ Verified: $lib_file is 64-bit"
            return 0
        elif [[ "$expected_arch" == "x86" ]] && echo "$file_info" | grep -qiE "i386|intel 80386|pe32"; then
            echo "✓ Verified: $lib_file is 32-bit"
            return 0
        else
            echo "✗ Architecture mismatch for $lib_file"
            return 1
        fi
    else
        echo "Warning: Neither objdump nor file command available, skipping verification"
        return 0
    fi
}

# 构建函数
function mingwBuild() {
    local arch="$1"
    local arch_flag="$2"
    local output_dir="$3"

    echo "Building MinGW $arch..."

    if ./tasks.sh --release \
        --target xege \
        --clean \
        --load \
        --build \
        -- \
        -G "MinGW Makefiles" \
        -DCMAKE_C_COMPILER="$CC" \
        -DCMAKE_CXX_COMPILER="$CXX" \
        -DCMAKE_RC_COMPILER="$RC" \
        -DCMAKE_MAKE_PROGRAM="$(which mingw32-make)" \
        -DCMAKE_C_FLAGS="$arch_flag" \
        -DCMAKE_CXX_FLAGS="$arch_flag"; then

        echo "Build $arch successful!"

        # 显示生成的文件
        echo "Generated files:"
        mkdir -p "Release/lib/$output_dir"
        find build -type f -name "*.a" -exec cp {} "Release/lib/$output_dir/" \;
        ls -l "Release/lib/$output_dir"

        # 验证生成的库文件架构
        echo ""
        echo "Verifying library architecture for $arch build..."
        local verify_failed=false
        local found_libs=false
        for lib_file in "Release/lib/$output_dir"/*.a; do
            if [[ -f "$lib_file" ]]; then
                found_libs=true
                if ! verifyLibArchitecture "$lib_file" "$arch"; then
                    verify_failed=true
                fi
            fi
        done

        if [[ "$found_libs" != "true" ]]; then
            echo "✗ Error: No .a library files found in Release/lib/$output_dir"
            verify_failed=true
        fi

        if [[ "$verify_failed" == "true" ]]; then
            echo "Architecture verification failed for $arch build!"
            FAILED_TASKS+=("mingw-$arch-verify")
        else
            echo "Architecture verification passed for $arch build!"
        fi
        echo ""

        ./utils/test-release-libs.sh \
            --build-dir "build-mingw-$arch" \
            -- \
            -G "MinGW Makefiles" \
            -DCMAKE_C_COMPILER="$CC" \
            -DCMAKE_CXX_COMPILER="$CXX" \
            -DCMAKE_RC_COMPILER="$RC" \
            -DCMAKE_MAKE_PROGRAM="$(which mingw32-make)" \
            -DCMAKE_C_FLAGS="$arch_flag" \
            -DCMAKE_CXX_FLAGS="$arch_flag"
        if [[ $? -ne 0 ]]; then
            echo "Test for $arch failed!"
            FAILED_TASKS+=("mingw-$arch-test")
        fi
    else
        echo "Build $arch failed!"
        FAILED_TASKS+=("mingw-$arch")
    fi
}

# 根据环境设置 CMake 参数
# Windows MINGW 本地编译配置
echo "Configuring CMake for MINGW compilation..."

# 构建 64 位版本
if [[ "$BUILD_ARCH_X64" == "true" ]]; then
    mingwBuild "x64" "-m64" "mingw64"
else
    echo "Skipping x64 build (BUILD_ARCH_X64 != true)"
fi

# 构建 32 位版本
if [[ "$BUILD_ARCH_X86" == "true" ]]; then
    mingwBuild "x86" "-m32" "mingw32"
else
    echo "Skipping x86 build (BUILD_ARCH_X86 != true)"
fi

# 报告结果
if [[ ${#FAILED_TASKS[@]} -gt 0 ]]; then
    echo "Failed tasks:" >&2
    printf "  %s\n" "${FAILED_TASKS[@]}" >&2
    exit ${#FAILED_TASKS[@]}
else
    echo "All MinGW builds completed successfully."
fi
