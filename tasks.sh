#!/usr/bin/env bash

set -e

cd "$(dirname "$0")"
PROJECT_DIR=$(pwd)

# 缓存 WSL 检测结果
__IS_WSL_CACHE=""

function isWsl() {
    if [[ -z "$__IS_WSL_CACHE" ]]; then
        if [[ -f /proc/sys/kernel/osrelease ]] && grep -qi "Microsoft" /proc/sys/kernel/osrelease; then
            __IS_WSL_CACHE=1
        elif [[ -d "/mnt/c/WINDOWS/system32" ]] || command -v wslpath &>/dev/null; then
            __IS_WSL_CACHE=1
        else
            __IS_WSL_CACHE=0
        fi
    fi
    [[ $__IS_WSL_CACHE -eq 1 ]]
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
        "$GIT_BASH_PATH_WSL" "$THIS_BASE_NAME" "$@"
        exit $?
    else
        echo "Git Bash not found, please install Git Bash!" >&2
        exit 1
    fi
fi

# 检测是否为 MSVC 环境的函数
# 判断逻辑：在 Windows 上，除非明确指定了非 MSVC 生成器，否则默认使用 MSVC
function isMSVC() {
    local config_str="${CMAKE_CONFIG_DEFINE[*]}"

    # 如果明确指定了 MinGW/Ninja/Unix Makefiles 生成器，则不是 MSVC
    if [[ "$config_str" == *"MinGW"* ]] || [[ "$config_str" == *"Unix Makefiles"* ]] || [[ "$config_str" == *"Ninja"* ]]; then
        return 1
    fi

    # 检查是否明确指定了 Visual Studio 生成器或工具集
    if [[ "$config_str" == *"Visual Studio"* ]] || [[ "$config_str" == *"-T v1"* ]]; then
        return 0
    fi

    # 检查是否存在 Visual Studio 相关环境变量
    if [[ -n "$VSINSTALLDIR" ]] || [[ -n "$VisualStudioVersion" ]]; then
        return 0
    fi

    # Windows 环境下，如果没有指定生成器，CMake 默认使用 Visual Studio
    if isWindows; then
        return 0
    fi

    return 1
}

# 检测是否明确指定了 MinGW 生成器
function isMinGW() {
    local config_str="${CMAKE_CONFIG_DEFINE[*]}"
    if [[ "$config_str" == *"MinGW"* ]]; then
        return 0
    fi
    return 1
}

# 根据编译器类型和构建类型确定 build 目录
# MSVC: build/ (CMake 自动处理 Debug/Release 子目录)
# MinGW/其他: build/Debug 或 build/Release
EGE_BACKEND=""

function getBuildDir() {
    local base_dir="$PROJECT_DIR/build"

    # Explicit backend selections must never share a CMake cache.  This is
    # especially important on Windows where MSVC otherwise uses build/ for
    # both the GDI-only and OpenGL-capable configurations.
    if [[ -n "$EGE_BACKEND" ]]; then
        base_dir="$base_dir/$EGE_BACKEND"
    fi

    # 如果用户已经通过 --build-dir 指定了目录，使用用户指定的
    if [[ -n "$USER_SPECIFIED_BUILD_DIR" ]]; then
        echo "$USER_SPECIFIED_BUILD_DIR"
        return
    fi

    # MSVC 使用单一 build 目录（CMake 会自动创建 Debug/Release 子目录）
    if isMSVC; then
        echo "$base_dir"
        return
    fi

    # MinGW 和其他编译器需要手动区分 Debug/Release 目录
    echo "$base_dir/$CMAKE_BUILD_TYPE"
}

CMAKE_BUILD_DIR=""          # 延迟初始化，在解析完所有参数后设置
USER_SPECIFIED_BUILD_DIR="" # 用户通过 --build-dir 指定的目录

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
        realpath "$@"
    }
fi

