#!/usr/bin/env bash
# Build the HAVEN EFI tools (today: haven-probe.efi, L0 of
# docs/methodology/stage2/20261109-uefi-loading/).
#
# Usage: boot/efi/build.sh OUTDIR [CROSS_PREFIX]
#   CROSS_PREFIX  default: aarch64-linux-gnu- if found, else
#                 aarch64-unknown-linux-gnu-
# Fails if the ELF needs any relocation, or if objcopy cannot write an
# AArch64 PE/COFF file.
set -euo pipefail

OUT="${1:?usage: build.sh OUTDIR [CROSS_PREFIX]}"
SRC="$(cd "$(dirname "$0")" && pwd)"
CROSS="${2:-}"
if [ -z "$CROSS" ]; then
	if command -v aarch64-linux-gnu-gcc >/dev/null; then CROSS=aarch64-linux-gnu-; else CROSS=aarch64-unknown-linux-gnu-; fi
fi
mkdir -p "$OUT"

"${CROSS}gcc" -std=gnu11 -Os -Wall -Wextra -Werror -ffreestanding -fno-builtin \
	-fpie -fvisibility=hidden -fno-stack-protector -fno-asynchronous-unwind-tables \
	-mgeneral-regs-only -mstrict-align -c "$SRC/probe.c" -o "$OUT/probe.o"
"${CROSS}ld" -nostdlib -static -T "$SRC/efi.lds" "$OUT/probe.o" -o "$OUT/haven-probe.elf"

if "${CROSS}readelf" -r "$OUT/haven-probe.elf" | grep -q "^Relocation section"; then
	echo "build.sh: haven-probe.elf needs relocations" >&2
	exit 1
fi
"${CROSS}objcopy" --version | head -1
"${CROSS}objcopy" -O pei-aarch64-little --subsystem efi-app \
	--file-alignment 512 --section-alignment 4096 \
	"$OUT/haven-probe.elf" "$OUT/haven-probe.efi"
echo "build.sh: $OUT/haven-probe.efi ($(wc -c <"$OUT/haven-probe.efi") bytes)"
