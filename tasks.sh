#!/usr/bin/env bash

set -e

cd "$(dirname "$0")"
PROJECT_DIR=$(pwd)

function isWsl() {
    [[ -d "/mnt/c" ]] || command -v wslpath &>/dev/null
}

function isWindows() {
    if ! isWsl; then
        [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]] || [[ -n "$WINDIR" ]]
    else
        [[ "$PROJECT_DIR" =~ ^/mnt/ ]] && [[ -z "$BUILD_EGE_NON_WINDOWS" ]]
    fi
}

if isWsl && isWindows; then
    # Switch to Git Bash when running in WSL and in a Windows directory
    echo "Detected WSL environment in Windows directory, switching to Git Bash..."
    GIT_BASH_PATH_WIN=$(/mnt/c/Windows/system32/cmd.exe /C "where bash.exe" | grep -i Git | head -n 1 | tr -d '\n\r')
    GIT_BASH_PATH_WSL=$(wslpath -u "$GIT_BASH_PATH_WIN")
    echo "== GIT_BASH_PATH_WIN=$GIT_BASH_PATH_WIN"
    echo "== GIT_BASH_PATH_WSL=$GIT_BASH_PATH_WSL"
    if [[ -f "$GIT_BASH_PATH_WSL" ]]; then
        THIS_BASE_NAME=$(basename "$0")
        "$GIT_BASH_PATH_WSL" "$THIS_BASE_NAME" $@
        exit $?
    else
        echo "Git Bash not found, please install Git Bash!" >&2
        exit 1
    fi
fi

CMAKE_BUILD_DIR="$PROJECT_DIR/build"

export BUILD_TARGET="" # 默认只构建 xege 静态库

# 默认开Release模式
export CMAKE_BUILD_TYPE="Release"

if [[ -z "$WIN_CMAKE_BUILD_DEFINE" ]]; then
    export WIN_CMAKE_BUILD_DEFINE=""
fi

if [[ -z "$CMAKE_CONFIG_DEFINE" ]]; then
    declare -a CMAKE_CONFIG_DEFINE=(
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.13"
    )
fi

