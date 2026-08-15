#!/usr/bin/env bash

set -euo pipefail

cd "$(dirname "$0")/.."
EGE_DIR=$(pwd -P)
RELEASE_DIR="${EGE_RELEASE_DIR:-$EGE_DIR/Release}"
BUILD_ROOT="${EGE_RELEASE_BUILD_ROOT:-$EGE_DIR/build/release-local}"
MACOS_DEPLOYMENT_TARGET="${EGE_MACOS_DEPLOYMENT_TARGET:-11.0}"

prepare_package_smoke() {
    local smoke_source="$BUILD_ROOT/package-smoke-source"
    local smoke_build="$BUILD_ROOT/package-smoke-build"

    cmake -E remove_directory "$smoke_source"
    cmake -E remove_directory "$smoke_build"
    cmake -E make_directory "$smoke_source/lib"
    cp -R "$EGE_DIR/include" "$EGE_DIR/demo" "$smoke_source/"
    cp "$EGE_DIR/.github/release-assets/CMakeLists.txt" "$smoke_source/"

    if [[ $(uname -s) == "Darwin" ]]; then
        cmake -E make_directory "$smoke_source/lib/macOS"
        cp "$RELEASE_DIR/lib/macOS/libgraphics.a" \
            "$smoke_source/lib/macOS/"
        cmake -S "$smoke_source" -B "$smoke_build" \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOS_DEPLOYMENT_TARGET"
    else
        cmake -E make_directory "$smoke_source/lib/mingw-w64-debian"
        cp "$RELEASE_DIR/lib/mingw-w64-debian/libgraphics.a" \
            "$smoke_source/lib/mingw-w64-debian/"
        cmake -S "$smoke_source" -B "$smoke_build" \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_TOOLCHAIN_FILE="$EGE_DIR/cmake/toolchains/mingw-w64.cmake"
    fi
    cmake --build "$smoke_build" --target demos --parallel 3

    if [[ $(uname -s) == "Darwin" ]]; then
        for arch in arm64 x86_64; do
            local arch_smoke_build="$BUILD_ROOT/package-smoke-$arch"
            cmake -E remove_directory "$arch_smoke_build"
            cmake -S "$smoke_source" -B "$arch_smoke_build" \
                -DCMAKE_BUILD_TYPE=Release \
                -DCMAKE_OSX_ARCHITECTURES="$arch" \
                -DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOS_DEPLOYMENT_TARGET"
            cmake --build "$arch_smoke_build" \
                --target graph_5star camera_base --parallel 3
            xcrun lipo "$arch_smoke_build/graph_5star" \
                -verify_arch "$arch"
            xcrun lipo "$arch_smoke_build/camera_base" \
                -verify_arch "$arch"
        done
    fi
}

if [[ $(uname -s) == "Darwin" ]]; then
    for arch in arm64 x86_64; do
        arch_build="$BUILD_ROOT/macos-$arch"
        cmake -S "$EGE_DIR" -B "$arch_build" \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_OSX_ARCHITECTURES="$arch" \
            -DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOS_DEPLOYMENT_TARGET" \
            -DEGE_DEFAULT_BACKEND=COREGRAPHICS \
            -DEGE_ENABLE_OPENGL=OFF \
            -DEGE_ENABLE_CAMERA_CAPTURE=ON \
            -DEGE_BUILD_TEST=OFF \
            -DEGE_BUILD_DEMO=OFF \
            -DEGE_BUILD_TEMP=OFF
        cmake --build "$arch_build" --target xege --parallel 3

        minos_values=$(find "$arch_build/CMakeFiles/xege.dir" \
            -name '*.o' -print0 | while IFS= read -r -d '' object; do
                xcrun vtool -show-build "$object"
            done | awk '$1 == "minos" { print $2 }' | sort -u)
        if [[ "$minos_values" != "$MACOS_DEPLOYMENT_TARGET" ]]; then
            echo "Unexpected ${arch} deployment target: ${minos_values}" >&2
            exit 1
        fi
    done

    cmake -E make_directory "$RELEASE_DIR/lib/macOS"
    xcrun lipo -create \
        "$BUILD_ROOT/macos-arm64/libgraphics.a" \
        "$BUILD_ROOT/macos-x86_64/libgraphics.a" \
        -output "$RELEASE_DIR/lib/macOS/libgraphics.a"
    xcrun lipo "$RELEASE_DIR/lib/macOS/libgraphics.a" \
        -verify_arch arm64 x86_64
    prepare_package_smoke
    echo "Created and verified universal macOS library: $RELEASE_DIR/lib/macOS/libgraphics.a"
elif [[ $(uname -s) == "Linux" ]]; then
    linux_build="$BUILD_ROOT/mingw-w64-debian"
    cmake -S "$EGE_DIR" -B "$linux_build" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE="$EGE_DIR/cmake/toolchains/mingw-w64.cmake" \
        -DEGE_ENABLE_CAMERA_CAPTURE=ON \
        -DEGE_BUILD_TEST=OFF \
        -DEGE_BUILD_DEMO=OFF \
        -DEGE_BUILD_TEMP=OFF
    cmake --build "$linux_build" --target xege --parallel 3
    cmake -E make_directory "$RELEASE_DIR/lib/mingw-w64-debian"
    cp "$linux_build/libgraphics.a" \
        "$RELEASE_DIR/lib/mingw-w64-debian/libgraphics.a"
    prepare_package_smoke
    echo "Created and verified MinGW library: $RELEASE_DIR/lib/mingw-w64-debian/libgraphics.a"
else
    bash -l "$EGE_DIR/utils/release-msvc.sh"
fi

echo "All tasks completed successfully."
