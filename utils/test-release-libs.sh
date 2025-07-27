#!/usr/bin/env bash

if [[ "$TEST_RELEASE_LIBS" != "true" ]]; then
    echo "TEST_RELEASE_LIBS is not set to true, skipping test of release libs."
else

    set -e

    cd "$(dirname "$0")/.."
    EGE_DIR=$(pwd)

    git clean -ffdx build-demo* || echo "git clean skipped, maybe no build directory exists."

    if [[ "$*" == *"msvc2015"* || "$*" == *"msvc2010"* ]]; then
        echo "Skipping tests for msvc2015 or msvc2010 because they do not support C++17 required by the demo."
    else
        set -x
        ./tasks.sh --test-release-libs --debug "$@"
        ./tasks.sh --test-release-libs --release "$@"
    fi
fi
