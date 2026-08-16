#!/usr/bin/env bash

# Launch demo executables one at a time. Surviving the timeout is a smoke-test
# success; exiting early with a non-zero status is a failure. This test creates
# visible windows and must be run only in an interactive desktop session.

set -u

cd "$(dirname "$0")/.." || exit 1
RELEASE_DIR="$(pwd -P)/Release"
TIMEOUT_SECONDS=5
INCLUDE_CAMERA=false

while [[ $# -gt 0 ]]; do
    case "$1" in
    --directory)
        [[ $# -ge 2 ]] || { echo "--directory requires a path" >&2; exit 2; }
        RELEASE_DIR="$2"
        shift 2
        ;;
    --timeout)
        [[ $# -ge 2 && "$2" =~ ^[1-9][0-9]*$ ]] || {
            echo "--timeout requires a positive integer" >&2
            exit 2
        }
        TIMEOUT_SECONDS="$2"
        shift 2
        ;;
    --include-camera)
        INCLUDE_CAMERA=true
        shift
        ;;
    -h|--help)
        echo "usage: $0 [--directory path] [--timeout seconds] [--include-camera]"
        exit 0
        ;;
    *)
        echo "Unknown option: $1" >&2
        exit 2
        ;;
    esac
done

if [[ ! -d "$RELEASE_DIR" ]]; then
    echo "Error: demo directory not found: $RELEASE_DIR" >&2
    exit 1
fi

declare -a DEMO_FILES=()
declare -a RUNNER=()
HOST_SYSTEM=$(uname -s)
SEARCH_ROOT="$RELEASE_DIR"
if [[ -d "$RELEASE_DIR/demo" ]]; then
    SEARCH_ROOT="$RELEASE_DIR/demo"
fi

if [[ "$HOST_SYSTEM" == "Darwin" || "$HOST_SYSTEM" == "Linux" ]]; then
    while IFS= read -r candidate; do
        case "$candidate" in
        */CMakeFiles/*|*/tests/*|*/Testing/*) continue ;;
        esac
        if [[ "$INCLUDE_CAMERA" != true && $(basename "$candidate") == camera_* ]]; then
            continue
        fi
        if [[ -x "$candidate" ]] && {
           { [[ "$HOST_SYSTEM" == "Darwin" ]] && file "$candidate" | grep -q "Mach-O.*executable"; } ||
           { [[ "$HOST_SYSTEM" == "Linux" ]] && file "$candidate" | grep -q "ELF.*executable"; }; }; then
            DEMO_FILES+=("$candidate")
        fi
    done < <(find "$SEARCH_ROOT" -type f -perm -111 2>/dev/null | sort)

    # A Linux directory can still intentionally contain MinGW demo artifacts.
    # Fall back to Wine only when no native ELF demos were found.
    if [[ "$HOST_SYSTEM" == "Linux" && ${#DEMO_FILES[@]} -eq 0 ]]; then
        while IFS= read -r candidate; do
            if [[ "$INCLUDE_CAMERA" != true && $(basename "$candidate") == camera_*.exe ]]; then
                continue
            fi
            DEMO_FILES+=("$candidate")
        done < <(find "$SEARCH_ROOT" -type f -name "*.exe" 2>/dev/null | sort)
        if [[ ${#DEMO_FILES[@]} -gt 0 ]]; then
            if ! command -v wine >/dev/null 2>&1; then
                echo "Error: Wine is required to launch Windows demos on Linux" >&2
                exit 1
            fi
            RUNNER=(wine)
        fi
    fi
else
    while IFS= read -r candidate; do
        if [[ "$INCLUDE_CAMERA" != true && $(basename "$candidate") == camera_*.exe ]]; then
            continue
        fi
        DEMO_FILES+=("$candidate")
    done < <(find "$SEARCH_ROOT" -type f -name "*.exe" 2>/dev/null | sort)
fi

if [[ ${#DEMO_FILES[@]} -eq 0 ]]; then
    echo "Error: no platform demo executables found in $RELEASE_DIR" >&2
    exit 1
fi

echo "Found ${#DEMO_FILES[@]} demo executable(s); timeout=${TIMEOUT_SECONDS}s"
current=0
for demo in "${DEMO_FILES[@]}"; do
    current=$((current + 1))
    relative="${demo#$RELEASE_DIR/}"
    echo "[$current/${#DEMO_FILES[@]}] $relative"
    "${RUNNER[@]}" "$demo" &
    pid=$!
    started=$(date +%s)
    timed_out=false

    while kill -0 "$pid" 2>/dev/null; do
        now=$(date +%s)
        if [[ $((now - started)) -ge $TIMEOUT_SECONDS ]]; then
            timed_out=true
            kill "$pid" 2>/dev/null || true
            if wait "$pid" 2>/dev/null; then :; fi
            echo "  PASS (survived timeout)"
            break
        fi
        sleep 0.1
    done

    if [[ "$timed_out" == false ]]; then
        if wait "$pid"; then
            exit_code=0
        else
            exit_code=$?
        fi
        if [[ $exit_code -ne 0 ]]; then
            echo "  FAIL (early exit $exit_code)" >&2
            exit 1
        fi
        echo "  PASS (normal early exit)"
    fi
done

echo "All ${#DEMO_FILES[@]} demos passed the launch smoke test."
