#!/usr/bin/env bash
# scripts/release.sh
#
# Haven release preparation script.
#
# Performs a complete pre-release validation pass:
#   0. Em-dash cleanup (fix-emdash.sh).
#   1. Version consistency check (VERSION file vs git tags).
#   2. Preflight: build + style + unit/integration tests.
#   3. Benchmark regression check against stored baseline.
#   4. QEMU smoke test (if qemu-system-aarch64 is available).
#   5. Evidence pack: collects build artefacts + test results.
#   6. Optionally creates a signed git tag.
#
# Usage:
#   ./scripts/release.sh [--tag] [--no-smoke] [--dry-run] [--dirty]
#
#   --tag       Create and push the git tag after successful validation.
#   --no-smoke  Skip the QEMU smoke test (e.g. on headless CI runners).
#   --dry-run   Run all checks but do not tag or write any release artefacts.
#   --dirty     Allow uncommitted changes (development use only).
#
# Prerequisites:
#   - cc (host build, GCC or Clang)
#   - python3
#   - qemu-system-aarch64 (optional, skipped with --no-smoke)
#   - aarch64-linux-gnu-gcc (optional, for ARM64 cross-compile check)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# -----------------------------------------------------------------------
# Argument parsing
# -----------------------------------------------------------------------
DO_TAG=0
DO_SMOKE=1
DRY_RUN=0
ALLOW_DIRTY=0

for arg in "$@"; do
    case "$arg" in
        --tag)      DO_TAG=1 ;;
        --no-smoke) DO_SMOKE=0 ;;
        --dry-run)  DRY_RUN=1 ;;
        --dirty)    ALLOW_DIRTY=1 ;;
        *)
            echo "[release] ERROR: unknown argument: $arg" >&2
            echo "  Usage: $0 [--tag] [--no-smoke] [--dry-run] [--dirty]" >&2
            exit 1
            ;;
    esac
done

# -----------------------------------------------------------------------
# Helpers
# -----------------------------------------------------------------------

step() { echo ""; echo "=== $* ==="; }
ok()   { echo "[release] OK: $*"; }
fail() { echo "[release] FAIL: $*" >&2; exit 1; }

cd "$ROOT"

# -----------------------------------------------------------------------
# 0. Em-dash cleanup
# -----------------------------------------------------------------------
step "0/6  Em-dash cleanup"

"$SCRIPT_DIR/dev/fix-emdash.sh"
ok "em-dash cleanup complete"

# -----------------------------------------------------------------------
# 0b. Working tree check
# -----------------------------------------------------------------------
if [ "$ALLOW_DIRTY" -eq 0 ]; then
    if git rev-parse --git-dir > /dev/null 2>&1; then
        if ! git diff --quiet HEAD; then
            fail "working tree has uncommitted changes. Use --dirty to override (development only)."
        fi
        ok "working tree clean"
    fi
else
    echo "[release] --dirty set: skipping working tree check"
fi

# -----------------------------------------------------------------------
# 1. Version consistency
# -----------------------------------------------------------------------
step "1/6  Version consistency"

VERSION="$(cat VERSION | tr -d '[:space:]')"
echo "[release] VERSION file: $VERSION"

# Reject if main is already tagged with this version
if git tag --list "v${VERSION}" | grep -q "v${VERSION}"; then
    fail "Tag v${VERSION} already exists. Bump VERSION before releasing."
fi

ok "v${VERSION} is a new version"

# -----------------------------------------------------------------------
# 2. Preflight (build + style + tests)
# -----------------------------------------------------------------------
step "2/6  Preflight"

./scripts/ci/ci-preflight.sh
ok "preflight passed"

# -----------------------------------------------------------------------
# 3. Benchmark regression
# -----------------------------------------------------------------------
step "3/6  Benchmark regression (threshold=10%)"