function MY_CMAKE_BUILD_DEFINE() {
    # 使用带引号的数组展开来保持参数中的空格
    local args=("-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
    args+=("${CMAKE_CONFIG_DEFINE[@]}")
    printf '%q ' "${args[@]}"
}

if ! command -v grealpath && command -v realpath; then
    function grealpath() {
        realpath $@
    }
fi

function loadCMakeProject() {
    # 构建 cmake 参数数组
    local cmake_args=("-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
    cmake_args+=("${CMAKE_CONFIG_DEFINE[@]}")
    if [[ -z "$EGE_SOURCE_PATH" ]]; then
        cmake_args+=("..")
    else
        cmake_args+=("$EGE_SOURCE_PATH")
    fi

    set -x

    mkdir -p "$CMAKE_BUILD_DIR" &&
        cd "$CMAKE_BUILD_DIR" &&
        cmake "${cmake_args[@]}"
    echo "CMake Project Loaded: CMAKE_CONFIG_DEFINE=(${CMAKE_CONFIG_DEFINE[*]})"
    set +x
}

function cmakeCleanAll() {
    pushd $PROJECT_DIR
    git clean -ffdx build
    popd
}

function reloadCMakeProject() {
    cmakeCleanAll && loadCMakeProject
}

function cmakeBuildAll() {
    pushd "$CMAKE_BUILD_DIR"

    if [[ -n "$BUILD_TARGET" ]]; then
        TARGET_RULE="--target $BUILD_TARGET"
    else
        TARGET_RULE=""
    fi

    set -x

    if ls *.sln >/dev/null 2>&1; then
        # MSVC 专属逻辑
        if [[ -n "$CMAKE_BUILD_TYPE" ]]; then
            export WIN_CMAKE_BUILD_DEFINE="$WIN_CMAKE_BUILD_DEFINE --config $CMAKE_BUILD_TYPE"
        fi

        # ref: https://stackoverflow.com/questions/11865085/out-of-a-git-console-how-do-i-execute-a-batch-file-and-then-return-to-git-conso
        cmd "/C cmake.exe --build . $TARGET_RULE $WIN_CMAKE_BUILD_DEFINE --parallel $(nproc)"

    else

        cmake --build . $TARGET_RULE $(test -n "$CMAKE_BUILD_TYPE" && echo --config $CMAKE_BUILD_TYPE) -- -j $(nproc)
    fi

    set +x
    popd
}

if [[ $# -eq 0 ]]; then
    echo "usage: [--load] [--reload] [--clean] [--build]"
fi

# 定义操作标志
DO_LOAD=false
DO_RELOAD=false
DO_CLEAN=false
DO_BUILD=false
RUN_EXECUTABLE=""

# 第一遍：解析所有参数并设置配置
while [[ $# -gt 0 ]]; do

    PARSE_KEY="$1"

    case "$PARSE_KEY" in
    --load)
        export DO_LOAD=true
        shift # past argument
        ;;
    --reload)
        export DO_RELOAD=true
        shift # past argument
        ;;
    --clean)
        export DO_CLEAN=true
        shift # past argument
        ;;
    --build)
        export DO_BUILD=true
        shift # past argument
        ;;
    --test-release-libs)
        echo "使用 ege 预编译包来编译所有 Demo..."
        export DO_TEST_RELEASE_LIBS=true
        shift # past argument
        ;;
    --build-dir)
        echo "set build dir to $2"
        if [[ "$2" == /* ]]; then
            export CMAKE_BUILD_DIR="$2"
        else
            export CMAKE_BUILD_DIR="$PROJECT_DIR/$2"
        fi
        shift
        shift
        ;;
    --debug)
        echo "enable debug mode"
        export CMAKE_BUILD_TYPE="Debug"
        shift
        ;;
    --release)
        echo "enable release mode"
        export CMAKE_BUILD_TYPE="Release"
        shift # past argument
        ;;
    --target)
        echo "set build target to $2"
        export BUILD_TARGET="$2"
        if [[ $BUILD_TARGET == "demos" ]]; then
            CMAKE_CONFIG_DEFINE+=("-DEGE_BUILD_DEMO=ON")
        fi
        shift
        shift
        ;;
    --toolset)
        echo "set toolset to $2"
        CMAKE_CONFIG_DEFINE+=("-T" "$2")
        shift
        shift
        ;;
    --arch)
        echo "set arch to $2"
        CMAKE_CONFIG_DEFINE+=("-A" "$2")
        shift
        shift
        ;;
    -G | --generator)
        echo "set generator to $2"
        CMAKE_CONFIG_DEFINE+=("-G" "$2")
        shift
        shift
        ;;
    --)
        shift
        echo "-- 后面的参数全都透传给 CMake"
        if [[ $# -gt 0 ]]; then
            CMAKE_CONFIG_DEFINE+=("$@")
            shift $#
        fi
        break
        ;;
    --run)
        RUN_EXECUTABLE="$2"
        shift
        shift
        ;;
    *)
        echo "unknown option $PARSE_KEY..."
        exit 1
        ;;
    esac
done

# 第二遍：按正确顺序执行操作

if [[ "$DO_CLEAN" == true ]]; then
    echo "DO_CLEAN is true, start cleaning..."
    cmakeCleanAll
fi

if [[ "$DO_RELOAD" == true ]]; then
    echo "DO_RELOAD is true, start reloading CMake project..."
    reloadCMakeProject
fi

if [[ "$DO_LOAD" == true ]]; then
    echo "DO_LOAD is true, start loading CMake project..."
    loadCMakeProject
fi

if [[ "$DO_BUILD" == true ]]; then
    echo "DO_BUILD is true, start building..."
    if [[ ! -f "$CMAKE_BUILD_DIR/CMakeCache.txt" ]]; then
        loadCMakeProject
    fi
    cmakeBuildAll
fi

if [[ "$DO_TEST_RELEASE_LIBS" == true ]]; then
    echo "DO_TEST_RELEASE_LIBS is true, start testing release libs..."

    if [[ "$(basename "$CMAKE_BUILD_DIR")" != *"msvc"* ]]; then
        if [[ "$CMAKE_BUILD_TYPE" == "Release" ]]; then
            export CMAKE_BUILD_DIR="${CMAKE_BUILD_DIR}-release"
        else
            export CMAKE_BUILD_DIR="${CMAKE_BUILD_DIR}-debug"
        fi
    fi

    export BUILD_TARGET="demos"
    # 添加预编译库选项
    CMAKE_CONFIG_DEFINE+=("-DEGE_BUILD_DEMO_WITH_PREBUILT_LIBS=ON")
    mkdir -p "$CMAKE_BUILD_DIR" && cd "$CMAKE_BUILD_DIR"
    if [[ ! -f "CMakeCache.txt" ]]; then
        export EGE_SOURCE_PATH="../demo"
        loadCMakeProject
    fi
    cmakeBuildAll
    echo "Build demo with release libs done."
fi

if [[ -n "$RUN_EXECUTABLE" ]]; then
    if isWindows; then
        echo "run $CMAKE_BUILD_DIR/demo/$CMAKE_BUILD_TYPE/$RUN_EXECUTABLE"
        "$CMAKE_BUILD_DIR/demo/$CMAKE_BUILD_TYPE/$RUN_EXECUTABLE"
    else
        echo run "$CMAKE_BUILD_DIR/demo/$RUN_EXECUTABLE"
        if command -v wine64 &>/dev/null; then
            wine64 "$CMAKE_BUILD_DIR/demo/$RUN_EXECUTABLE"
        elif command -v wine &>/dev/null; then
            wine "$CMAKE_BUILD_DIR/demo/$RUN_EXECUTABLE"
        else
            echo "Command 'wine64' not found, please install wine first."
        fi
    fi
fi
