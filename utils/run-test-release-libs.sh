#!/usr/bin/env bash

set -e
cd "$(dirname "$0")"
export TEST_RELEASE_LIBS=true
./release.sh
