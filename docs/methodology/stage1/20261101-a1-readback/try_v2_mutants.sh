#!/bin/bash
# Feasibility runs for probe v2 (not a scored case set): build disposable
# mutants of HAVEN HEAD and read them back. Usage: try_mutants.sh <haven> <out>
set -uo pipefail
H="$1"; OUT="$2"
export PATH=/opt/homebrew/bin:/opt/homebrew/Cellar/aarch64-unknown-linux-gnu/15.2.0/toolchain/bin:$PATH
G="$H/docs/methodology/stage1/20261101-a1-readback/grant-qemu-g1.json"
RB="$H/tools/readback/readback.py"
mkdir -p "$OUT"

mutate() { # case file python-replacement-pair...
    local c="$1" src="/tmp/rb2-$1"
    rm -rf "$src"; mkdir -p "$src"
    git -C "$H" archive HEAD | tar -x -C "$src"
    python3 - "$src" "$c" <<'PY'
import sys
src, case = sys.argv[1], sys.argv[2]
EDITS = {
 "S1": [("drivers/iommu/smmu_v3.c", "\tste[0] = STE_0_VALID | STE_0_CONFIG_ABORT;", "\tste[0] = STE_0_VALID | STE_0_CONFIG_BYPASS;")],
 "S2": [("drivers/iommu/smmu_v3.c", "\tsmmu_write32(base, SMMU_CR0, CR0_SMMU_ENABLE);\n\tsmmu_wait_cr0ack(base, CR0_SMMU_ENABLE);\n", "")],
 "D1": [("arch/arm64/mm.c", "(*l2)[l2_idx] = (cur_pa & HV_BLOCK_MASK_L2) |", "(*l2)[l2_idx] = ((cur_pa + HV_BLOCK_SIZE_L2) & HV_BLOCK_MASK_L2) |")],
 "D6": [("arch/arm64/mm.c", "\t\t\t\t\tS2_DESC_BLOCK | hw_attrs | S2_AF;", "\t\t\t\t\tS2_DESC_BLOCK | hw_attrs;")],
}
for path, old, new in EDITS[case]:
    p = f"{src}/{path}"; s = open(p).read()
    n = s.count(old)
    if n != 1:
        sys.exit(f"{case}: edit matches {n} times in {path}")
    open(p, "w").write(s.replace(old, new))
PY
    (cd "$src" && cmake --preset arm64-qemu -DHAVEN_BOOT_PROFILE=qemu-direct >/dev/null && cmake --build build >/dev/null) || { echo "$c: build failed"; return; }
    for cpu in cortex-a72 max; do
        echo "=== $c $cpu"
        python3 "$RB" "$src/build" "$G" "$OUT/$c-$cpu" --cpu "$cpu" --wait 8 \
            --device virtio-rng-pci > "$OUT/$c-$cpu.txt" 2>&1
        echo "exit=$?" >> "$OUT/$c-$cpu.txt"
        grep -E "read at|SMMU enabled|DMA |FINDING|RESULT|exit=" "$OUT/$c-$cpu.txt"
    done
    rm -rf "$src"
}
for c in S1 S2 D1 D6; do mutate "$c"; done
