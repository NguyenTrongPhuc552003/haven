#!/usr/bin/env bash
# Haven QEMU virt runner - ARM64 two-partition isolation demo
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
HAVEN_BIN="${ROOT}/build/haven.bin"
GUEST_A_BIN="${ROOT}/build/guest_a.bin"
GUEST_B_BIN="${ROOT}/build/guest_b.bin"
HV_LOAD_ADDR="${HV_LOAD_ADDR:-0x80000000}"
GUEST_A_LOAD_ADDR="${GUEST_A_LOAD_ADDR:-0x80800000}"
# Partition B PA base: PART_A_PA_BASE (0x80800000) + PART_A_SIZE (0x20000000)
GUEST_B_LOAD_ADDR="${GUEST_B_LOAD_ADDR:-0xA0800000}"

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
echo "[qemu-run] Hypervisor: $HAVEN_BIN @ $HV_LOAD_ADDR"
if [ -f "$GUEST_A_BIN" ]; then
    echo "[qemu-run] Guest A:    $GUEST_A_BIN @ $GUEST_A_LOAD_ADDR"
else
    echo "[qemu-run] Guest A:    not found (run: make ARCH=arm64 all)"
fi
if [ -f "$GUEST_B_BIN" ]; then
    echo "[qemu-run] Guest B:    $GUEST_B_BIN @ $GUEST_B_LOAD_ADDR"
else
    echo "[qemu-run] Guest B:    not found (run: make ARCH=arm64 all)"
fi
echo "[qemu-run] Press Ctrl-A X to quit QEMU"
echo ""

# Build optional guest loader arguments.
# Haven stage-2 maps:
#   PA 0x80800000 → IPA 0x40000000 for Partition A (PART_A_PA_BASE → PART_A_IPA_BASE)
#   PA 0xA0800000 → IPA 0x60000000 for Partition B (PART_B_PA_BASE → PART_B_IPA_BASE)
GUEST_LOADER=""
if [ -f "$GUEST_A_BIN" ]; then
    GUEST_LOADER="-device loader,file=${GUEST_A_BIN},addr=${GUEST_A_LOAD_ADDR},force-raw=on"
fi
if [ -f "$GUEST_B_BIN" ]; then
    GUEST_LOADER="${GUEST_LOADER} -device loader,file=${GUEST_B_BIN},addr=${GUEST_B_LOAD_ADDR},force-raw=on"
fi

# Load Haven at HV_LOAD_ADDR. cpu-num=0 sets CPU0's initial PC so execution
# begins at _start (EL2).  Secondary CPUs (1-3) are parked in the boot stub
# until Haven starts them via PSCI or direct ERET.
qemu-system-aarch64 \
    -machine virt,virtualization=on,gic-version=3,iommu=smmuv3 \
    -cpu cortex-a72 \
    -smp 4 \
    -m 2G \
    -nographic \
    -device loader,file="$HAVEN_BIN",addr="$HV_LOAD_ADDR",force-raw=on,cpu-num=0 \
    ${GUEST_LOADER} \
    "${@}"

