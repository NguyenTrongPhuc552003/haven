#!/usr/bin/env bash
# Host unit tests on a clean copy of COMMIT, once unpatched and once per
# fault patch. Usage: run_unit.sh COMMIT OUTDIR
set -euo pipefail
COMMIT=$1
OUT=$2
ROOT=$(git rev-parse --show-toplevel)
HERE=$(cd "$(dirname "$0")" && pwd)
CASES=$HERE/../20261106-linux-chain-cases
mkdir -p "$OUT"

run() { # name patch-or-empty
	local name=$1 patch=$2 w
	w=$(mktemp -d)
	git -C "$ROOT" archive "$COMMIT" | tar -x -C "$w"
	if [ -n "$patch" ]; then
		patch -s -p1 -d "$w" <"$patch"
	fi
	(cd "$w" && cmake --preset host-tests >/dev/null &&
		cmake --build build-host -j8 >/dev/null &&
		cd build-host && ctest -E bench_temporal_isolation --output-on-failure) \
		>"$OUT/$name.log" 2>&1 || true
	grep -E 'tests passed|Failed|\*\*\*' "$OUT/$name.log" | sed "s/^/$name: /"
	rm -rf "$w"
}

run base ""
run H1 "$CASES/patches/H1.patch"
run H3 "$CASES/patches/H3.patch"
run R1 "$CASES/heldout/R1.patch"
run R2 "$CASES/heldout/R2.patch"
run R3 "$HERE/R3-sysreg_trap.patch"
