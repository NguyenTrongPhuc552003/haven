#!/usr/bin/env python3
"""Run the frozen A1 case set v2 (CASES-v2.md) and score it.

Usage: run_cases_v2.py <haven-repo> <out-dir> [case ...]
For each case: export HEAD into a scratch copy, apply the seeded change (each
edit must match exactly once), build host tests and the qemu-direct image, then
run on cortex-a72 and max: read-back (tools/readback/readback.py, probe v2),
host unit tests (ctest, as in CI) and the QEMU boot check. Writes per-case logs,
results.json and results.md. The repository itself is never modified.
"""
import json
import os
import re
import subprocess
import sys
import tempfile

A1 = "docs/methodology/stage1/20261101-a1-readback"
S2A = "arch/arm64/include/asm/s2_attrs.h"
SMMU = "drivers/iommu/smmu_v3.c"
PART = "src/core/partition.c"
POLICY = "src/core/dma/smmu.c"
MAPPED_B = "\thv_printk(\"HAVEN: partition B mapped"


def grant_call(part, ipa, pa):
    return ("\t{\n\t\thv_u16 sid;\n"
            f"\t\tif (hv_smmu_allocate_streamid({part}, &sid) == HV_OK)\n"
            f"\t\t\t(void)hv_smmu_configure_dma_window(sid, {ipa}, 0x100000UL,\n"
            f"\t\t\t\t\t\t\t    {pa}, HV_DMA_RW);\n\t}}\n" + MAPPED_B)


INCLUDE = ("#include <haven/stage2.h>", "#include <haven/smmu.h>\n#include <haven/stage2.h>")
FROM_8 = ("\tfor (hv_u16 i = 0; i < HV_MAX_SMMU_DEVICES; i++) {\n\t\tif (!smmu_state.streamid_allocated[i]) {",
          "\tfor (hv_u16 i = 8; i < HV_MAX_SMMU_DEVICES; i++) {\n\t\tif (!smmu_state.streamid_allocated[i]) {")
ABORT_BODY = ("\t/* Word 0: VALID=1, CONFIG=ABORT */\n\tste[0] = STE_0_VALID | STE_0_CONFIG_ABORT;\n"
              "\t/* Words 1-7: zero (unused for ABORT) */\n\tfor (int i = 1; i < SMMU_STE_DWORDS; i++)\n\t\tste[i] = 0;\n")

# (file, old, new); old must occur exactly once; "re:" prefix = regular expression
CASES = {
    "R1": [(S2A, r"re:hw \|= \(\(attrs & HV_MEM_W\) != 0U\) \? \(uint64_t\)S2AP_RW :\s*\(uint64_t\)S2AP_RO;",
            "hw |= (uint64_t)S2AP_RW;")],
    "S1": [(SMMU, "\tste[0] = STE_0_VALID | STE_0_CONFIG_ABORT;", "\tste[0] = STE_0_VALID | STE_0_CONFIG_BYPASS;")],
    "S2": [(SMMU, "\tsmmu_write32(base, SMMU_CR0, CR0_SMMU_ENABLE);\n\tsmmu_wait_cr0ack(base, CR0_SMMU_ENABLE);\n", "")],
    "S5": [(PART, INCLUDE[0], INCLUDE[1]),
           (PART, MAPPED_B, grant_call("PARTITION_A_ID", "PART_A_IPA_BASE", "PART_A_PA_BASE"))],
    "S6": [(PART, INCLUDE[0], INCLUDE[1]),
           (PART, MAPPED_B, grant_call("PARTITION_A_ID", "PART_A_IPA_BASE", "PART_A_PA_BASE")),
           (POLICY, FROM_8[0], FROM_8[1])],
    "S7": [(PART, INCLUDE[0], INCLUDE[1]),
           (PART, MAPPED_B, grant_call("PARTITION_B_ID", "PART_B_IPA_BASE", "PART_B_PA_BASE")),
           (POLICY, FROM_8[0], FROM_8[1])],
    "U1": [],
    "C1": [],
    "C5": [(SMMU, ABORT_BODY,
            "\t/* Words 1-7 first, then word 0: same STE bytes */\n\tfor (int i = 1; i < SMMU_STE_DWORDS; i++)\n"
            "\t\tste[i] = 0;\n\tste[0] = STE_0_VALID | STE_0_CONFIG_ABORT;\n")],
}
GRANT = {c: "grant-qemu-v2.json" for c in CASES}
GRANT.update({"S5": "grant-sid8-to-A.json", "S6": "grant-sid8-to-A.json", "S7": "grant-sid8-to-A.json"})
DEVICES = {c: ["virtio-rng-pci"] for c in CASES}
DEVICES["U1"] = ["virtio-rng-pci", "virtio-rng-device"]
# Approved expected read-back findings, as (name, class); empty = no finding
EXPECT = {
    "R1": [("A RO", "access drift")],
    "S1": [("pci-1af4:1005", "DMA enforcement drift"), ("SMMU", "extra stream binding")],
    "S2": [("SMMU", "SMMU enable drift"), ("pci-1af4:1005", "DMA enforcement drift")],
    "S5": [("pci-1af4:1005", "DMA enforcement drift"), ("SMMU", "extra stream binding")],
    "S6": [("SID 8", "DMA extent drift")],
    "S7": [("pci-1af4:1005", "DMA enforcement drift")],
    "U1": [("virtio-rng-device", "unenforced DMA master")],
    "C1": [], "C5": [],
}
CPUS = ["cortex-a72", "max"]
TOOLCHAIN = "/opt/homebrew/Cellar/aarch64-unknown-linux-gnu/15.2.0/toolchain/bin"
ENV = dict(os.environ, PATH=f"/opt/homebrew/bin:/usr/local/bin:{TOOLCHAIN}:" + os.environ.get("PATH", ""))


