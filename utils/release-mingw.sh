#!/usr/bin/env bash

cd "$(dirname "$0")/.." || exit 1

EGE_DIR=$(pwd)

git clean -ffdx build Release
set -e
set -x

function findMingw64Path() {
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

# 未设置 MINGW64 路径, 尝试找到一个合适的
if [[ -z "$MINGW64_PATH" ]]; then
    MINGW64_PATH=$(findMingw64Path)

    if [[ ! -d "$MINGW64_PATH" ]]; then
        echo "Error: MINGW64_PATH is not set and no default path found."
        echo "Please set MINGW64_PATH to your MinGW64 installation directory."
        exit 1
    fi
else
    if command -v cygpath &>/dev/null; then
        echo "Converting MINGW64_PATH to Unix style..."
        MINGW64_PATH=$(cygpath -u "$MINGW64_PATH")
    fi
fi

BUILD_MINGW32=false

# 处理 MINGW32_PATH
if [[ -z "$DISABLE_MINGW32" ]]; then
    if [[ -n "$MINGW32_PATH" ]]; then
        # 转换路径格式（如果需要）
        if command -v cygpath &>/dev/null; then
            echo "Converting MINGW32_PATH to Unix style..."
            MINGW32_PATH=$(cygpath -u "$MINGW32_PATH")
        fi

        if [[ -d "$MINGW32_PATH" ]]; then
            echo "MINGW32_PATH is set to '${MINGW32_PATH}', 32-bit MinGW will be built, too."
            BUILD_MINGW32=true
        fi
    fi
fi

function buildMingwArchitecture() {
    local ARCH="$1" # "64" or "32"
    local MINGW_PATH="$2"
    local OUTPUT_DIR="$3"

    echo "==================== Building ${ARCH}-bit version ===================="

    local MINGW_BIN="${MINGW_PATH}/bin"

    # 检查路径是否存在
    if [[ ! -d "$MINGW_BIN" ]]; then
        echo "Error: MinGW${ARCH} not found at $MINGW_PATH"
        return 1
    fi

    # 临时修改 PATH，只在此函数作用域内生效
    local OLD_PATH="$PATH"
    export PATH="$MINGW_BIN:$OLD_PATH"

    # 设置编译器
    export CC="gcc"
    export CXX="g++"
    export RC="windres"

    # 验证编译器是否可用
    if ! command -v $CC &>/dev/null; then
        echo "Error: $CC could not be found in $MINGW_BIN"
        export PATH="$OLD_PATH" # 恢复原来的PATH
        return 1
    fi

    echo "Found compiler: $(which $CC)"
    echo "Compiler version: $($CC --version | head -n1)"
    echo "C++ compiler: $(which $CXX)"
    echo "Resource compiler: $(which $RC)"

    # 编译
    echo "Configuring CMake for ${ARCH}-bit MinGW compilation..."

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
        -DCMAKE_MAKE_PROGRAM="$(which mingw32-make)"; then

        echo "${ARCH}-bit build successful!"

        # 创建输出目录并复制文件
        echo "Generated files for ${ARCH}-bit:"
        mkdir -p "$OUTPUT_DIR"
        find build -type f -name "*.a" -exec cp {} "$OUTPUT_DIR"/ \;
        ls -l "$OUTPUT_DIR"

        # 运行测试
        ./utils/test-release-libs.sh \
            --build-dir "build-mingw-windows-${ARCH}" \
            -- \
            -G "MinGW Makefiles" \
            -DCMAKE_C_COMPILER="$CC" \
            -DCMAKE_CXX_COMPILER="$CXX" \
            -DCMAKE_RC_COMPILER="$RC" \
            -DCMAKE_MAKE_PROGRAM="$(which mingw32-make)"
    else
        echo "${ARCH}-bit CMake configuration failed!"
        export PATH="$OLD_PATH" # 恢复原来的PATH
        return 1
    fi

    # 恢复原来的PATH
    export PATH="$OLD_PATH"
    return 0
}

if [[ $(uname -s) == MINGW* ]] || [[ "$OSTYPE" == "msys" ]]; then
    echo "Building in Windows MINGW environment..."

    # 编译 64位版本
    if ! buildMingwArchitecture "64" "$MINGW64_PATH" "Release/lib/mingw64"; then
        echo "64-bit build failed!"
        exit 1
    fi

    # 如果需要，编译 32位版本
    if [[ "$BUILD_MINGW32" == "true" ]]; then
        if ! buildMingwArchitecture "32" "$MINGW32_PATH" "Release/lib/mingw32"; then
            echo "32-bit build failed!"
            exit 1
        fi
    fi

    echo "All builds completed successfully!"
else
    echo "Unsupported environment for MINGW build"
    exit 1
fi
