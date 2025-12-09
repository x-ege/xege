#!/usr/bin/env bash

set -e

cd "$(dirname "$0")"
TESTS_DIR=$(pwd)
PROJECT_DIR="$(dirname "$TESTS_DIR")"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 配置变量
BUILD_TYPE="Debug"
REBUILD=false
VERBOSE=false
LIST_TESTS=false
RUN_SPECIFIC_TEST=""
BUILD_DIR=""
SKIP_BUILD=false
STOP_ON_FAIL=true

# 测试列表
declare -a TEST_CASES=(
    "putimage_performance_test"
    "putimage_basic_test"
    "putimage_alphablend_test"
    "putimage_transparent_test"
    "putimage_rotate_test"
    "putimage_comparison_test"
    "putimage_alphablend_comprehensive_test"
)

# 打印帮助信息
function print_help() {
    cat <<EOF
${BLUE}EGE 测试运行脚本${NC}

用法: ./run_tests.sh [选项]

选项:
    -h, --help                  显示此帮助信息
    -r, --rebuild               强制重新构建测试用例（不使用缓存）
    -d, --debug                 构建 Debug 版本（默认）
    -R, --release               构建 Release 版本
    -v, --verbose               详细输出模式
    -t, --test <name>           运行指定的测试用例
    -l, --list                  列出所有可用的测试用例
    -b, --build-dir <dir>       指定构建目录
    --skip-build                跳过构建步骤，仅运行已有的测试二进制文件
    --continue-on-fail          继续运行其他测试（即使某个测试失败）

示例:
    # 默认行为：构建并运行所有测试
    ./run_tests.sh

    # 强制重新构建
    ./run_tests.sh --rebuild

    # 运行特定测试
    ./run_tests.sh --test putimage_basic_test

    # 构建 Release 版本并运行所有测试
    ./run_tests.sh --release

    # 详细输出并继续运行（即使失败）
    ./run_tests.sh --verbose --continue-on-fail

    # 列出所有可用的测试用例
    ./run_tests.sh --list

EOF
}

# 列出所有可用的测试用例
function list_tests() {
    echo -e "${BLUE}可用的测试用例:${NC}"
    echo ""
    for i in "${!TEST_CASES[@]}"; do
        echo "  $((i + 1)). ${TEST_CASES[$i]}"
    done
    echo ""
}

# 打印日志信息
function log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

function log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

function log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

function log_section() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}\n"
}

# 解析命令行参数
function parse_arguments() {
    while [[ $# -gt 0 ]]; do
        case $1 in
        -h | --help)
            print_help
            exit 0
            ;;
        -r | --rebuild)
            REBUILD=true
            shift
            ;;
        -d | --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -R | --release)
            BUILD_TYPE="Release"
            shift
            ;;
        -v | --verbose)
            VERBOSE=true
            shift
            ;;
        -t | --test)
            RUN_SPECIFIC_TEST="$2"
            shift 2
            ;;
        -l | --list)
            LIST_TESTS=true
            shift
            ;;
        -b | --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --skip-build)
            SKIP_BUILD=true
            shift
            ;;
        --continue-on-fail)
            STOP_ON_FAIL=false
            shift
            ;;
        *)
            log_error "未知的参数: $1"
            print_help
            exit 1
            ;;
        esac
    done
}

# 获取构建目录
function get_build_dir() {
    if [[ -n "$BUILD_DIR" ]]; then
        echo "$BUILD_DIR"
        return
    fi

    # 从 tasks.sh 中读取 BUILD_DIR 的逻辑，或者使用默认值
    echo "$PROJECT_DIR/build"
}

# 获取二进制输出目录
function get_bin_dir() {
    local build_dir=$(get_build_dir)
    
    # 检测是否为 MSVC（Windows 上，build 目录为单一目录）
    # MSVC 生成器会在 bin 目录下创建 Debug/Release 子目录
    if [[ -d "$build_dir/bin/$BUILD_TYPE" ]]; then
        # MSVC 模式：build/bin/Debug 或 build/bin/Release
        echo "$build_dir/bin/$BUILD_TYPE"
    elif [[ -d "$build_dir/bin" ]]; then
        # 多目录模式（MinGW/Unix）：build/bin
        echo "$build_dir/bin"
    else
        # 回退到旧路径
        echo "$build_dir/$BUILD_TYPE/bin"
    fi
}

# 检查测试用例是否有效
function validate_test_case() {
    local test_name="$1"
    for test in "${TEST_CASES[@]}"; do
        if [[ "$test" == "$test_name" ]]; then
            return 0
        fi
    done
    return 1
}

