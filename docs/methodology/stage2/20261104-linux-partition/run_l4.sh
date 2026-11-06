#!/usr/bin/env bash
# L4: Linux with KDAL in HAVEN partition A; KDAL's demand against the grant;
# read-back of the enforced state against the same grant.
# Usage: run_l4.sh IMAGE KDAL_KO KDAL_SRC [OUTDIR]
#   IMAGE     uncompressed arm64 Linux Image (ELMOS demo-linux-v6.18-1)
#   KDAL_KO   kdal.ko built against IMAGE (same ELMOS release)
#   KDAL_SRC  a kdal checkout (for scripts/ci/kunit-init.c and mkcpio.py)
# Needs clang with lld (aarch64 target), dtc, python3, qemu-system-aarch64.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../../.." && pwd)
IMAGE=$1
KO=$2
KSRC=$3
OUT=${4:-$HERE/runs/l4}
DEV="virtio-rng-pci,disable-legacy=on,iommu_platform=on" # as the try bundle
INITRD_IPA=0x48000000                                   # PA = IPA + 0x40800000
mkdir -p "$OUT"
B=$OUT/build

echo "== HAVEN, Linux profile"
cmake -S "$ROOT" --preset arm64-qemu -B "$B" -DHAVEN_BOOT_PROFILE=qemu-direct \
	-DHAVEN_PART_A_DTB_IPA=0x50000000 >"$OUT/cmake.log" 2>&1
cmake --build "$B" >"$OUT/build.log" 2>&1

echo "== /init and the initramfs (KDAL's CI init and cpio writer)"
clang --target=aarch64-linux-gnu -ffreestanding -nostdlib -static -O2 \
	-fno-stack-protector -fuse-ld=lld -Wl,-e,_start \
	-o "$OUT/init" "$KSRC/scripts/ci/kunit-init.c"
python3 "$KSRC/scripts/ci/mkcpio.py" "$OUT/initramfs.cpio" "$OUT/init" "$KO"
SIZE=$(wc -c <"$OUT/initramfs.cpio" | tr -d ' ')
END=$(printf '0x%x' $((INITRD_IPA + SIZE)))

echo "== device tree: partition-a.dts + initrd + kdal,device over the PL061"
python3 - "$HERE/partition-a.dts" "$OUT/partition-a-l4.dts" "$INITRD_IPA" "$END" <<'PY'
import sys
src, dst, start, end = sys.argv[1:5]
s = open(src).read()
s = s.replace('stdout-path = "/pl011@9000000";',
              'stdout-path = "/pl011@9000000";\n\t\tlinux,initrd-start = <0x0 %s>;\n\t\tlinux,initrd-end = <0x0 %s>;' % (start, end), 1)
s = s.replace('panic=-1"', 'panic=-1 rdinit=/init"', 1)
node = ('\n\t/* KDAL device over QEMU virt\'s PL061 GPIO, as in ELMOS\'s demo device\n'
        '\t * tree. Partition A is not granted it (L4 expectation 2). */\n'
        '\tkdal-gpio@9030000 {\n\t\tcompatible = "kdal,device";\n'
        '\t\treg = <0x0 0x09030000 0x0 0x1000>;\n\t\tinterrupts = <0 7 4>;\n\t};\n};\n')
i = s.rstrip().rfind("};")
open(dst, "w").write(s[:i].rstrip() + "\n" + node)
PY
dtc -q -I dts -O dtb -o "$OUT/partition-a-l4.dtb" "$OUT/partition-a-l4.dts"
PA_INITRD=$(printf '0x%x' $((INITRD_IPA + 0x40800000)))
shasum -a 256 "$IMAGE" "$KO" "$OUT/init" "$OUT/initramfs.cpio" "$OUT/partition-a-l4.dtb" \
	"$B/haven.bin" >"$OUT/SHA256SUMS"

echo "== run (until Linux powers off)"
timeout 60 qemu-system-aarch64 \
	-machine virt,virtualization=on,gic-version=3,iommu=smmuv3 \
	-cpu cortex-a72 -smp 4 -m 2G -display none -monitor none -nic none \
	-device "$DEV" -serial "file:$OUT/uart.log" \
	-device "loader,file=$B/haven.bin,addr=0x80000000,force-raw=on,cpu-num=0" \
	-device "loader,file=$IMAGE,addr=0x80800000,force-raw=on" \
	-device "loader,file=$OUT/partition-a-l4.dtb,addr=0x90800000,force-raw=on" \
	-device "loader,file=$OUT/initramfs.cpio,addr=$PA_INITRD,force-raw=on" \
	-device "loader,file=$B/guest_b.bin,addr=0xA0800000,force-raw=on" || true
echo "uart: $OUT/uart.log ($(wc -l <"$OUT/uart.log") lines)"

echo "== KDAL's demand against the grant"
sed -n '/KDAL-DEMAND-BEGIN/,/KDAL-DEMAND-END/p' "$OUT/uart.log" | sed '1d;$d' | tr -d '\r' >"$OUT/demand.json"
python3 "$ROOT/tools/readback/demand_check.py" "$HERE/grant-l4.json" "$OUT/demand.json" \
	--partition 1 >"$OUT/demand-check.txt" 2>&1 || echo "demand_check exit $?" >>"$OUT/demand-check.txt"
cat "$OUT/demand-check.txt"

echo "== read-back against the same grant"
python3 "$ROOT/tools/readback/readback.py" "$B" "$HERE/grant-l4.json" "$OUT/readback" \
	--cpu cortex-a72 --device "$DEV" --wait 60 --marker KDAL-DEMAND-END \
	--guest-a "$IMAGE" --load "$OUT/partition-a-l4.dtb@0x90800000" \
	--load "$OUT/initramfs.cpio@$PA_INITRD" | tee "$OUT/readback.txt" | grep -E '^RESULT|FINDING|read at|probe' || true
