#!/bin/bash
# Feasibility runs for the read-only region and the SMMU findings SM1/SM2
# (not a scored case set). Usage: try_v2b.sh <haven> <out>
set -uo pipefail
H="$1"; OUT="$2"
export PATH=/opt/homebrew/bin:/opt/homebrew/Cellar/aarch64-unknown-linux-gnu/15.2.0/toolchain/bin:$PATH
A="$H/docs/methodology/stage1/20261101-a1-readback"
RB="$H/tools/readback/readback.py"
mkdir -p "$OUT"
python3 - "$A/grant-qemu-v2.json" "$OUT/grant-sid8-to-A.json" <<'PY'
import json, sys
g = json.load(open(sys.argv[1]))
g["source"] += "; plus the PCIe device (hardware SID 8) granted to partition A with a 1 MiB DMA window at IPA 0x40000000"
g["smmu"]["streams"] = {"8": {"partition": 1, "window": {"ipa": "0x40000000", "size": "0x100000"}}}
json.dump(g, open(sys.argv[2], "w"), indent=1)
PY

run() { # case grant
    local c="$1" grant="$2" src="/tmp/rb2b-$1"
    rm -rf "$src"; mkdir -p "$src"
    git -C "$H" archive HEAD | tar -x -C "$src"
    rsync -a "$H/src/" "$src/src/"; rsync -a "$H/drivers/" "$src/drivers/"   # include uncommitted work tree
    python3 - "$src" "$c" <<'PY'
import sys
src, case = sys.argv[1], sys.argv[2]
GRANT = ("\thv_printk(\"HAVEN: partition B mapped",
         "\t{\n\t\thv_u16 sid;\n"
         "\t\tif (hv_smmu_allocate_streamid(PARTITION_A_ID, &sid) == HV_OK)\n"
         "\t\t\t(void)hv_smmu_configure_dma_window(sid, PART_A_IPA_BASE, 0x100000UL,\n"
         "\t\t\t\t\t\t\t    PART_A_PA_BASE, HV_DMA_RW);\n\t}\n"
         "\thv_printk(\"HAVEN: partition B mapped")
EDITS = {
 "R1": [("arch/arm64/include/asm/s2_attrs.h", "re:hw \\|= \\(\\(attrs & HV_MEM_W\\) != 0U\\) \\? \\(uint64_t\\)S2AP_RW :\\s*\\(uint64_t\\)S2AP_RO;", "hw |= (uint64_t)S2AP_RW;")],
 "S1": [("drivers/iommu/smmu_v3.c", "\tste[0] = STE_0_VALID | STE_0_CONFIG_ABORT;", "\tste[0] = STE_0_VALID | STE_0_CONFIG_BYPASS;")],
 "S5": [("src/core/partition.c",) + GRANT],
 "S6": [("src/core/partition.c",) + GRANT,
        ("src/core/dma/smmu.c", "\tfor (hv_u16 i = 0; i < HV_MAX_SMMU_DEVICES; i++) {\n\t\tif (!smmu_state.streamid_allocated[i]) {",
         "\tfor (hv_u16 i = 8; i < HV_MAX_SMMU_DEVICES; i++) {\n\t\tif (!smmu_state.streamid_allocated[i]) {")],
}
import re
for path, old, new in EDITS[case]:
    p = f"{src}/{path}"; s = open(p).read()
    if old.startswith("re:"):
        s2, n = re.subn(old[3:], new, s)
    else:
        n = s.count(old); s2 = s.replace(old, new)
    if n != 1:
        sys.exit(f"{case}: edit matches {n} times in {path}")
    if case in ("S5", "S6") and path.endswith("partition.c") and "#include <haven/smmu.h>" not in s2:
        s2 = s2.replace("#include <haven/stage2.h>", "#include <haven/smmu.h>\n#include <haven/stage2.h>", 1)
    open(p, "w").write(s2)
PY
    (cd "$src" && cmake --preset arm64-qemu -DHAVEN_BOOT_PROFILE=qemu-direct >/dev/null && cmake --build build >/dev/null) || { echo "$c: build failed"; return; }
    for cpu in cortex-a72 max; do
        echo "=== $c $cpu"
        python3 "$RB" "$src/build" "$grant" "$OUT/$c-$cpu" --cpu "$cpu" --wait 8 \
            --device virtio-rng-pci > "$OUT/$c-$cpu.txt" 2>&1
        echo "exit=$?" >> "$OUT/$c-$cpu.txt"
        grep -E "read at|A RO|DMA |FINDING|RESULT" "$OUT/$c-$cpu.txt" | grep -v "^  H ipa=0x04\|^  H ipa=0x06"
    done
    (cd "$src" && ./scripts/qemu/qemu-direct-check.sh build cortex-a72 | tail -1)
    rm -rf "$src"
}
run R1 "$A/grant-qemu-v2.json"
run S1 "$A/grant-qemu-v2.json"
run S5 "$OUT/grant-sid8-to-A.json"
run S6 "$OUT/grant-sid8-to-A.json"