# 构建测试
function build_tests() {
    log_section "构建测试用例"

    local build_dir=$(get_build_dir)

    # 如果启用了重新构建，删除构建目录
    if [[ "$REBUILD" == true ]]; then
        if [[ -d "$build_dir" ]]; then
            log_info "删除现有的构建目录: $build_dir"
            rm -rf "$build_dir"
        fi
    fi

    # 创建构建目录
    mkdir -p "$build_dir"
    cd "$build_dir"

    # 运行 CMake 配置
    log_info "配置项目 (构建类型: $BUILD_TYPE)..."
    if [[ "$VERBOSE" == true ]]; then
        cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" ..
    else
        cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" .. >/dev/null 2>&1
    fi

    # 构建测试
    log_info "构建测试用例..."
    if [[ "$VERBOSE" == true ]]; then
        cmake --build . --config "$BUILD_TYPE"
    else
        cmake --build . --config "$BUILD_TYPE" >/dev/null 2>&1
    fi

    if [[ $? -eq 0 ]]; then
        log_info "构建成功！"
    else
        log_error "构建失败！"
        exit 1
    fi
}

# 运行单个测试
function run_single_test() {
    local test_name="$1"
    local bin_dir=$(get_bin_dir)
    local test_exe="$bin_dir/$test_name"

    # 对于 Windows，添加 .exe 扩展名
    if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]] || [[ -n "$WINDIR" ]]; then
        test_exe="${test_exe}.exe"
    fi

    if [[ ! -f "$test_exe" ]]; then
        log_error "测试二进制文件不存在: $test_exe"
        return 1
    fi

    log_info "运行测试: $test_name"

    if [[ "$VERBOSE" == true ]]; then
        echo -e "${YELLOW}执行: $test_exe${NC}"
        "$test_exe"
    else
        "$test_exe"
    fi

    local exit_code=$?
    if [[ $exit_code -eq 0 ]]; then
        echo -e "${GREEN}✓ $test_name 通过${NC}"
    else
        echo -e "${RED}✗ $test_name 失败 (退出码: $exit_code)${NC}"
    fi

    return $exit_code
}

# 运行所有测试
function run_all_tests() {
    log_section "运行测试用例"

    local passed=0
    local failed=0
    local failed_tests=()

    for test in "${TEST_CASES[@]}"; do
        echo ""
        if run_single_test "$test"; then
            ((passed++))
        else
            ((failed++))
            failed_tests+=("$test")

            if [[ "$STOP_ON_FAIL" == true ]]; then
                log_error "测试失败，停止运行后续测试。使用 --continue-on-fail 继续运行。"
                break
            fi
        fi
    done

    # 打印测试总结
    log_section "测试总结"
    echo -e "通过: ${GREEN}$passed${NC}"
    echo -e "失败: ${RED}$failed${NC}"
    echo -e "总计: $((passed + failed))"

    if [[ $failed -gt 0 ]]; then
        echo ""
        echo -e "${RED}失败的测试:${NC}"
        for test in "${failed_tests[@]}"; do
            echo "  - $test"
        done
        return 1
    fi

    return 0
}

# 运行指定的测试
function run_specific_tests() {
    local test_name="$1"

    if ! validate_test_case "$test_name"; then
        log_error "未知的测试用例: $test_name"
        echo ""
        list_tests
        exit 1
    fi

    log_section "运行指定的测试用例"

    if run_single_test "$test_name"; then
        return 0
    else
        return 1
    fi
}

# 主函数
function main() {
    log_section "EGE 测试运行器"

    # 解析参数
    parse_arguments "$@"

    # 如果列出测试用例，直接输出并退出
    if [[ "$LIST_TESTS" == true ]]; then
        list_tests
        exit 0
    fi

    # 检查项目目录
    if [[ ! -f "$PROJECT_DIR/CMakeLists.txt" ]]; then
        log_error "项目根目录不存在或 CMakeLists.txt 不存在"
        exit 1
    fi

    log_info "项目目录: $PROJECT_DIR"
    log_info "测试目录: $TESTS_DIR"
    log_info "构建类型: $BUILD_TYPE"
    echo ""

    # 构建测试（除非跳过构建）
    if [[ "$SKIP_BUILD" != true ]]; then
        build_tests
        echo ""
    else
        log_info "跳过构建步骤"
        echo ""
    fi

    # 运行测试
    if [[ -n "$RUN_SPECIFIC_TEST" ]]; then
        run_specific_tests "$RUN_SPECIFIC_TEST"
    else
        run_all_tests
    fi

    # 根据测试结果返回退出码
    if [[ $? -eq 0 ]]; then
        log_section "✓ 所有测试都已通过！"
        exit 0
    else
        log_section "✗ 部分测试失败"
        exit 1
    fi
}

# 执行主函数
main "$@"
