#!/usr/bin/env bash
# Haven QEMU virt runner - ARM64 two-partition isolation demo
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
HAVEN_BIN="${ROOT}/build/haven.bin"
HV_LOAD_ADDR="${HV_LOAD_ADDR:-0x80000000}"

# Check build exists
if [ ! -f "$HAVEN_BIN" ]; then
    echo "[qemu-run] ERROR: $HAVEN_BIN not found. Run: make ARCH=arm64 all"
    exit 1
fi

# Check for qemu-system-aarch64
if ! command -v qemu-system-aarch64 &>/dev/null; then
    echo "[qemu-run] ERROR: qemu-system-aarch64 not found."
    echo "  Install: brew install qemu  (macOS)"
    echo "           apt install qemu-system-arm  (Debian/Ubuntu)"
    exit 1
fi

echo "[qemu-run] Launching Haven on QEMU virt (arm64, GICv3, SMMUv3)"
echo "[qemu-run] Binary: $HAVEN_BIN"
echo "[qemu-run] Press Ctrl-A X to quit QEMU"
echo ""

# Load Haven at HV_LOAD_ADDR. The loader device sets both the binary in
# memory and CPU0's initial PC so execution begins at _start (EL2).
# ARM64 has no reset vector at 0x0; cpu-num=0 in the loader is sufficient.
qemu-system-aarch64 \
    -machine virt,virtualization=on,gic-version=3,iommu=smmuv3 \
    -cpu cortex-a72 \
    -smp 4 \
    -m 2G \
    -nographic \
    -device loader,file="$HAVEN_BIN",addr="$HV_LOAD_ADDR",force-raw=on,cpu-num=0 \
    "${@}"
