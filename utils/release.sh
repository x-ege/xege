#!/usr/bin/env bash

cd "$(dirname "$0")/.."

EGE_DIR=$(pwd)

git clean -ffdx build Release

if [[ $(uname -s) == "Darwin" ]]; then
    # macos/linux
    if ./tasks.sh --clean --release --load --target xege --build; then
        mkdir -p Release/lib/macOS
        cp -rf build/*.a Release/lib/macOS
        echo "Copy macOS libs done: $(pwd)/Release/lib/macOS"

        ./utils/test-release-libs.sh --build-dir "$EGE_DIR/build-mingw-macos"
    else
        echo "Build macOS failed!" >&2
    fi
elif [[ $(uname -s) == "Linux" ]]; then
    if ./tasks.sh --clean --release --load --target xege --build; then
        # 目前暂时只提供 mingw-w64-debian 的版本, 所以默认 mingw-w64-debian. 后续如果要支持更多, 那么再改.
        mkdir -p Release/lib/mingw-w64-debian
        cp -rf build/*.a Release/lib/mingw-w64-debian
        echo "Copy mingw-w64-debian libs done: $(pwd)/Release/lib/mingw-w64-debian"
        ./utils/test-release-libs.sh --build-dir "$EGE_DIR/build-mingw-debian"
    else
        echo "Build mingw-w64-debian failed!" >&2
    fi
else
    bash -l "$EGE_DIR/utils/release-msvc.sh" || {
        echo "Build MSVC failed!" >&2
        exit 1
    }
fi

if [[ $? -eq 0 ]]; then
    echo "All tasks completed successfully."
fi
