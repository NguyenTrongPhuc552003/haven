#!/usr/bin/env bash
# Build the HAVEN EFI tools (docs/methodology/stage2/20261109-uefi-loading/):
#   haven-probe.efi            read-only probe (L0)
#   haven-boot.efi             loader (L1b)
#   haven-boot-seed-mmu.efi    test-only: leaves the MMU on (never for a board)
#   haven-boot-seed-e2h.efi    test-only: sets HCR_EL2.E2H (never for a board)
#
# Usage: boot/efi/build.sh OUTDIR [CROSS_PREFIX]
#   CROSS_PREFIX  default: aarch64-linux-gnu- if found, else
#                 aarch64-unknown-linux-gnu-
# Fails if an ELF needs any relocation or has a .got, or if objcopy cannot
# write an AArch64 PE/COFF file. The loader's layout comes from HAVEN's
# qemu-virt platform header (boot/efi/layout.h).
set -euo pipefail

OUT="${1:?usage: build.sh OUTDIR [CROSS_PREFIX]}"
SRC="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SRC/../.." && pwd)"
CROSS="${2:-}"
if [ -z "$CROSS" ]; then
	if command -v aarch64-linux-gnu-gcc >/dev/null; then CROSS=aarch64-linux-gnu-; else CROSS=aarch64-unknown-linux-gnu-; fi
fi
mkdir -p "$OUT"

CFLAGS=(-std=gnu11 -Os -Wall -Wextra -Werror -ffreestanding -fno-builtin
	-fpie -fvisibility=hidden -fno-stack-protector -fno-asynchronous-unwind-tables
	-mgeneral-regs-only -mstrict-align -I"$SRC" -I"$ROOT/src/platform/qemu-virt")

cc() { "${CROSS}gcc" "${CFLAGS[@]}" "$@"; }

# link NAME OBJ... : ELF, relocation and .got checks, then PE/COFF
link() {
	local name="$1"; shift
	"${CROSS}ld" -nostdlib -static -T "$SRC/efi.lds" "$@" -o "$OUT/$name.elf"
	if "${CROSS}readelf" -r "$OUT/$name.elf" | grep -q "^Relocation section"; then
		echo "build.sh: $name.elf needs relocations" >&2
		exit 1
	fi
	if "${CROSS}readelf" -S "$OUT/$name.elf" | grep -q "\.got"; then
		echo "build.sh: $name.elf has a .got" >&2
		exit 1
	fi
	"${CROSS}objcopy" -O pei-aarch64-little --subsystem efi-app \
		--file-alignment 512 --section-alignment 4096 \
		"$OUT/$name.elf" "$OUT/$name.efi"
	echo "build.sh: $OUT/$name.efi ($(wc -c <"$OUT/$name.efi") bytes)"
}

"${CROSS}objcopy" --version | head -1
cc -c "$SRC/efi_lib.c" -o "$OUT/efi_lib.o"
cc -c "$SRC/probe.c" -o "$OUT/probe.o"
link haven-probe "$OUT/probe.o" "$OUT/efi_lib.o"

cc -c "$SRC/sha256.c" -o "$OUT/sha256.o"
cc -c "$SRC/boot.c" -o "$OUT/boot.o"
for v in "" seed-mmu seed-e2h; do
	def=()
	[ "$v" = seed-mmu ] && def=(-DSEED_MMU)
	[ "$v" = seed-e2h ] && def=(-DSEED_E2H)
	cc ${def[@]+"${def[@]}"} -c "$SRC/handoff.S" -o "$OUT/handoff${v:+-$v}.o"
	link "haven-boot${v:+-$v}" "$OUT/boot.o" "$OUT/handoff${v:+-$v}.o" "$OUT/sha256.o" "$OUT/efi_lib.o"
done