def sh(cmd, cwd, log, timeout=900):
    with open(log, "a") as f:
        f.write(f"$ {cmd}\n")
        f.flush()
        try:
            return subprocess.run(cmd, cwd=cwd, shell=True, stdout=f, stderr=subprocess.STDOUT,
                                  env=ENV, timeout=timeout).returncode
        except subprocess.TimeoutExpired:
            f.write("TIMEOUT\n")
            return 124


def mutate(src, edits):
    for path, old, new in edits:
        p = os.path.join(src, path)
        s = open(p).read()
        if old.startswith("re:"):
            s2, n = re.subn(old[3:], new, s)
        else:
            n = s.count(old)
            s2 = s.replace(old, new)
        if n != 1:
            raise SystemExit(f"edit does not match exactly once in {path} (found {n})")
        open(p, "w").write(s2)


def main():
    repo, out = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    only = sys.argv[3:] or list(CASES)
    head = subprocess.check_output(["git", "-C", repo, "rev-parse", "--short", "HEAD"], text=True).strip()
    readback = os.path.join(repo, "tools/readback/readback.py")
    results = {}
    for case in only:
        cdir = os.path.join(out, case)
        os.makedirs(cdir, exist_ok=True)
        log = os.path.join(cdir, "build.log")
        open(log, "w").close()
        src = tempfile.mkdtemp(prefix=f"a1v2-{case}-")
        subprocess.run(f"git -C {repo} archive HEAD | tar -x -C {src}", shell=True, check=True)
        mutate(src, CASES[case])
        with open(os.path.join(cdir, "mutation.diff"), "w") as f:
            for path in sorted({e[0] for e in CASES[case]}):
                subprocess.run(["diff", "-u", f"{repo}/{path}", f"{src}/{path}"], stdout=f)
        r = {"case": case, "grant": GRANT[case], "devices": DEVICES[case]}
        r["unit_rc"] = (sh("cmake --preset host-tests && cmake --build build-host", src, log) or
                        sh("ctest --test-dir build-host --output-on-failure --timeout 60 -E bench_temporal_isolation",
                           src, os.path.join(cdir, "ctest.log")))
        r["build_rc"] = sh("cmake --preset arm64-qemu -DHAVEN_BOOT_PROFILE=qemu-direct && cmake --build build", src, log)
        for cpu in CPUS:
            c = {"readback_rc": None, "boot_rc": None, "findings": [], "trigger": None}
            if r["build_rc"] == 0:
                rb_out = os.path.join(cdir, f"readback-{cpu}")
                args = [sys.executable, readback, os.path.join(src, "build"), os.path.join(repo, A1, GRANT[case]),
                        rb_out, "--cpu", cpu, "--wait", "8"]
                for d in DEVICES[case]:
                    args += ["--device", d]
                p = subprocess.run(args, capture_output=True, text=True, env=ENV, timeout=240)
                open(os.path.join(cdir, f"readback-{cpu}.txt"), "w").write(p.stdout + p.stderr)
                c["readback_rc"] = p.returncode
                try:
                    rep = json.load(open(os.path.join(rb_out, "readback.json")))
                    c["findings"] = [f[:2] for f in rep["findings"]]
                    c["trigger"] = rep["trigger"]
                except (OSError, ValueError, KeyError):
                    pass
                c["boot_rc"] = sh(f"./scripts/qemu/qemu-direct-check.sh build {cpu}", src,
                                  os.path.join(cdir, f"boot-{cpu}.log"), 120)
            r[cpu] = c
        results[case] = r
        print(case, json.dumps(r), flush=True)
        subprocess.run(["rm", "-rf", src])
    json.dump({"haven_commit": head, "results": results}, open(os.path.join(out, "results.json"), "w"), indent=1)

    rows = ["| Case | CPU | Read-back | Expected findings present | Extra findings | Unit tests | Boot check | Read-back only |",
            "| - | - | - | - | - | - | - | - |"]
    for case, r in results.items():
        exp = EXPECT[case]
        for cpu in CPUS:
            c = r[cpu]
            got = [tuple(x) for x in c["findings"]]
            rb = {0: "matches grant", 1: "findings", 2: "probe failure", None: "not run"}.get(c["readback_rc"], "?")
            if c["readback_rc"] in (2, None):
                present = "no verdict"
            elif not exp:
                present = "yes" if c["readback_rc"] == 0 else "no (false finding)"
            else:
                present = "yes" if all(e in got for e in exp) else "no"
            extra = sorted({f"{n}: {k}" for n, k in got if (n, k) not in exp})
            unit = "pass" if r["unit_rc"] == 0 else "fail"
            boot = {0: "pass", None: "not run"}.get(c["boot_rc"], "fail")
            only = "control" if not exp else ("yes" if present == "yes" and unit == "pass" and boot == "pass" else "no")
            rows.append(f"| {case} | {cpu} | {rb} | {present} | {'; '.join(extra) or '-'} | {unit} | {boot} | {only} |")
    open(os.path.join(out, "results.md"), "w").write(f"# A1 case set v2 results (haven {head})\n\n" + "\n".join(rows) + "\n")
    print("\n".join(rows))


if __name__ == "__main__":
    main()
