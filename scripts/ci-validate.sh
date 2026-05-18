#!/usr/bin/env sh
# ci-validate.sh — Structured V1/V2/V3 validation transcript
#
# Runs the three validation levels defined in
# docs/methodology/VIRTUAL_PLATFORM_VALIDATION.md and writes a structured
# transcript to build/evidence/validation-transcript.json.
#
# Validation levels:
#   V1 — Environment sanity: toolchain + shell compatibility
#   V2 — Repository preflight: build, style-check, test, config-check
#   V3 — QEMU workflow sanity: qemu available + invocation + artifact collection
#
# Exit code:
#   0  All levels that could run have passed.
#   1  One or more mandatory levels failed.
set -eu

OUTDIR="build/evidence"
TRANSCRIPT="${OUTDIR}/validation-transcript.json"
TIMESTAMP=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
GIT_COMMIT=$(git rev-parse HEAD 2>/dev/null || echo "unknown")
PLATFORM=$(uname -srm)

mkdir -p "$OUTDIR"

run_ok=0
run_fail=1

# -----------------------------------------------------------------------
# Helpers
# -----------------------------------------------------------------------
level_result() {
	# $1=level $2=status $3=note
	printf '    { "level": "%s", "status": "%s", "note": "%s" }' "$1" "$2" "$3"
}

check_cmd() {
	# Returns 0 if command exists, 1 otherwise
	command -v "$1" >/dev/null 2>&1
}

# -----------------------------------------------------------------------
# V1 — Environment sanity
# -----------------------------------------------------------------------
echo "[ci-validate] V1: environment sanity"

V1_STATUS="pass"
V1_NOTE=""

# Toolchain
if ! check_cmd cc && ! check_cmd gcc && ! check_cmd clang; then
	V1_STATUS="fail"
	V1_NOTE="no C compiler found"
	echo "[ci-validate] V1 FAIL: no C compiler"
else
	CC_BIN=$(command -v "${CC:-cc}" 2>/dev/null || echo "cc")
	V1_NOTE="compiler=${CC_BIN}"
	echo "[ci-validate] V1: compiler OK (${CC_BIN})"
fi

# Shell compatibility (run ci-preflight.sh through sh -n)
if ! sh -n scripts/ci-preflight.sh 2>/dev/null; then
	V1_STATUS="fail"
	V1_NOTE="${V1_NOTE} shell-syntax-error"
	echo "[ci-validate] V1 FAIL: shell syntax error in ci-preflight.sh"
else
	echo "[ci-validate] V1: shell syntax OK"
fi

echo "[ci-validate] V1 done: ${V1_STATUS}"

# -----------------------------------------------------------------------
# V2 — Repository preflight
# -----------------------------------------------------------------------
echo "[ci-validate] V2: repository preflight"

V2_STATUS="pass"
V2_NOTE=""
V2_LOG="${OUTDIR}/v2-preflight.txt"

if ./scripts/ci-preflight.sh > "$V2_LOG" 2>&1; then
	V2_NOTE="all preflight steps passed"
	echo "[ci-validate] V2: preflight passed"
else
	V2_STATUS="fail"
	V2_NOTE="preflight failed — see v2-preflight.txt"
	echo "[ci-validate] V2 FAIL: preflight failed (see ${V2_LOG})"
fi

echo "[ci-validate] V2 done: ${V2_STATUS}"

# -----------------------------------------------------------------------
# V3 — QEMU workflow sanity
# -----------------------------------------------------------------------
echo "[ci-validate] V3: QEMU workflow sanity"

V3_STATUS="skipped"
V3_NOTE="qemu-system-aarch64 not found"

if check_cmd qemu-system-aarch64; then
	QEMU_VER=$(qemu-system-aarch64 --version 2>&1 | head -n 1)
	echo "[ci-validate] V3: QEMU found: ${QEMU_VER}"

	V3_LOG="${OUTDIR}/v3-qemu.txt"
	if ./scripts/qemu-smoke.sh > "$V3_LOG" 2>&1; then
		V3_STATUS="pass"
		V3_NOTE="${QEMU_VER}"
		echo "[ci-validate] V3: QEMU smoke passed"
	else
		V3_STATUS="fail"
		V3_NOTE="qemu smoke failed — see v3-qemu.txt"
		echo "[ci-validate] V3 FAIL: qemu smoke failed (see ${V3_LOG})"
	fi
else
	echo "[ci-validate] V3: QEMU not available — skipped"
fi

echo "[ci-validate] V3 done: ${V3_STATUS}"

# -----------------------------------------------------------------------
# Emit structured transcript JSON
# -----------------------------------------------------------------------
cat > "$TRANSCRIPT" << EOF
{
  "timestamp_utc": "${TIMESTAMP}",
  "git_commit": "${GIT_COMMIT}",
  "platform": "${PLATFORM}",
  "levels": [
$(level_result "V1" "${V1_STATUS}" "${V1_NOTE}"),
$(level_result "V2" "${V2_STATUS}" "${V2_NOTE}"),
$(level_result "V3" "${V3_STATUS}" "${V3_NOTE}")
  ]
}
EOF

echo "[ci-validate] transcript written to ${TRANSCRIPT}"

# -----------------------------------------------------------------------
# Overall exit
# -----------------------------------------------------------------------
OVERALL_FAIL=0
if [ "$V1_STATUS" = "fail" ] || [ "$V2_STATUS" = "fail" ]; then
	OVERALL_FAIL=1
fi
# V3 "skipped" is not a failure (QEMU may not be installed on all hosts)
if [ "$V3_STATUS" = "fail" ]; then
	OVERALL_FAIL=1
fi

if [ "$OVERALL_FAIL" -ne 0 ]; then
	echo "[ci-validate] FAILED — one or more mandatory levels failed"
	exit 1
fi

echo "[ci-validate] all levels passed"
