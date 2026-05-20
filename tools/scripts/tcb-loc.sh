#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# tools/scripts/tcb-loc.sh - Trusted Computing Base line-of-code counter
#
# Counts source lines in the Haven trusted core, excluding test files,
# generated files, and third-party code.  Reports a per-module breakdown
# plus a total.  The target threshold is <5 KLOC for the trusted core.
#
# Usage: ./tools/scripts/tcb-loc.sh [--verbose] [ROOT]
#   ROOT defaults to the repository root (two levels up from this script).
#
# Exit codes:
#   0 - count succeeded (threshold may or may not be met)
#   1 - usage error
#   2 - cloc/wc not found

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="${2:-$(cd "$SCRIPT_DIR/../.." && pwd)}"
VERBOSE=0

for arg in "$@"; do
  case "$arg" in
    --verbose|-v) VERBOSE=1 ;;
    --help|-h)
      echo "Usage: $0 [--verbose] [REPO_ROOT]"
      exit 0
      ;;
  esac
done

# ---------------------------------------------------------------------------
# Trusted core modules: directories whose source contributes to the TCB.
# Excludes: tests/, tools/, verification/, website/, docs/, build/.
# ---------------------------------------------------------------------------
declare -a TCB_DIRS=(
  "src/core"
  "src/common"
  "arch/arm64"
  "drivers/iommu"
  "drivers/irqchip"
  "drivers/uart"
  "include/haven"
)

# Source file extensions to count
EXTS=("*.c" "*.h" "*.S")

# ---------------------------------------------------------------------------
# Helper: count non-blank, non-comment lines in a directory tree
# Uses cloc if available; falls back to wc -l over .c/.h/.S files.
# ---------------------------------------------------------------------------
count_dir() {
  local dir="$1"
  local full="$REPO_ROOT/$dir"
  if [[ ! -d "$full" ]]; then
    echo 0
    return
  fi

  if command -v cloc &>/dev/null; then
    # cloc: count C, C header, Assembly — suppress banner/progress
    cloc --quiet --csv --include-lang="C,C/C++ Header,Assembly" \
         --not-match-f="test_|_test\." "$full" 2>/dev/null \
      | awk -F',' 'NR>1 && $2 ~ /^(C|C\/C\+\+ Header|Assembly)$/ {sum += $5} END {print sum+0}'
  else
    # Fallback: raw line count excluding blank lines
    local total=0
    for ext in "${EXTS[@]}"; do
      while IFS= read -r -d '' f; do
        # Skip test helper files
        [[ "$(basename "$f")" == test_* ]] && continue
        [[ "$(basename "$f")" == *_test.* ]] && continue
        total=$((total + $(grep -c '' "$f" || true)))
      done < <(find "$full" -name "$ext" -print0 2>/dev/null)
    done
    echo "$total"
  fi
}

# ---------------------------------------------------------------------------
# Main: iterate modules, accumulate totals
# ---------------------------------------------------------------------------
echo "Haven Trusted Computing Base — Line Count"
echo "=========================================="
printf "%-35s  %8s\n" "Module" "SLOC"
echo "------------------------------------------"

grand_total=0

for dir in "${TCB_DIRS[@]}"; do
  loc=$(count_dir "$dir")
  grand_total=$((grand_total + loc))
  printf "%-35s  %8d\n" "$dir" "$loc"
done

echo "------------------------------------------"
printf "%-35s  %8d\n" "TOTAL" "$grand_total"

THRESHOLD=5000
echo ""
if [[ "$grand_total" -le "$THRESHOLD" ]]; then
  echo "OK: TCB is ${grand_total} SLOC (threshold: ${THRESHOLD})"
else
  echo "WARNING: TCB is ${grand_total} SLOC — EXCEEDS threshold of ${THRESHOLD}"
fi

if [[ "$VERBOSE" == "1" ]]; then
  echo ""
  echo "Detailed file breakdown:"
  for dir in "${TCB_DIRS[@]}"; do
    full="$REPO_ROOT/$dir"
    [[ -d "$full" ]] || continue
    echo "  $dir/"
    for ext in "${EXTS[@]}"; do
      while IFS= read -r -d '' f; do
        rel="${f#"$REPO_ROOT/"}"
        lc=$(grep -c '' "$f" || true)
        printf "    %-55s %6d\n" "$rel" "$lc"
      done < <(find "$full" -name "$ext" -print0 2>/dev/null)
    done
  done
fi
