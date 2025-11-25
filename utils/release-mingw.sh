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
    if [[ -d "/c/Program Files/RedPanda-Cpp/mingw64" ]]; then
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

if [[ -z "$MINGW_PATH" ]] &&  [[ -n "$MINGW64_PATH" ]]; then
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
