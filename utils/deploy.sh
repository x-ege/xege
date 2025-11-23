#!/usr/bin/env bash

cd "$(dirname "$0")/.."
EGE_DIR=$(pwd)

cd utils

# 解析命令行参数
FORCE_RELEASE=false
while [[ $# -gt 0 ]]; do
    case $1 in
        -f|--force)
            FORCE_RELEASE=true
            shift
            ;;
        *)
            echo "Unknown option: $1" >&2
            echo "Usage: $0 [-f|--force]" >&2
            exit 1
            ;;
    esac
done

# 检查 Release/lib 目录是否存在且不为空
if [[ $FORCE_RELEASE == false ]] && [[ -d "$EGE_DIR/Release/lib" ]] && [[ -n "$(ls -A "$EGE_DIR/Release/lib" 2>/dev/null)" ]]; then
    echo "Release/lib directory exists and is not empty. Skipping release.sh (use -f to force)."
else
    # 执行 release.sh
    ./release.sh || exit 1
fi

# 检查项目同级目录是否存在 xege_libs
SIBLING_LIBS_DIR="$EGE_DIR/../xege_libs"
if [[ -d "$SIBLING_LIBS_DIR/.git" ]]; then
    # 检查是否为正确的 git 仓库
    cd "$SIBLING_LIBS_DIR"
    REMOTE_URL=$(git remote get-url origin 2>/dev/null)
    if [[ "$REMOTE_URL" == "git@github.com:wysaid/xege.org.git" ]]; then
        echo "Using existing xege_libs from sibling directory."
        LIBS_DIR="$(realpath "$SIBLING_LIBS_DIR")"
    else
        echo "Warning: Sibling xege_libs exists but has different remote URL: $REMOTE_URL"
        echo "Using local xege_libs in utils directory instead."
        cd "$EGE_DIR/utils"
        LIBS_DIR=""
    fi
else
    cd "$EGE_DIR/utils"
    LIBS_DIR=""
fi

# 如果没有使用同级目录的 xege_libs，则在 utils 目录下创建/使用
if [[ -z "$LIBS_DIR" ]]; then
    if [[ ! -d "xege_libs/.git" ]]; then
        git clone git@github.com:wysaid/xege.org.git xege_libs
    fi

    LIBS_DIR="$(realpath "$EGE_DIR/utils/xege_libs")"
fi

# 进入 xege_libs 目录进行 git 操作
if ! cd "$LIBS_DIR"; then
    echo "Failed to enter xege_libs directory: $LIBS_DIR" >&2
    exit 1
fi

# 保存当前的修改，防止接下来的拷贝覆盖操作丢失数据
git add .
git stash -m "Auto stash before deploy"
git checkout master
git reset --hard origin/master
git pull

### Copy files

cp -rf "$EGE_DIR/Release/lib" "$LIBS_DIR/."
cp -rf "$EGE_DIR/include" "$LIBS_DIR/."
cp -rf "$EGE_DIR/man" "$LIBS_DIR/."