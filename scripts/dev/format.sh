#!/usr/bin/env sh
# Haven clang-format automation script.
#
# Usage:
#   scripts/format.sh               - format all tracked C/H files in-place
#   scripts/format.sh --staged      - format only files staged for this commit
#   scripts/format.sh --check       - check all tracked files (exit 1 if any need reformatting)
#   scripts/format.sh --staged --check - check only staged files (used by pre-commit hook)
#
# Assembly (.S) files are intentionally excluded: ARM64 EL2 entry code uses
# GNU assembler directives that clang-format mishandles.

set -eu

# ---------------------------------------------------------------------------
# Parse arguments
# ---------------------------------------------------------------------------
MODE="format"   # format | check
TARGET="all"    # all | staged

for arg in "$@"; do
    case "$arg" in
        --check)  MODE="check" ;;
        --staged) TARGET="staged" ;;
        --help)
            sed -n '2,11p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            echo "[format] unknown argument: $arg" >&2
            exit 1
            ;;
    esac
done

# ---------------------------------------------------------------------------
# Verify clang-format is available and recent enough
# ---------------------------------------------------------------------------
if ! command -v clang-format >/dev/null 2>&1; then
    echo "[format] ERROR: clang-format not found"
    echo "  macOS : brew install clang-format"
    echo "  Ubuntu: sudo apt-get install clang-format"
    exit 1
fi

CF_MAJOR=$(clang-format --version 2>&1 | grep -oE '[0-9]+' | head -1)
if [ "${CF_MAJOR:-0}" -lt 11 ]; then
    echo "[format] WARNING: clang-format >= 11 recommended (found version ${CF_MAJOR:-?})"
fi

# ---------------------------------------------------------------------------
# Collect files
# ---------------------------------------------------------------------------
if [ "$TARGET" = "staged" ]; then
    # Only C/H files that are staged (Added, Copied, Modified, Renamed)
    FILES=$(git diff --cached --name-only --diff-filter=ACMR 2>/dev/null \
            | grep -E '\.(c|h)$' || true)
    DESC="staged"
else
    # All tracked C/H files in the repository
    FILES=$(git ls-files 2>/dev/null | grep -E '\.(c|h)$' || true)
    DESC="all tracked"
fi

if [ -z "$FILES" ]; then
    echo "[format] no C/H files to process ($DESC)"
    exit 0
fi

# ---------------------------------------------------------------------------
# Format or check
# ---------------------------------------------------------------------------
FAIL=0
FMT_COUNT=0
BAD_FILES=""

for f in $FILES; do
    [ -f "$f" ] || continue

    if [ "$MODE" = "check" ]; then
        # --dry-run --Werror: exits non-zero if the file would change
        if ! clang-format --dry-run --Werror "$f" >/dev/null 2>&1; then
            BAD_FILES="${BAD_FILES} $f"
            FAIL=1
        fi
    else
        clang-format -i "$f"
        FMT_COUNT=$((FMT_COUNT + 1))

        # Re-stage the file so the formatted version is what gets committed.
        # Only done in --staged mode to avoid accidentally staging untracked
        # changes when running in bulk (--all) mode.
        if [ "$TARGET" = "staged" ]; then
            git add "$f"
        fi
    fi
done

# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------
if [ "$MODE" = "format" ]; then
    echo "[format] formatted $FMT_COUNT file(s) ($DESC)"
elif [ "$FAIL" -ne 0 ]; then
    echo "[format] FAIL: the following file(s) need reformatting:"
    for f in $BAD_FILES; do
        echo "  $f"
    done
    echo ""
    echo "  Fix with:  scripts/format.sh"
    echo "  Then:      git add <files> && git commit"
    exit 1
else
    echo "[format] ok ($DESC)"
fi
