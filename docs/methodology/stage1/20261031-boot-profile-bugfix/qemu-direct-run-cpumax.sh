#!/bin/bash
# QEMU run of the qemu-direct profile on -cpu max (2026-10-31). Machine options from
# haven/scripts/qemu/qemu-run.sh; load addresses from its defaults.
# Usage: qemu-direct-run.sh <build-dir> <seconds>
B="$1"; T="${2:-20}"
cd "$B" || exit 2
gtimeout "$T" qemu-system-aarch64 \
  -machine virt,virtualization=on,gic-version=3,iommu=smmuv3 \
  -cpu max -smp 4 -m 2G -nographic \
  -device loader,file=haven.bin,addr=0x80000000,force-raw=on,cpu-num=0 \
  -device loader,file=guest_a.bin,addr=0x80800000,force-raw=on \
  -device loader,file=guest_b.bin,addr=0xA0800000,force-raw=on
echo "QEMU_RC=$? (124 = stopped by the timeout)"
