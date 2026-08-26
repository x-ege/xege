#!/usr/bin/env bash

# 自动化测试 Demo 可执行文件
# 每个 Demo 运行最多 5 秒，超时后自动终止
# 如果 Demo 在超时前异常退出，则视为错误

set -e

cd "$(dirname "$0")/.." || exit 1

RELEASE_DIR="$(pwd)/Release"
TIMEOUT_SECONDS=5

# 检查 Release 目录是否存在
if [[ ! -d "$RELEASE_DIR" ]]; then
    echo "Error: Release directory not found at $RELEASE_DIR"
    echo "Please run the release script first to generate executables:"
    echo "  ./utils/release.sh"
    exit 1
fi

# 搜索所有 exe 文件
mapfile -t EXE_FILES < <(find "$RELEASE_DIR" -name "*.exe" -type f 2>/dev/null | sort)

if [[ ${#EXE_FILES[@]} -eq 0 ]]; then
    echo "Error: No executable files found in $RELEASE_DIR"
    echo "Please run the release script first to generate executables:"
    echo "  ./utils/release.sh"
    exit 1
fi

TOTAL_COUNT=${#EXE_FILES[@]}
TOTAL_TIME=$((TOTAL_COUNT * TIMEOUT_SECONDS))

echo "Found ${TOTAL_COUNT} executable(s) to test:"
for exe in "${EXE_FILES[@]}"; do
    # 显示相对于 Release 目录的路径
    rel_path="${exe#$RELEASE_DIR/}"
    echo "  - $rel_path"
done
echo ""
echo "Estimated total time: ${TOTAL_TIME}s (max ${TIMEOUT_SECONDS}s per demo)"
echo ""

# 记录成功运行的可执行文件
declare -a SUCCESS_EXES=()
CURRENT_INDEX=0

# 依次执行每个可执行文件
for exe in "${EXE_FILES[@]}"; do
    CURRENT_INDEX=$((CURRENT_INDEX + 1))
    # 使用相对于 Release 目录的路径
    exe_rel_path="${exe#$RELEASE_DIR/}"

    echo "[${CURRENT_INDEX}/${TOTAL_COUNT}] Running: $exe_rel_path ..."

    # 在后台运行可执行文件
    "$exe" &
    pid=$!
    TIMEOUT_REACHED=false

    # 等待指定时间或进程退出
    start_time=$(date +%s)
    while kill -0 "$pid" 2>/dev/null; do
        current_time=$(date +%s)
        elapsed=$((current_time - start_time))

        if [[ $elapsed -ge $TIMEOUT_SECONDS ]]; then
            # 超时，正常终止进程
            echo "  Timeout reached, terminating $exe_rel_path..."
            kill "$pid" 2>/dev/null || true
            wait "$pid" 2>/dev/null || true
            echo "  ✓ $exe_rel_path completed (timeout)"
            SUCCESS_EXES+=("$exe_rel_path")
            TIMEOUT_REACHED=true
            break
        fi

        sleep 0.1
    done

    # 检查进程是否在超时前退出（仅当非超时情况）
    if [[ "$TIMEOUT_REACHED" != "true" ]]; then
        # 进程已退出，获取退出码
        wait "$pid" || true
        exit_code=$?

        current_time=$(date +%s)
        elapsed=$((current_time - start_time))

        if [[ $exit_code -ne 0 ]]; then
            echo "  ✗ $exe_rel_path exited abnormally with code $exit_code after ${elapsed}s"
            echo ""
            echo "Error: Demo test failed!"
            echo "Failed executable: $exe_rel_path"
            exit 1
        else
            # 正常退出（退出码为 0）
            echo "  ✓ $exe_rel_path completed normally (${elapsed}s)"
            SUCCESS_EXES+=("$exe_rel_path")
        fi
    fi

    # 打印进度信息
    REMAINING_COUNT=$((TOTAL_COUNT - CURRENT_INDEX))
    REMAINING_TIME=$((REMAINING_COUNT * TIMEOUT_SECONDS))
    echo "  Progress: ${CURRENT_INDEX}/${TOTAL_COUNT} completed, ~${REMAINING_TIME}s remaining"
    echo ""
done

# 打印成功执行的可执行文件
echo "========================================"
echo "All demos completed successfully!"
echo "========================================"
echo "Total: ${#SUCCESS_EXES[@]} executable(s) tested:"
for exe in "${SUCCESS_EXES[@]}"; do
    echo "  ✓ $exe"
done