function loadCMakeProject() {
    # 构建 cmake 参数数组
    local cmake_args=("-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
    cmake_args+=("${CMAKE_CONFIG_DEFINE[@]}")

    # 确定源代码路径（相对于 build 目录）
    local source_path
    if [[ -z "$EGE_SOURCE_PATH" ]]; then
        # 计算从 CMAKE_BUILD_DIR 到 PROJECT_DIR 的相对路径
        # 如果是 build/Debug 或 build/Release，则需要 ../..
        # 如果是 build，则需要 ..
        if [[ "$CMAKE_BUILD_DIR" == "$PROJECT_DIR/build" ]]; then
            source_path=".."
        elif [[ "$CMAKE_BUILD_DIR" == "$PROJECT_DIR/build/"* ]]; then
            source_path="../.."
        else
            # 用户自定义路径，使用绝对路径
            source_path="$PROJECT_DIR"
        fi
    else
        source_path="$EGE_SOURCE_PATH"
    fi
    cmake_args+=("$source_path")

    set -x

    mkdir -p "$CMAKE_BUILD_DIR" &&
        cd "$CMAKE_BUILD_DIR" &&
        cmake "${cmake_args[@]}"
    local configure_status=$?
    if [[ $configure_status -ne 0 ]]; then
        set +x
        return $configure_status
    fi
    if ! verifyBackendSelection; then
        set +x
        return 1
    fi
    echo "CMake Project Loaded: CMAKE_CONFIG_DEFINE=(${CMAKE_CONFIG_DEFINE[*]})"
    set +x
}

function cmakeCleanAll() {
    local resolved_project_dir
    local resolved_build_root
    local resolved_build_dir
    local clean_target

    resolved_project_dir="$(grealpath -m -- "$PROJECT_DIR")"
    resolved_build_root="$(grealpath -m -- "$PROJECT_DIR/build")"
    resolved_build_dir="$(grealpath -m -- "$CMAKE_BUILD_DIR")"

    case "$resolved_build_dir" in
        "$resolved_build_root"|"$resolved_build_root"/*)
            ;;
        *)
            echo "Error: refusing to clean outside $resolved_build_root: $resolved_build_dir" >&2
            return 1
            ;;
    esac

    clean_target="${resolved_build_dir#"$resolved_project_dir"/}"
    echo "Cleaning selected build path: $resolved_build_dir"
    pushd "$resolved_project_dir"
    git clean -ffdx -- "$clean_target"
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

    if compgen -G "*.sln" >/dev/null 2>&1 || compgen -G "*.slnx" >/dev/null 2>&1; then
        # MSVC 专属逻辑
        if [[ -n "$CMAKE_BUILD_TYPE" ]]; then
            export WIN_CMAKE_BUILD_DEFINE="$WIN_CMAKE_BUILD_DEFINE --config $CMAKE_BUILD_TYPE"
        fi

        # ref: https://stackoverflow.com/questions/11865085/out-of-a-git-console-how-do-i-execute-a-batch-file-and-then-return-to-git-conso
        cmd //C "cmake.exe --build . $TARGET_RULE $WIN_CMAKE_BUILD_DEFINE --parallel $(getconf _NPROCESSORS_ONLN)"
    else
        cmake --build . $TARGET_RULE $(test -n "$CMAKE_BUILD_TYPE" && echo --config $CMAKE_BUILD_TYPE) --parallel "$(getconf _NPROCESSORS_ONLN)"
    fi

    set +x
    popd
}

function cmakeCacheGet() {
    local var="$1"
    local cache_file="$CMAKE_BUILD_DIR/CMakeCache.txt"
    if [[ -f "$cache_file" ]]; then
        # Format: VAR:TYPE=VALUE
        # Keep the last match in case of duplicates.
        grep -E "^${var}:" "$cache_file" | tail -n 1 | cut -d= -f2-
    fi
}

function verifyBackendSelection() {
    if [[ -z "$EGE_BACKEND" ]]; then
        return
    fi

    local expected_opengl="OFF"
    if [[ "$EGE_BACKEND" == "opengl" ]]; then
        expected_opengl="ON"
    fi

    local configured_opengl
    configured_opengl="$(cmakeCacheGet EGE_BUILD_OPENGL)"
    if [[ "$configured_opengl" != "$expected_opengl" ]]; then
        echo "Error: --$EGE_BACKEND requested EGE_BUILD_OPENGL=$expected_opengl," >&2
        echo "but CMake configured $configured_opengl. Check dev.cmake or forwarded options." >&2
        return 1
    fi
}

function printUsage() {
    cat <<'EOF'
Usage: bash -l tasks.sh [options] [-- <cmake-options...>]

Configure and build Xege using a platform-appropriate CMake generator.

Actions:
  --load                 Configure the selected build directory
  --reload               Clean and configure the selected build directory
  --clean                Remove only the selected build directory
  --build                Build after configuring automatically when needed
  --run <executable>     Run a built demo executable
  --test-release-libs    Build demos against the packaged Ege libraries

Build selection:
  --debug                Use the Debug configuration
  --release              Use the Release configuration (default)
  --gdi                  Select the Windows-compatible GDI build
  --opengl               Build OpenGL support (Windows apps opt in with INIT_OPENGL)
  --build-dir <path>     Override the default build directory
  --target <name>        Build one CMake target, for example xege or demos
  -G, --generator <name> Select a CMake generator
  --toolset <name>       Select an MSVC toolset
  --arch <name>          Select a CMake target architecture

Other:
  -h, --help             Show this help and exit
  -- <cmake-options...>  Forward remaining options to CMake

Examples:
  bash -l tasks.sh --debug --target xege --load --build
  bash -l tasks.sh --gdi --release --target demos --build
  bash -l tasks.sh --opengl --release --target demos --build
  bash -l tasks.sh --debug --target demos --build
  bash -l tasks.sh --debug --build-dir build/native-debug --load --build -- \
    -DEGE_BUILD_TEST=ON -DEGE_ENABLE_CAMERA_CAPTURE=ON
EOF
}

if [[ $# -eq 0 ]]; then
    printUsage
    exit 0
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
    -h | --help)
        printUsage
        exit 0
        ;;
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
        if [[ -z "$2" ]]; then
            echo "Error: --build-dir requires a directory path argument" >&2
            exit 1
        fi
        echo "set build dir to $2"
        if [[ "$2" == /* ]]; then
            export USER_SPECIFIED_BUILD_DIR="$2"
        else
            export USER_SPECIFIED_BUILD_DIR="$PROJECT_DIR/$2"
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
    --gdi)
        if [[ -n "$EGE_BACKEND" && "$EGE_BACKEND" != "gdi" ]]; then
            echo "Error: --gdi and --opengl cannot be used together" >&2
            exit 1
        fi
        echo "select GDI backend build"
        export EGE_BACKEND="gdi"
        shift
        ;;
    --opengl)
        if [[ -n "$EGE_BACKEND" && "$EGE_BACKEND" != "opengl" ]]; then
            echo "Error: --gdi and --opengl cannot be used together" >&2
            exit 1
        fi
        echo "select OpenGL backend build"
        export EGE_BACKEND="opengl"
        shift
        ;;
    --target)
        if [[ -z "$2" ]]; then
            echo "Error: --target requires a target name argument" >&2
            exit 1
        fi
        echo "set build target to $2"
        export BUILD_TARGET="$2"
        if [[ $BUILD_TARGET == "demos" ]]; then
            CMAKE_CONFIG_DEFINE+=("-DEGE_BUILD_DEMO=ON")
        fi
        shift
        shift
        ;;
    --toolset)
        if [[ -z "$2" ]]; then
            echo "Error: --toolset requires a toolset name argument" >&2
            exit 1
        fi
        echo "set toolset to $2"
        CMAKE_CONFIG_DEFINE+=("-T" "$2")
        shift
        shift
        ;;
    --arch)
        if [[ -z "$2" ]]; then
            echo "Error: --arch requires an architecture argument" >&2
            exit 1
        fi
        echo "set arch to $2"
        CMAKE_CONFIG_DEFINE+=("-A" "$2")
        shift
        shift
        ;;
    -G | --generator)
        if [[ -z "$2" ]]; then
            echo "Error: --generator requires a generator name argument" >&2
            exit 1
        fi
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
        if [[ -z "$2" ]]; then
            echo "Error: --run requires an executable name argument" >&2
            exit 1
        fi
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

# Keep the explicit backend authoritative even when conflicting -D options
# were forwarded earlier.  System GLFW remains an available explicit opt-out.
if [[ "$EGE_BACKEND" == "gdi" ]]; then
    CMAKE_CONFIG_DEFINE+=("-DEGE_BUILD_OPENGL=OFF")
elif [[ "$EGE_BACKEND" == "opengl" ]]; then
    config_str="${CMAKE_CONFIG_DEFINE[*]}"
    if isWindows && [[ "$config_str" != *"EGE_USE_BUNDLED_GLFW"* ]]; then
        CMAKE_CONFIG_DEFINE+=("-DEGE_USE_BUNDLED_GLFW=ON")
    fi
    CMAKE_CONFIG_DEFINE+=("-DEGE_BUILD_OPENGL=ON")
fi

# 参数解析完成后，初始化 CMAKE_BUILD_DIR
CMAKE_BUILD_DIR="$(getBuildDir)"
export CMAKE_BUILD_DIR
echo "Build directory: $CMAKE_BUILD_DIR (BUILD_TYPE: $CMAKE_BUILD_TYPE)"

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

    # 对于测试预编译库的场景，使用独立的 build 目录前缀
    if [[ "$(basename "$CMAKE_BUILD_DIR")" != *"msvc"* ]]; then
        # 非 MSVC 环境，目录名包含构建类型
        _base_name=$(basename "$CMAKE_BUILD_DIR")
        _dir_name=$(dirname "$CMAKE_BUILD_DIR")
        if [[ "$_base_name" == "Debug" || "$_base_name" == "Release" ]]; then
            # 已经是 build/Debug 或 build/Release 格式
            export CMAKE_BUILD_DIR="${_dir_name}-demo/${_base_name}"
        else
            # 旧格式或自定义目录
            if [[ "$CMAKE_BUILD_TYPE" == "Release" ]]; then
                export CMAKE_BUILD_DIR="${CMAKE_BUILD_DIR}-release"
            else
                export CMAKE_BUILD_DIR="${CMAKE_BUILD_DIR}-debug"
            fi
        fi
    fi

    export BUILD_TARGET="demos"
    # 添加预编译库选项
    CMAKE_CONFIG_DEFINE+=("-DEGE_BUILD_DEMO_WITH_PREBUILT_LIBS=ON")
    mkdir -p "$CMAKE_BUILD_DIR" && cd "$CMAKE_BUILD_DIR"
    if [[ ! -f "CMakeCache.txt" ]]; then
        export EGE_SOURCE_PATH="$PROJECT_DIR/demo"
        loadCMakeProject
    fi
    cmakeBuildAll
    echo "Build demo with release libs done."

    if cd "$PROJECT_DIR/Release"; then
        CMAKE_BUILD_DIR_BASE_NAME=$(basename "$CMAKE_BUILD_DIR")
        OUTPUT_BASE_DIR=${CMAKE_BUILD_DIR_BASE_NAME/build-/}
        OUTPUT_DIR="$PROJECT_DIR/Release/bin/$OUTPUT_BASE_DIR"
        mkdir -p "$OUTPUT_DIR"
        echo "Copying executables to $OUTPUT_DIR"
        cd "$CMAKE_BUILD_DIR"
        # 找到 $CMAKE_BUILD_DIR 里面的所有 exe 文件, 保持相对于 $CMAKE_BUILD_DIR 的目录结构, 并复制到 $OUTPUT_DIR 目录
        find . -maxdepth 2 -type f -name "*.exe" -print0 | while IFS= read -r -d '' file; do
            relative_path="${file#./}"
            mkdir -p "$OUTPUT_DIR/$(dirname "$relative_path")"
            cp "$file" "$OUTPUT_DIR/$relative_path"
        done
    else
        echo "Release directory does not exist, skipping copy."
    fi
fi

if [[ -n "$RUN_EXECUTABLE" ]]; then
    # 根据编译器类型确定可执行文件路径
    exe_path=""
    if isMSVC; then
        # MSVC: demo/<BuildType>/xxx.exe
        exe_path="$CMAKE_BUILD_DIR/demo/$CMAKE_BUILD_TYPE/$RUN_EXECUTABLE"
    else
        # MinGW/其他: demo/xxx.exe (因为已经在 build/Debug 或 build/Release 目录下了)
        exe_path="$CMAKE_BUILD_DIR/demo/$RUN_EXECUTABLE"
    fi

    if isWindows; then
        echo "run $exe_path"
        "$exe_path"
    else
        cache_opengl="$(cmakeCacheGet EGE_BUILD_OPENGL)"

        # In native OpenGL builds on Unix, demos are native binaries (usually without .exe).
        # VS Code tasks historically pass *.exe; auto-map to the native name to keep tasks stable.
        if [[ "$cache_opengl" == "ON" ]] && [[ "$exe_path" == *.exe ]]; then
            native_path="${exe_path%.exe}"
            if [[ -f "$native_path" ]]; then
                exe_path="$native_path"
            fi
        fi

        echo run "$exe_path"
        # If the output is a native binary (no .exe suffix), run it directly.
        # Otherwise, fall back to wine for the legacy cross-compile workflow.
        if [[ "$exe_path" != *.exe ]]; then
            "$exe_path"
        elif [[ "$cache_opengl" == "ON" ]]; then
            echo "Error: This build directory is configured with EGE_BUILD_OPENGL=ON (native OpenGL)." >&2
            echo "Refusing to run Windows .exe via wine. Please run the native binary (no .exe suffix)." >&2
            exit 2
        elif command -v wine64 &>/dev/null; then
            wine64 "$exe_path"
        elif command -v wine &>/dev/null; then
            wine "$exe_path"
        else
            echo "Command 'wine64' not found, please install wine first." >&2
            echo "Tip: On Linux/macOS you can also rebuild with -DEGE_BUILD_OPENGL=ON to run native demos without wine." >&2
            exit 127
        fi
    fi
fi
