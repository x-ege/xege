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

function isMacOS() {
    [[ "$(uname -s)" == "Darwin" ]]
}

# A macOS host builds a native Mach-O target unless the caller explicitly
# supplies a cross-compilation toolchain/system. Keep this decision independent
# from the presence of mingw-w64 or Wine on the developer machine.
function isNativeMacOS() {
    isMacOS &&
        ! hasCMakeDefinition "CMAKE_TOOLCHAIN_FILE" &&
        ! cmakeDefinitionEquals "CMAKE_SYSTEM_NAME" "Windows"
}

function isNativeLinux() {
    [[ "$(uname -s)" == "Linux" ]] && ! isWindows &&
        ! hasCMakeDefinition "CMAKE_TOOLCHAIN_FILE" &&
        ! cmakeDefinitionEquals "CMAKE_SYSTEM_NAME" "Windows"
}

function hasCMakeDefinition() {
    local definition="$1"
    local argument
    for argument in "${CMAKE_CONFIG_DEFINE[@]}"; do
        case "$argument" in
        -D"${definition}"=* | -D"${definition}":*=*)
            return 0
            ;;
        esac
    done
    return 1
}

function cmakeDefinitionEquals() {
    local definition="$1"
    local expected_value="$2"
    local argument
    local value
    for argument in "${CMAKE_CONFIG_DEFINE[@]}"; do
        case "$argument" in
        -D"${definition}"=* | -D"${definition}":*=*)
            value="${argument#*=}"
            if [[ "$value" == "$expected_value" ]]; then
                return 0
            fi
            ;;
        esac
    done
    return 1
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
function getBuildDir() {
    local base_dir="${EGE_BUILD_ROOT:-$PROJECT_DIR/build}"

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

    # Never reuse a historical MinGW/Windows CMake cache for native macOS.
    # The platform component also makes the output type obvious in VS Code.
    if isNativeMacOS; then
        echo "$base_dir/macos/$CMAKE_BUILD_TYPE"
        return
    fi
    if isNativeLinux; then
        echo "$base_dir/linux/$CMAKE_BUILD_TYPE"
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

if ! command -v grealpath >/dev/null 2>&1 && command -v realpath >/dev/null 2>&1; then
    function grealpath() {
        realpath $@
    }
fi

function loadCMakeProject() {
    # 构建 cmake 参数数组
    local cmake_args=("-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
    cmake_args+=("${CMAKE_CONFIG_DEFINE[@]}")

    # Always pass an absolute source path. Build directories are intentionally
    # platform-specific (for example build/macos/Debug), so deriving the source
    # with a fixed number of '..' components is both fragile and unnecessary.
    local source_path="${EGE_SOURCE_PATH:-$PROJECT_DIR}"
    case "$source_path" in
    /* | [A-Za-z]:[\\/]*)
        ;;
    *)
        source_path="$PROJECT_DIR/$source_path"
        ;;
    esac

    # Resolve relative components before changing into the build directory.
    # CMake must receive a stable source directory even when EGE_SOURCE_PATH is
    # supplied as ".", "demo", or another path relative to the checkout.
    if [[ "$source_path" != [A-Za-z]:[\\/]* ]]; then
        if [[ ! -d "$source_path" ]]; then
            echo "Error: EGE source directory does not exist: $source_path" >&2
            return 1
        fi
        source_path="$(cd "$source_path" && pwd -P)"
    fi
    cmake_args+=("$source_path")

    set -x

    mkdir -p "$CMAKE_BUILD_DIR" &&
        cd "$CMAKE_BUILD_DIR" &&
        cmake "${cmake_args[@]}"
    echo "CMake Project Loaded: CMAKE_CONFIG_DEFINE=(${CMAKE_CONFIG_DEFINE[*]})"
    set +x
}

function cmakeCleanAll() {
    if [[ -z "$CMAKE_BUILD_DIR" || "$CMAKE_BUILD_DIR" == "/" ||
          "$CMAKE_BUILD_DIR" == "$PROJECT_DIR" ]]; then
        echo "Error: refusing unsafe build-directory cleanup: $CMAKE_BUILD_DIR" >&2
        return 1
    fi

    # --clean/--reload remove only the selected configuration directory. If a
    # custom non-empty directory is supplied, require proof that it is an EGE
    # CMake tree before deleting it.
    if [[ -d "$CMAKE_BUILD_DIR" && -n "$(ls -A "$CMAKE_BUILD_DIR" 2>/dev/null)" ]]; then
        local cache_file="$CMAKE_BUILD_DIR/CMakeCache.txt"
        local cache_home=""
        local canonical_cache_home=""
        local canonical_project_dir=""
        if [[ -f "$cache_file" ]]; then
            cache_home=$(sed -n \
                's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$cache_file" | head -n 1)
        fi
        # CMake may store a physical path or a Windows drive path while Git
        # Bash exposes the checkout through a symlink or /c/... spelling.
        if [[ -n "$cache_home" ]] && command -v cygpath >/dev/null 2>&1; then
            cache_home=$(cygpath -u "$cache_home" 2>/dev/null || printf '%s' "$cache_home")
        fi
        if [[ -n "$cache_home" && -d "$cache_home" ]]; then
            canonical_cache_home=$(cd "$cache_home" && pwd -P)
        fi
        canonical_project_dir=$(cd "$PROJECT_DIR" && pwd -P)
        if [[ -z "$canonical_cache_home" ||
              "$canonical_cache_home" != "$canonical_project_dir" ]]; then
            echo "Error: refusing to remove an unrecognized build directory:" >&2
            echo "  $CMAKE_BUILD_DIR" >&2
            return 1
        fi
    fi

    echo "Removing selected CMake build directory: $CMAKE_BUILD_DIR"
    cmake -E remove_directory "$CMAKE_BUILD_DIR"
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

function runCTest() {
    if [[ ! -f "$CMAKE_BUILD_DIR/CMakeCache.txt" ]]; then
        echo "Error: CMake project is not configured: $CMAKE_BUILD_DIR" >&2
        return 1
    fi
    # --test-dir was added after the project's CMake/CTest 3.13 minimum.
    # Running from a subshell preserves compatibility without changing the
    # caller's working directory.
    (
        cd "$CMAKE_BUILD_DIR"
        ctest --build-config "$CMAKE_BUILD_TYPE" --output-on-failure
    )
}

function printUsage() {
    echo "usage: tasks.sh [--debug|--release] [--load|--reload|--clean] [--build] [--test]"
    echo "                [--target name] [--run name] [--build-dir path]"
    echo "                [--toolset name] [--arch name] [-G|--generator name]"
    echo "                [--show-config] [--test-release-libs] [-- cmake-options...]"
    echo "                [-h|--help]"
    echo "  --clean/--reload remove only the selected CMake build directory."
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
DO_TEST=false
DO_SHOW_CONFIG=false
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
    --test)
        export DO_TEST=true
        shift
        ;;
    --show-config)
        export DO_SHOW_CONFIG=true
        shift
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

# CMake AUTO already resolves to CoreGraphics on Darwin. Make the VS Code/script
# contract explicit as well, so an installed MinGW compiler cannot influence a
# normal macOS invocation and OpenGL remains opt-in.
if isNativeMacOS; then
    if ! hasCMakeDefinition "EGE_DEFAULT_BACKEND"; then
        CMAKE_CONFIG_DEFINE+=("-DEGE_DEFAULT_BACKEND=COREGRAPHICS")
    fi
    if ! hasCMakeDefinition "EGE_ENABLE_OPENGL"; then
        CMAKE_CONFIG_DEFINE+=("-DEGE_ENABLE_OPENGL=OFF")
    fi
    if ! hasCMakeDefinition "EGE_ENABLE_WINDOW_TESTS"; then
        CMAKE_CONFIG_DEFINE+=("-DEGE_ENABLE_WINDOW_TESTS=OFF")
    fi
    if ! hasCMakeDefinition "CMAKE_OSX_DEPLOYMENT_TARGET"; then
        CMAKE_CONFIG_DEFINE+=("-DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=11.0")
    fi
fi

if isNativeLinux; then
    if ! hasCMakeDefinition "EGE_DEFAULT_BACKEND"; then
        CMAKE_CONFIG_DEFINE+=("-DEGE_DEFAULT_BACKEND=CAIRO")
    fi
    if ! hasCMakeDefinition "EGE_ENABLE_OPENGL"; then
        CMAKE_CONFIG_DEFINE+=("-DEGE_ENABLE_OPENGL=OFF")
    fi
    if ! hasCMakeDefinition "EGE_ENABLE_WINDOW_TESTS"; then
        CMAKE_CONFIG_DEFINE+=("-DEGE_ENABLE_WINDOW_TESTS=OFF")
    fi
fi

# 参数解析完成后，初始化 CMAKE_BUILD_DIR
CMAKE_BUILD_DIR="$(getBuildDir)"
export CMAKE_BUILD_DIR
echo "Build directory: $CMAKE_BUILD_DIR (BUILD_TYPE: $CMAKE_BUILD_TYPE)"
if isNativeMacOS; then
    echo "macOS native build: AppleClang/CoreGraphics (headless tests by default)"
elif isNativeLinux; then
    echo "Linux native build: Cairo/Xlib (headless tests by default)"
fi

if [[ "$DO_SHOW_CONFIG" == true ]]; then
    printf 'CMake configure arguments:'
    printf ' %q' "${CMAKE_CONFIG_DEFINE[@]}"
    printf '\n'
fi

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

if [[ "$DO_TEST" == true ]]; then
    echo "DO_TEST is true, start running CTest..."
    runCTest
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
        if isNativeMacOS || isNativeLinux; then
            # Native package demos are extensionless executables. Exclude
            # CMake probes and metadata.
            find . -maxdepth 2 -type f -perm -111 -print0 |
                while IFS= read -r -d '' file; do
                    [[ "$file" == */CMakeFiles/* ]] && continue
                    if isNativeMacOS; then
                        file "$file" | grep -q "Mach-O.*executable" || continue
                    else
                        file "$file" | grep -q "ELF.*executable" || continue
                    fi
                    relative_path="${file#./}"
                    mkdir -p "$OUTPUT_DIR/$(dirname "$relative_path")"
                    cp "$file" "$OUTPUT_DIR/$relative_path"
                done
        else
            # Preserve the relative layout of Windows demo executables.
            find . -maxdepth 2 -type f -name "*.exe" -print0 |
                while IFS= read -r -d '' file; do
                    relative_path="${file#./}"
                    mkdir -p "$OUTPUT_DIR/$(dirname "$relative_path")"
                    cp "$file" "$OUTPUT_DIR/$relative_path"
                done
        fi
    else
        echo "Release directory does not exist, skipping copy."
    fi
fi

if [[ -n "$RUN_EXECUTABLE" ]]; then
    # 根据编译器类型确定可执行文件路径
    exe_path=""
    if isMSVC; then
        # MSVC: demo/<BuildType>/xxx.exe
        [[ "$RUN_EXECUTABLE" == *.exe ]] || RUN_EXECUTABLE="${RUN_EXECUTABLE}.exe"
        exe_path="$CMAKE_BUILD_DIR/demo/$CMAKE_BUILD_TYPE/$RUN_EXECUTABLE"
    else
        if isNativeMacOS || isNativeLinux; then
            # Native CMake targets are extensionless executables.
            RUN_EXECUTABLE="${RUN_EXECUTABLE%.exe}"
        elif [[ "$RUN_EXECUTABLE" != *.exe ]] && ! isWindows; then
            # Non-native Unix invocations historically cross-compile Windows.
            RUN_EXECUTABLE="${RUN_EXECUTABLE}.exe"
        fi
        exe_path="$CMAKE_BUILD_DIR/demo/$RUN_EXECUTABLE"
    fi

    if isWindows || isNativeMacOS || isNativeLinux; then
        echo "run $exe_path"
        "$exe_path"
    else
        echo run "$exe_path"
        if command -v wine64 &>/dev/null; then
            wine64 "$exe_path"
        elif command -v wine &>/dev/null; then
            wine "$exe_path"
        else
            echo "Command 'wine64' not found, please install wine first."
        fi
    fi
fi
