#!/usr/bin/env bash
# scripts/ci/static-analysis.sh
#
# Run cppcheck + clang --analyze on core and driver modules.
# Mirrors the checks performed by .github/workflows/static-analysis.yml.
#
# Usage:
#   ./scripts/ci/static-analysis.sh          # analyse all targets
#   ./scripts/ci/static-analysis.sh core     # only src/core + src/guest
#   ./scripts/ci/static-analysis.sh drivers  # only drivers/
#
# Requires: cppcheck, clang
# Exit code: 0 = clean, non-zero = defects found

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

TARGET="${1:-all}"
FAILED=0

have_tool() {
    command -v "$1" > /dev/null 2>&1
}

# ---------------------------------------------------------------------------
# cppcheck helpers
# ---------------------------------------------------------------------------
run_cppcheck() {
    local label="$1"; shift
    local dirs=("$@")

    if ! have_tool cppcheck; then
        echo "[static-analysis] SKIP cppcheck not found (install: brew install cppcheck / apt install cppcheck)"
        return 0
    fi

    echo "[static-analysis] cppcheck: ${label}"
    if ! cppcheck \
            --std=c11 \
            --enable=warning,performance,portability \
            --error-exitcode=1 \
            --inline-suppr \
            --suppress=missingIncludeSystem \
            --suppress=syntaxError:drivers/linux/haven_driver.c \
            -I include \
            "${dirs[@]}" 2>&1; then
        echo "[static-analysis] FAIL cppcheck: ${label}"
        FAILED=1
    else
        echo "[static-analysis] PASS cppcheck: ${label}"
    fi
}

# ---------------------------------------------------------------------------
# clang --analyze helpers
# ---------------------------------------------------------------------------
run_clang_analyze() {
    local label="$1"; shift
    local dirs=("$@")

    if ! have_tool clang; then
        echo "[static-analysis] SKIP clang --analyze not found"
        return 0
    fi

    echo "[static-analysis] clang --analyze: ${label}"
    local file_failed=0
    while IFS= read -r f; do
        if ! clang --analyze \
                -std=c11 \
                -I. \
                -Iinclude \
                -Isrc \
                -Xanalyzer -analyzer-output=text \
                -Xanalyzer -analyzer-checker=core,deadcode,security \
                "$f" 2>&1; then
            file_failed=1
        fi
    done < <(find "${dirs[@]}" -name "*.c" | sort)

    if [ "$file_failed" -ne 0 ]; then
        echo "[static-analysis] FAIL clang --analyze: ${label}"
        FAILED=1
    else
        echo "[static-analysis] PASS clang --analyze: ${label}"
    fi
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
if [ "$TARGET" = "all" ] || [ "$TARGET" = "core" ]; then
    run_cppcheck "src/core + src/guest" src/core src/guest
    run_clang_analyze "src/core + src/guest" src/core src/guest
fi

if [ "$TARGET" = "all" ] || [ "$TARGET" = "drivers" ]; then
    run_cppcheck "drivers/" drivers
fi

if [ "$FAILED" -ne 0 ]; then
    echo "[static-analysis] FAIL - defects found above"
    exit 1
fi

echo "[static-analysis] PASS - no defects"
exit 0
