#!/usr/bin/env bash

if [[ "$TEST_RELEASE_LIBS" != "true" ]]; then
    echo "TEST_RELEASE_LIBS is not set to true, skipping test of release libs."
else

    set -e

    cd "$(dirname "$0")/.."
    EGE_DIR=$(pwd)

    git clean -ffdx build-demo* || echo "git clean skipped, maybe no build directory exists."

    ./tasks.sh --test-release-libs --debug $@
    ./tasks.sh --test-release-libs --release $@
fi
