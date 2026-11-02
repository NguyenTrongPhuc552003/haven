#!/bin/sh
# Try HAVEN without hardware: two partitions on two emulated CPUs, then
# read-back of the enforced state, then the same with a seeded fault that
# only read-back notices.
#
# Needs: qemu-system-aarch64 (9.2 or later for the DMA part), python3, dtc.
#   macOS:          brew install qemu dtc
#   Debian, Ubuntu: sudo apt install qemu-system-arm python3 device-tree-compiler
#   Windows:        run this inside WSL2 (Ubuntu) with the apt line above
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE"
OUT="$HERE/out"
DEV="virtio-rng-pci,disable-legacy=on,iommu_platform=on"

need() {
	command -v "$1" >/dev/null 2>&1 || { echo "try: missing $1. $2"; exit 2; }
}
need qemu-system-aarch64 "Install QEMU (see the top of this script)."
need python3 "Install Python 3."
need dtc "Install the device tree compiler (dtc or device-tree-compiler)."

ver=$(qemu-system-aarch64 --version | sed -n 's/.*version \([0-9]*\)\.\([0-9]*\).*/\1 \2/p')
set -- $ver
if [ "${1:-0}" -lt 9 ] || { [ "${1:-0}" -eq 9 ] && [ "${2:-0}" -lt 2 ]; }; then
	echo "try: QEMU ${1:-?}.${2:-?} found; 9.2 or later emulates the SMMU stage HAVEN uses for DMA."
	echo "     The run continues; expect the DMA grant to be withheld and read-back to say so."
fi
mkdir -p "$OUT"

echo
echo "== 1/3  Two partitions on two CPUs (about 30 s)"
echo "   Partition A (CPU 0) prints on the UART; partition B (CPU 2) has no UART"
echo "   and reports through its own memory, which this script reads over QMP."
python3 tools/partition-b-check.py images/good cortex-a72 60 | grep -E '^HAVEN: CPU 2|^PARTITION_A: (Hello|UART|duty)|^partition B status|^ok|^MISSING|^partition-b-check' || true

echo
echo "== 2/3  Read-back: what the hardware enforces, compared with the grant"
python3 tools/readback.py images/good grant.json "$OUT/good" --cpu cortex-a72 \
	--device "$DEV" --marker "IRQ test done" | grep -E '^RESULT|FINDING' || true

echo
echo "== 3/3  The same with a seeded fault: HAVEN also maps partition B's memory"
echo "   into its own address space. Both partitions still work..."
python3 tools/partition-b-check.py images/fault-el2-maps-b cortex-a72 60 | grep -E '^partition-b-check' || true
echo "   ...and only read-back notices:"
python3 tools/readback.py images/fault-el2-maps-b grant.json "$OUT/fault" --cpu cortex-a72 \
	--device "$DEV" --marker "IRQ test done" | grep -E '^RESULT|FINDING' || true

echo
echo "Full logs and reports: $OUT"
echo "What this shows and how to build it yourself: README.md"
