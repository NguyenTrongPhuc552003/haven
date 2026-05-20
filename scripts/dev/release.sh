#!/usr/bin/env sh
# Haven release workflow.
#
# Usage:
#   ./scripts/release.sh [VERSION]
#
# If VERSION is omitted the value from the VERSION file is used.
# The script:
#   1. Validates the working tree is clean (--dirty flag skips this).
#   2. Runs the full CI preflight gate.
#   3. Refreshes the benchmark baseline.
#   4. Packages the evidence bundle.
#   5. Archives the evidence bundle to build/releases/<version>/.
#   6. Optionally creates a git tag (skipped with --no-tag).
#   7. Prints a release summary.
#
# Flags:
#   --dirty   allow uncommitted changes (development use only)
#   --no-tag  skip git tagging

set -eu

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

ALLOW_DIRTY=0
NO_TAG=0
VERSION_ARG=""

for arg in "$@"; do
	case "$arg" in
		--dirty)  ALLOW_DIRTY=1 ;;
		--no-tag) NO_TAG=1 ;;
		-*)       printf '[release] unknown flag: %s\n' "$arg" >&2; exit 1 ;;
		*)        VERSION_ARG="$arg" ;;
	esac
done

if [ -n "$VERSION_ARG" ]; then
	RELEASE_VERSION="$VERSION_ARG"
else
	if [ ! -f VERSION ]; then
		echo "[release] ERROR: VERSION file not found and no version argument given." >&2
		exit 1
	fi
	RELEASE_VERSION="$(tr -d '[:space:]' < VERSION)"
fi

echo "[release] preparing version: $RELEASE_VERSION"

if [ "$ALLOW_DIRTY" -eq 0 ]; then
	if command -v git > /dev/null 2>&1 && git rev-parse --git-dir > /dev/null 2>&1; then
		if ! git diff --quiet HEAD; then
			echo "[release] ERROR: working tree has uncommitted changes." >&2
			echo "          Use --dirty to override (development only)." >&2
			exit 1
		fi
		echo "[release] working tree clean."
	else
		echo "[release] git not available or not a repo - skipping cleanliness check."
	fi
fi

echo "[release] step 1/4 - running CI preflight gate …"
if ! ./scripts/ci-preflight.sh; then
	echo "[release] ERROR: CI preflight failed - release aborted." >&2
	exit 1
fi
echo "[release] CI preflight passed."

echo "[release] step 2/4 - refreshing benchmark baseline …"
if command -v python3 > /dev/null 2>&1; then
	if ! python3 ./scripts/benchmark-baseline.py; then
		echo "[release] WARNING: benchmark baseline refresh failed - continuing." >&2
	else
		echo "[release] benchmark baseline refreshed."
	fi
else
	echo "[release] python3 not found - skipping benchmark baseline refresh."
fi

echo "[release] step 3/4 - packaging evidence bundle …"
if ! ./scripts/package-evidence.sh; then
	echo "[release] ERROR: evidence packaging failed - release aborted." >&2
	exit 1
fi
echo "[release] evidence bundle packaged."

echo "[release] step 4/4 - archiving evidence bundle …"
RELEASE_DIR="build/releases/${RELEASE_VERSION}"
mkdir -p "$RELEASE_DIR"

if [ -d build/evidence ]; then
	mkdir -p "$RELEASE_DIR/evidence"
	cp -r build/evidence/. "$RELEASE_DIR/evidence/"
fi
if [ -f build/benchmarks/baseline.json ]; then
	mkdir -p "$RELEASE_DIR/benchmarks"
	cp build/benchmarks/baseline.json "$RELEASE_DIR/benchmarks/"
fi
if [ -f build/benchmarks/isolation-latency.json ]; then
	mkdir -p "$RELEASE_DIR/benchmarks"
	cp build/benchmarks/isolation-latency.json "$RELEASE_DIR/benchmarks/"
fi

TIMESTAMP="$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
cat > "$RELEASE_DIR/release-manifest.json" << EOF
{
  "version": "${RELEASE_VERSION}",
  "timestamp": "${TIMESTAMP}",
  "preflight": "pass",
  "evidence_dir": "evidence/",
  "benchmarks_dir": "benchmarks/"
}
EOF
echo "[release] archived to ${RELEASE_DIR}/"

if [ "$NO_TAG" -eq 0 ]; then
	if command -v git > /dev/null 2>&1 && git rev-parse --git-dir > /dev/null 2>&1; then
		TAG_NAME="v${RELEASE_VERSION}"
		if git tag -l "$TAG_NAME" | grep -q "^${TAG_NAME}$"; then
			echo "[release] WARNING: tag $TAG_NAME already exists - skipping."
		else
			git tag -a "$TAG_NAME" -m "Haven ${RELEASE_VERSION} - automated release"
			echo "[release] tag created: $TAG_NAME"
			echo "[release] to push: git push origin $TAG_NAME"
		fi
	else
		echo "[release] git not available - skipping tag."
	fi
else
	echo "[release] --no-tag set - skipping git tag."
fi

printf '\n'
echo "======================================"
printf '  Haven %s release complete\n' "$RELEASE_VERSION"
printf '  Evidence : %s/evidence/\n' "$RELEASE_DIR"
printf '  Manifest : %s/release-manifest.json\n' "$RELEASE_DIR"
echo "======================================"
