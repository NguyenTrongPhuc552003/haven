#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# scripts/dev/fix-emdash.sh
#
# Replace em-dashes (-, U+2014) with hyphens (-) in all tracked text files.
#
# Usage:
#   ./scripts/dev/fix-emdash.sh          # Fix all tracked text files
#   ./scripts/dev/fix-emdash.sh --check  # Report occurrences without modifying
#   ./scripts/dev/fix-emdash.sh --staged # Fix only files staged for commit
#
# Always exits 0 (non-blocking). Safe to call from pre-commit hooks and
# release scripts. Idempotent - running twice has no additional effect.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

CHECK_ONLY=0
STAGED_ONLY=0

for arg in "$@"; do
	case "$arg" in
		--check)  CHECK_ONLY=1 ;;
		--staged) STAGED_ONLY=1 ;;
		*) printf '[fix-emdash] unknown flag: %s\n' "$arg" >&2 ;;
	esac
done

# File extensions to scan
PATTERNS="*.md *.mdx *.sh *.c *.h *.py *.yml *.yaml *.tex *.txt *.v *.thy"

# Collect files to scan
if [ "$STAGED_ONLY" -eq 1 ]; then
	# shellcheck disable=SC2086
	FILES=$(git diff --cached --name-only --diff-filter=ACM -- $PATTERNS 2>/dev/null || true)
else
	# All tracked text files matching the patterns
	# shellcheck disable=SC2086
	FILES=$(git ls-files -- $PATTERNS 2>/dev/null || true)
fi

if [ -z "$FILES" ]; then
	echo "[fix-emdash] no matching files found"
	exit 0
fi

FOUND=0
FIXED=0

while IFS= read -r f; do
	[ -f "$f" ] || continue
	# Check for em-dash (UTF-8: E2 80 94)
	if grep -qP '\xe2\x80\x94' "$f" 2>/dev/null; then
		FOUND=$((FOUND + 1))
		if [ "$CHECK_ONLY" -eq 1 ]; then
			echo "[fix-emdash] found em-dash: $f"
		else
			# In-place replacement; macOS sed needs empty string for -i
			sed -i.emdash_bak 's/\xe2\x80\x94/-/g' "$f"
			rm -f "${f}.emdash_bak"
			echo "[fix-emdash] fixed: $f"
			FIXED=$((FIXED + 1))
		fi
	fi
done <<EOF
$FILES
EOF

if [ "$CHECK_ONLY" -eq 1 ]; then
	if [ "$FOUND" -eq 0 ]; then
		echo "[fix-emdash] check: no em-dashes found"
	else
		echo "[fix-emdash] check: $FOUND file(s) contain em-dashes"
	fi
else
	if [ "$FIXED" -eq 0 ]; then
		echo "[fix-emdash] no em-dashes found"
	else
		echo "[fix-emdash] fixed $FIXED file(s)"
	fi
fi

exit 0
