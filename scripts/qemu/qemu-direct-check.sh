#!/usr/bin/env bash
# Boot a qemu-direct HAVEN build in QEMU and check the partition A markers.
#
# Usage: scripts/qemu/qemu-direct-check.sh [build-dir] [cpu] [seconds]
#   build-dir  directory with haven.bin, guest_a.bin, guest_b.bin (default: build)
#   cpu        QEMU CPU model (default: cortex-a72)
#   seconds    boot time limit (default: 30)
#
# Writes <build-dir>/evidence/qemu-direct-<cpu>-uart.log and exits 0 only when
# every expected marker appears. Used by CI, by ELMOS (task haven:smoke) and
# by developers; runs on Linux and macOS.
set -euo pipefail

BUILD="${1:-build}"
CPU="${2:-cortex-a72}"
LIMIT="${3:-30}"
OUT="$BUILD/evidence"
LOG="$OUT/qemu-direct-$CPU-uart.log"
MARKERS=(
	"Hello from partition A"
	"Denied stage-2 violation"
	"PARTITION_A: Post-fault check OK"
)

for f in haven.bin guest_a.bin guest_b.bin; do
	[ -f "$BUILD/$f" ] || { echo "qemu-direct-check: missing $BUILD/$f (build with the qemu-direct profile first)"; exit 2; }
done
command -v qemu-system-aarch64 >/dev/null || { echo "qemu-direct-check: qemu-system-aarch64 not found"; exit 2; }

mkdir -p "$OUT"
rm -f "$LOG"
qemu-system-aarch64 \
	-machine virt,virtualization=on,gic-version=3,iommu=smmuv3 \
	-cpu "$CPU" -smp 4 -m 2G -display none -monitor none -nic none \
	-serial "file:$LOG" \
	-device "loader,file=$BUILD/haven.bin,addr=0x80000000,force-raw=on,cpu-num=0" \
	-device "loader,file=$BUILD/guest_a.bin,addr=0x80800000,force-raw=on" \
	-device "loader,file=$BUILD/guest_b.bin,addr=0xA0800000,force-raw=on" &
QPID=$!
trap 'kill "$QPID" 2>/dev/null || true' EXIT

elapsed=0
while [ "$elapsed" -lt "$((LIMIT * 5))" ]; do
	if [ -f "$LOG" ] && grep -q "${MARKERS[2]}" "$LOG"; then break; fi
	kill -0 "$QPID" 2>/dev/null || break
	sleep 0.2
	elapsed=$((elapsed + 1))
done
kill "$QPID" 2>/dev/null || true
wait "$QPID" 2>/dev/null || true

echo "---- UART ($LOG) ----"
cat "$LOG" 2>/dev/null || true
echo "---------------------"
missing=0
for m in "${MARKERS[@]}"; do
	if grep -qF "$m" "$LOG" 2>/dev/null; then echo "ok      $m"; else echo "MISSING $m"; missing=1; fi
done
[ "$missing" -eq 0 ] && echo "qemu-direct-check: PASS ($CPU)" || echo "qemu-direct-check: FAIL ($CPU)"
exit "$missing"
