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

MINGW64_BIN="$MINGW64_PATH/bin"

if [[ $(uname -s) == MINGW* ]] || [[ "$OSTYPE" == "msys" ]]; then
    echo "Building in Windows MINGW environment..."

    # 检查 MINGW64 路径是否存在
    if [[ ! -d "$MINGW64_BIN" ]]; then
        echo "Error: MINGW64 not found at $MINGW64_PATH"
        echo "Please check if Dev-Cpp is installed and MINGW64 is configured correctly."
        exit 1
    fi

    # 添加 MINGW64 到 PATH
    export PATH="$MINGW64_BIN:$PATH"

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

# 根据环境设置 CMake 参数
# Windows MINGW 本地编译配置
echo "Configuring CMake for native MINGW compilation..."

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

    echo "Build successful!"

    # 显示生成的文件
    echo "Generated files:"
    mkdir -p Release/lib/mingw64
    find build -type f -name "*.a" -exec cp {} Release/lib/mingw64/ \;
    ls -l Release/lib/mingw64

    ./utils/test-release-libs.sh \
        --build-dir "build-mingw-windows" \
        -- \
        -G "MinGW Makefiles" \
        -DCMAKE_C_COMPILER="$CC" \
        -DCMAKE_CXX_COMPILER="$CXX" \
        -DCMAKE_RC_COMPILER="$RC" \
        -DCMAKE_MAKE_PROGRAM="$(which mingw32-make)"
else
    echo "CMake configuration failed!"
    exit 1
fi
