#!/bin/sh
# Build script for UNIX systems including WSL2.
#
# Usage
#   scripts/build.sh            build GUI and CLI for the host
#   scripts/build.sh test       build and run unit tests
#   scripts/build.sh windows    cross-compile Windows executables (needs mingw-w64)
#   scripts/build.sh clean      remove build output
#
# Output is written to build/<target>/.

set -e

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT_DIR"

# Tool Check
if ! command -v make >/dev/null 2>&1; then
    echo "error: 'make' is required" >&2
    exit 1
fi
if ! command -v cc >/dev/null 2>&1 && ! command -v gcc >/dev/null 2>&1; then
    echo "error: a C compiler (cc or gcc) is required" >&2
    exit 1
fi

JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)

# Target Dispatch
case "${1:-all}" in
    all)
        make -j"$JOBS" all
        echo "Built: build/linux/rdr-expert and build/linux/rdr-cli"
        ;;
    test)
        make -j"$JOBS" test
        ;;
    windows)
        if ! command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
            echo "error: mingw-w64 is required (sudo apt install mingw-w64)" >&2
            exit 1
        fi
        make -j"$JOBS" windows
        echo "Built: build/windows/rdr-expert.exe and build/windows/rdr-cli.exe"
        ;;
    clean)
        make clean
        ;;
    *)
        echo "usage: $0 [all|test|windows|clean]" >&2
        exit 1
        ;;
esac
