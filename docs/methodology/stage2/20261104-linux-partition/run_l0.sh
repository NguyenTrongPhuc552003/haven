#!/usr/bin/env bash
# L0: boot a Linux kernel Image as HAVEN partition A in QEMU (qemu-direct).
# Usage: run_l0.sh IMAGE [OUTDIR] [SECONDS]
#   IMAGE   uncompressed arm64 Linux Image (for example ELMOS's demo kernel)
# Builds HAVEN with HAVEN_PART_A_DTB_IPA=0x50000000, compiles partition-a.dts
# and loads: HAVEN at PA 0x80000000, the Image at PA 0x80800000 (IPA
# 0x40000000), the device tree at PA 0x90800000 (IPA 0x50000000).
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../../.." && pwd)
IMAGE=$1
OUT=${2:-$HERE/runs/l0}
SECS=${3:-30}
mkdir -p "$OUT"
B=$OUT/build
cmake -S "$ROOT" --preset arm64-qemu -B "$B" -DHAVEN_BOOT_PROFILE=qemu-direct \
	-DHAVEN_PART_A_DTB_IPA=0x50000000 >"$OUT/cmake.log" 2>&1
cmake --build "$B" >"$OUT/build.log" 2>&1
dtc -q -I dts -O dtb -o "$OUT/partition-a.dtb" "$HERE/partition-a.dts"
shasum -a 256 "$IMAGE" "$OUT/partition-a.dtb" "$B/haven.bin" >"$OUT/SHA256SUMS"
timeout "$SECS" qemu-system-aarch64 \
	-machine virt,virtualization=on,gic-version=3,iommu=smmuv3 \
	-cpu cortex-a72 -smp 4 -m 2G -display none -monitor none -nic none \
	-serial "file:$OUT/uart.log" \
	-device "loader,file=$B/haven.bin,addr=0x80000000,force-raw=on,cpu-num=0" \
	-device "loader,file=$IMAGE,addr=0x80800000,force-raw=on" \
	-device "loader,file=$OUT/partition-a.dtb,addr=0x90800000,force-raw=on" \
	-device "loader,file=$B/guest_b.bin,addr=0xA0800000,force-raw=on" || true
echo "uart: $OUT/uart.log ($(wc -l <"$OUT/uart.log") lines)"