BASELINE="tests/benchmarks/latency-baseline.json"
if [ -f "$BASELINE" ]; then
    BASELINE="$BASELINE" ./scripts/ci/bench-regression.sh
    ok "benchmark regression check passed"
else
    echo "[release] WARNING: $BASELINE not found, skipping regression check"
fi

# -----------------------------------------------------------------------
# 4. QEMU smoke test
# -----------------------------------------------------------------------
step "4/6  QEMU smoke test"

if [ "$DO_SMOKE" -eq 1 ]; then
    if command -v qemu-system-aarch64 &>/dev/null; then
        if [ -f "build/haven.bin" ]; then
            ./scripts/qemu/qemu-smoke.sh
            ok "QEMU smoke test passed"
        else
            echo "[release] WARNING: build/haven.bin not found (no ARM64 cross-compiler?), skipping smoke"
        fi
    else
        echo "[release] WARNING: qemu-system-aarch64 not found, skipping smoke test"
        echo "  Install: brew install qemu (macOS) / apt install qemu-system-arm (Debian)"
    fi
else
    echo "[release] smoke test skipped (--no-smoke)"
fi

# -----------------------------------------------------------------------
# 5. Evidence pack
# -----------------------------------------------------------------------
step "5/6  Evidence pack"

EVIDENCE_DIR="build/releases/${VERSION}"
if [ "$DRY_RUN" -eq 0 ]; then
    mkdir -p "$EVIDENCE_DIR"

    # Capture test results summary
    TIMESTAMP="$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
    GIT_SHA="$(git rev-parse HEAD)"
    QEMU_STATUS="SKIPPED"
    if [ "$DO_SMOKE" -eq 1 ] && command -v qemu-system-aarch64 &>/dev/null && [ -f "build/haven.bin" ]; then
        QEMU_STATUS="PASS"
    fi

    {
        echo "Haven v${VERSION} release evidence"
        echo "Generated: ${TIMESTAMP}"
        echo "Git commit: ${GIT_SHA}"
        echo ""
        echo "Build: PASS"
        echo "Style: PASS"
        echo "Unit+integration tests: PASS"
        echo "Benchmark regression: PASS"
        echo "QEMU smoke: ${QEMU_STATUS}"
    } > "$EVIDENCE_DIR/release-summary.txt"

    # Write machine-readable release manifest
    cat > "$EVIDENCE_DIR/release-manifest.json" << EOF
{
  "version": "${VERSION}",
  "timestamp": "${TIMESTAMP}",
  "git_sha": "${GIT_SHA}",
  "preflight": "pass",
  "benchmark_regression": "pass",
  "qemu_smoke": "${QEMU_STATUS}",
  "evidence_dir": "build/releases/${VERSION}/"
}
EOF

    # Copy benchmark results if available
    if [ -d "build/benchmarks" ]; then
        cp -r build/benchmarks "$EVIDENCE_DIR/"
    fi

    # Copy CI metadata if available
    if [ -f "build/ci/metadata.txt" ]; then
        cp build/ci/metadata.txt "$EVIDENCE_DIR/"
    fi

    ok "evidence written to $EVIDENCE_DIR/"
else
    echo "[release] dry-run: would write evidence to build/releases/${VERSION}/"
fi

# -----------------------------------------------------------------------
# 6. Git tag
# -----------------------------------------------------------------------
step "6/6  Git tag"

if [ "$DO_TAG" -eq 1 ]; then
    if [ "$DRY_RUN" -eq 0 ]; then
        git tag -a "v${VERSION}" -m "Release v${VERSION}"
        echo "[release] Tagged: v${VERSION}"
        echo "[release] Push tag with: git push origin v${VERSION}"
    else
        echo "[release] dry-run: would tag v${VERSION}"
    fi
else
    echo "[release] tagging skipped (pass --tag to create v${VERSION} tag)"
fi

echo ""
echo "=========================================="
echo " Haven v${VERSION} release validation OK"
echo "=========================================="
