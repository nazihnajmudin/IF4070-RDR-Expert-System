#!/bin/sh
# End-to-end test runner.
#
# Usage
#   sh tests/e2e/run_e2e.sh             run CLI and GUI scenarios
#   sh tests/e2e/run_e2e.sh --update    rewrite expected CLI outputs (review the diff afterwards)
#   sh tests/e2e/run_e2e.sh --windows   also run the CLI scenarios and the GUI smoke test with the
#                                       Windows executables through WSL interop
#   sh tests/e2e/run_e2e.sh --no-gui    skip GUI scenarios (for machines without a display)
#
# CLI scenarios (tests/e2e/cli) are compared line by line with tests/e2e/expected.
# GUI scenarios (tests/e2e/gui) drive the real GUI through the test hook and fail
# when an "expect" line does not hold. Screenshots are written to build/e2e/shots.

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$ROOT_DIR" || exit 1

UPDATE=0
WINDOWS=0
GUI=1
for arg in "$@"; do
    case "$arg" in
        --update) UPDATE=1 ;;
        --windows) WINDOWS=1 ;;
        --no-gui) GUI=0 ;;
        *) echo "unknown option: $arg" >&2; exit 2 ;;
    esac
done

CLI_BIN=build/linux/rdr-cli
GUI_BIN=build/hook/linux/rdr-expert
CLI_WIN=build/windows/rdr-cli.exe
GUI_WIN=build/hook/windows/rdr-expert.exe
OUT=build/e2e
PASS=0
FAIL=0

# Workspace Setup
rm -rf "$OUT"
mkdir -p "$OUT/tmp" "$OUT/out" "$OUT/shots"
awk 'BEGIN { print "case,conclusion"; for (i = 0; i < 2000; i++) printf "\"f%d; g%d\",Kelas%d\n", i, i % 10, (i * 7) % 5 }' \
    > "$OUT/tmp/large.csv"

report() {
    if [ "$1" -eq 0 ]; then
        PASS=$((PASS + 1))
        echo "  PASS  $2"
    else
        FAIL=$((FAIL + 1))
        echo "  FAIL  $2"
    fi
}

# CLI Scenarios
run_cli() {
    bin="$1"
    label="$2"
    echo "CLI scenarios ($label)"
    for script in tests/e2e/cli/*.txt; do
        name=$(basename "$script" .txt)
        actual="$OUT/out/$name.$label.out"
        "$bin" --echo "$script" 2>&1 | tr -d '\r' > "$actual"
        expected="tests/e2e/expected/$name.out"
        if [ "$UPDATE" -eq 1 ] && [ "$label" = "linux" ]; then
            cp "$actual" "$expected"
        fi
        diff -u "$expected" "$actual" > "$OUT/out/$name.$label.diff" 2>&1
        report $? "$name"
    done
}

run_cli "$CLI_BIN" linux
if [ "$WINDOWS" -eq 1 ]; then
    run_cli "$CLI_WIN" windows
fi

# GUI Scenarios
run_gui() {
    bin="$1"
    label="$2"
    pattern="$3"
    echo "GUI scenarios ($label)"
    for script in $pattern; do
        name=$(basename "$script" .txt)
        mkdir -p "$OUT/shots/$label/$name"
        timeout 120 "$bin" --test-script "$script" --shot-dir "$OUT/shots/$label/$name" \
            > "$OUT/out/$name.$label.gui.log" 2>&1
        status=$?
        grep -h "EXPECT FAILED" "$OUT/out/$name.$label.gui.log" | sed 's/^/        /'
        report $status "$name"
    done
}

if [ "$GUI" -eq 1 ]; then
    if [ -z "$DISPLAY" ] && [ -z "$WAYLAND_DISPLAY" ]; then
        echo "GUI scenarios skipped: no display"
    else
        run_gui "$GUI_BIN" linux "tests/e2e/gui/*.txt"
        if [ "$WINDOWS" -eq 1 ]; then
            run_gui "$GUI_WIN" windows "tests/e2e/gui/02_*.txt"
        fi
    fi
fi

echo "E2E: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
