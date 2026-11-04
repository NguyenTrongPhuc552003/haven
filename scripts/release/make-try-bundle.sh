#!/usr/bin/env bash
# Build the "try HAVEN without hardware" bundle from the committed HEAD.
#
# Usage: scripts/release/make-try-bundle.sh [out-dir]   (default: dist)
# Produces <out-dir>/haven-try-<describe>.tar.gz containing two prebuilt
# qemu-psci images (correct, and with one seeded fault), symbol lists so no
# cross binutils are needed, the check tools, the grant and try.sh.
# Needs the ARM64 toolchain used by the arm64-qemu preset and an aarch64 nm.
set -euo pipefail

REPO=$(cd "$(dirname "$0")/../.." && pwd)
OUT=$(mkdir -p "${1:-dist}" && cd "${1:-dist}" && pwd)
DESC=$(git -C "$REPO" describe --tags --always --dirty)
NM=$(command -v aarch64-unknown-linux-gnu-nm || command -v aarch64-linux-gnu-nm)
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
NAME="haven-try-$DESC"
STAGE="$WORK/$NAME"
mkdir -p "$STAGE/images" "$STAGE/tools"

build() { # build <variant> [python edit script]
	local src="$WORK/src-$1"
	mkdir -p "$src"
	git -C "$REPO" archive HEAD | tar -x -C "$src"
	if [ -n "${2:-}" ]; then (cd "$src" && python3 -c "$2"); fi
	(cd "$src" && cmake --preset arm64-qemu -B build -DHAVEN_BOOT_PROFILE=qemu-psci >/dev/null &&
		cmake --build build >/dev/null)
	mkdir -p "$STAGE/images/$1"
	cp "$src/build/haven.bin" "$src/build/guest_a.bin" "$src/build/guest_b.bin" "$STAGE/images/$1/"
	"$NM" "$src/build/haven.elf" >"$STAGE/images/$1/haven.nm"
	"$NM" "$src/build/guest_b.elf" >"$STAGE/images/$1/guest_b.nm"
}

build good
# Seeded fault (control N2 in docs/methodology/stage2/20261102-el2-memory):
# partition B's first 2 MiB also mapped at EL2 as Normal memory.
build fault-el2-maps-b "
p = 'src/platform/qemu-virt/platform.c'
s = open(p).read()
old = '\t{HAVEN_LOAD_PA, HAVEN_SIZE, HV_EL2_NORMAL},\n'
assert s.count(old) == 1, 'fault edit does not apply'
open(p, 'w').write(s.replace(old, old + '\t{0xA0800000UL, 0x200000UL, HV_EL2_NORMAL},\n'))
"

cp "$REPO/tools/readback/readback.py" "$REPO/tools/readback/grant.py" "$STAGE/tools/"
cp "$REPO/scripts/qemu/partition-b-check.py" "$STAGE/tools/"
cp "$REPO/docs/methodology/stage2/20261102-budgets/grant-budget.json" "$STAGE/grant.json"
cp "$REPO/scripts/release/try/try.sh" "$REPO/scripts/release/try/README.md" "$STAGE/"
chmod +x "$STAGE/try.sh"
{
	echo "haven $DESC ($(git -C "$REPO" rev-parse HEAD))"
	echo "built $(date -u +%Y-%m-%dT%H:%M:%SZ) on $(uname -sm)"
	"$(dirname "$NM")/$(basename "$NM" | sed 's/nm$/gcc/')" --version 2>/dev/null | head -1 || true
	(cd "$STAGE" && find images tools grant.json try.sh -type f | sort | xargs shasum -a 256)
} >"$STAGE/BUILD-INFO.txt"

tar -C "$WORK" -czf "$OUT/$NAME.tar.gz" "$NAME"
(cd "$OUT" && shasum -a 256 "$NAME.tar.gz" >"$NAME.tar.gz.sha256")
echo "$OUT/$NAME.tar.gz"
