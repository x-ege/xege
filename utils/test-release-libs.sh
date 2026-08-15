#!/usr/bin/env bash

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    cat <<'EOF'
Usage: utils/test-release-libs.sh [tasks.sh options]

When TEST_RELEASE_LIBS=true, build demos against the libraries in Release/.
The caller should pass a unique --build-dir for each compiler/architecture.
No build directories are removed implicitly.
EOF
    exit 0
fi

if [[ "${TEST_RELEASE_LIBS:-false}" != "true" ]]; then
    echo "TEST_RELEASE_LIBS is not set to true, skipping test of release libs."
else

    set -e

    cd "$(dirname "$0")/.."
    EGE_DIR=$(pwd)

    if [[ "$*" == *"msvc2015"* || "$*" == *"msvc2010"* ]]; then
        echo "Skipping tests for msvc2015 or msvc2010 because they do not support C++17 required by the demo."
    else
        set -x
        ./tasks.sh --test-release-libs --debug "$@" &&
            ./tasks.sh --test-release-libs --release "$@"
    fi
fi
