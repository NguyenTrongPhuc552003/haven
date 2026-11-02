#!/usr/bin/env python3
"""Run and score the frozen A1 case set v3 (CASES-v3.md).

Usage:
  run_cases_v3.py <haven-repo> <out-dir> [case ...]      main set
  run_cases_v3.py <haven-repo> <out-dir> --check         edits apply, nothing built
  run_cases_v3.py <haven-repo> <out-dir> --held-out DIR  owner's held-out cases

Each case exports HEAD into a scratch copy, applies its change (main set:
each edit must match exactly once; held-out: DIR/NAME.patch with
`git apply`), builds the host tests and the qemu-direct image, then on
cortex-a72 and max runs read-back (tools/readback, memory read after
partition A prints "IRQ test done" or after 20 s), the host unit tests (as
CI) and the QEMU boot check. Writes per-case logs, results.json and
results.md. The repository is not modified. For held-out cases the
expected verdicts stay in DIR/NAME.expect.txt; they are copied next to the
results, not interpreted.
"""
import glob
import json
import os
import shutil
import subprocess
import sys
import tempfile

HERE = "docs/methodology/stage2/20261102-a1-cases-v3"
GRANT = f"{HERE}/grant-v3.json"
DEVICE = "virtio-rng-pci,disable-legacy=on,iommu_platform=on"
S2A = "arch/arm64/include/asm/s2_attrs.h"
MEM = "src/platform/qemu-virt/memory.h"
PART = "src/core/partition.c"
POLICY = "src/core/dma/smmu.c"
GIC = "drivers/irqchip/gic_v3.c"
SMMU = "drivers/iommu/smmu_v3.c"

CASES = {
    "M3": [(S2A, "\tif ((attrs & HV_MEM_X) == 0U) {\n\t\thw |= (uint64_t)S2_XN_NEVER;\n\t}\n",
            "\thw |= (uint64_t)S2_XN_NEVER;\n")],
    "M4": [(S2A, "\t\thw = (uint64_t)(S2_MEMATTR_DEVICE_nGnRE | S2_SH_NS);",
            "\t\thw = (uint64_t)(S2_MEMATTR_NORMAL_WB | S2_SH_IS);")],
    "M5": [(MEM, "#define PART_B_SIZE 0x04000000UL", "#define PART_B_SIZE 0x04200000UL")],
    "S8": [(PART, "\t\t.access = HV_DMA_RW,", "\t\t.access = HV_DMA_RO,")],
    "S9": [(POLICY, "\t\t\treturn HV_SMMU_DMA_VMID_BASE + i;", "\t\t\treturn 1U;")],
    "I7": [(GIC, "(gic.nr_irqs / 4); i++)\n\t\tgic_write32(gic.gicd_base, GICD_IPRIORITYR(i),\n"
                 "\t\t\t    GIC_PRIO_LINUX * 0x01010101U);",
            "(gic.nr_irqs / 4); i++)\n\t\tgic_write32(gic.gicd_base, GICD_IPRIORITYR(i),\n"
            "\t\t\t    GIC_PRIO_LOWEST * 0x01010101U);")],
    "I8": [(GIC, "\t\t    GICD_CTLR_ARE_NS | GICD_CTLR_ENABLE_G1A |\n\t\t\t    GICD_CTLR_ENABLE_G1);",
            "\t\t    GICD_CTLR_ARE_NS);")],
    "P1": [(PART, "\t\t.bar_pa = PART_A_DMA_BAR_PA,", "\t\t.bar_pa = PART_A_DMA_BAR_PA + 0x4000UL,")],
    "P2": [(PART, "\t\t.mmio_count = 2U,", "\t\t.mmio_count = 1U,")],
    "C6": [(SMMU, "\tuint32_t idx = smmu.cmdq_prod & SMMU_CMDQ_IDX_MASK;",
            "\tuint32_t idx = smmu.cmdq_prod % SMMU_CMDQ_ENTRIES;")],
    "C7": [(S2A, "\tif ((attrs & HV_MEM_DEVICE) != 0U) {\n\t\thw = (uint64_t)(S2_MEMATTR_DEVICE_nGnRE | S2_SH_NS);\n"
                 "\t} else {\n\t\thw = (uint64_t)(S2_MEMATTR_NORMAL_WB | S2_SH_IS);\n\t}",
            "\tif ((attrs & HV_MEM_DEVICE) == 0U) {\n\t\thw = (uint64_t)(S2_MEMATTR_NORMAL_WB | S2_SH_IS);\n"
            "\t} else {\n\t\thw = (uint64_t)(S2_MEMATTR_DEVICE_nGnRE | S2_SH_NS);\n\t}")],
}
# Owner-approved expected read-back findings (CASES-v3.md, all marked Y)
EXPECT = {
    "M3": [("A RAM", "execute drift"), ("B RAM", "execute drift")],
    "M4": [("A UART", "memory-type drift"), ("A UART", "shareability drift"),
           ("A PCI config", "memory-type drift"), ("A PCI config", "shareability drift"),
           ("A PCI BAR", "memory-type drift"), ("A PCI BAR", "shareability drift")],
    "M5": [("B RAM", "extent drift")],
    "S8": [("SID 8", "DMA access drift")],
    "S9": [],
    "I7": [],
    "I8": [("GIC", "GIC distributor drift")],
    "P1": [],
    "P2": [("A PCI BAR", "missing binding")],
    "C6": [],
    "C7": [],
}
# Cases whose empty expectation records a known blind spot, not a control
BLIND = {"S9", "I7", "P1"}
PREDICT = {  # (unit tests, boot check) as predicted in CASES-v3.md
    "M3": ("fail", "fail"), "M4": ("fail", "pass"), "M5": ("pass", "pass"), "S8": ("pass", "pass"),
    "S9": ("pass", "pass"), "I7": ("pass", "pass"), "I8": ("pass", "pass"), "P1": ("pass", "pass"),
    "P2": ("pass", "pass"), "C6": ("pass", "pass"), "C7": ("pass", "pass"),
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
        if s.count(old) != 1:
            raise SystemExit(f"edit does not match exactly once in {path} (found {s.count(old)})")
        open(p, "w").write(s.replace(old, new))


def export(repo, prefix):
    src = tempfile.mkdtemp(prefix=prefix)
    subprocess.run(f"git -C {repo} archive HEAD | tar -x -C {src}", shell=True, check=True)
    return src


def run_case(repo, src, cdir):
    readback = os.path.join(repo, "tools/readback/readback.py")
    log = os.path.join(cdir, "build.log")
    r = {}
    r["unit_rc"] = (sh("cmake --preset host-tests && cmake --build build-host", src, log) or
                    sh("ctest --test-dir build-host --output-on-failure --timeout 60 -E bench_temporal_isolation",
                       src, os.path.join(cdir, "ctest.log")))
    r["build_rc"] = sh("cmake --preset arm64-qemu -DHAVEN_BOOT_PROFILE=qemu-direct && cmake --build build", src, log)
    for cpu in CPUS:
        c = {"readback_rc": None, "boot_rc": None, "findings": [], "trigger": None}
        if r["build_rc"] == 0:
            rb_out = os.path.join(cdir, f"readback-{cpu}")
            p = subprocess.run([sys.executable, readback, os.path.join(src, "build"), os.path.join(repo, GRANT),
                                rb_out, "--cpu", cpu, "--wait", "20", "--device", DEVICE,
                                "--marker", "IRQ test done"], capture_output=True, text=True, env=ENV, timeout=300)
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
    return r


def table(results, head, title, expect):
    rows = [f"# {title} (haven {head})", "",
            "| Case | CPU | Read-back | Expected findings present | Extra findings | Unit tests | Boot check "
            "| Caught only by read-back |", "| - | - | - | - | - | - | - | - |"]
    for case, r in results.items():
        exp = expect.get(case)
        for cpu in CPUS:
            c = r[cpu]
            got = [tuple(x) for x in c["findings"]]
            rb = {0: "matches grant", 1: "findings", 2: "probe failure", None: "not run"}.get(c["readback_rc"], "?")
            if exp is None:
                present = "see expect.txt"
            elif c["readback_rc"] in (2, None):
                present = "no verdict"
            elif not exp:
                present = "yes" if c["readback_rc"] == 0 else "no (unexpected finding)"
            else:
                present = "yes" if all(e in got for e in exp) else "no"
            extra = sorted({f"{n}: {k}" for n, k in got if not exp or (n, k) not in exp})
            unit = "pass" if r["unit_rc"] == 0 else "fail"
            boot = {0: "pass", None: "not run"}.get(c["boot_rc"], "fail")
            if exp is None:
                only = "-"
            elif not exp:
                only = "blind spot" if case in BLIND else "control"
            else:
                only = "yes" if present == "yes" and unit == "pass" and boot == "pass" else "no"
            rows.append(f"| {case} | {cpu} | {rb} | {present} | {'; '.join(extra) or '-'} | {unit} | {boot} | {only} |")
    return "\n".join(rows) + "\n"


def main():
    repo, out = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    args = sys.argv[3:]
    head = subprocess.check_output(["git", "-C", repo, "rev-parse", "--short", "HEAD"], text=True).strip()
    if args[:1] == ["--check"]:
        for case, edits in CASES.items():
            src = export(repo, f"v3chk-{case}-")
            mutate(src, edits)
            shutil.rmtree(src)
            print(case, "edits apply")
        return
    held = None
    if args[:1] == ["--held-out"]:
        held = os.path.abspath(args[1])
        names = sorted(os.path.basename(p)[:-6] for p in glob.glob(os.path.join(held, "*.patch")))
        out = os.path.join(out, "held-out")
    else:
        names = args or list(CASES)
    os.makedirs(out, exist_ok=True)
    results = {}
    for case in names:
        cdir = os.path.join(out, case)
        os.makedirs(cdir, exist_ok=True)
        open(os.path.join(cdir, "build.log"), "w").close()
        src = export(repo, f"v3-{case}-")
        if held:
            patch = os.path.join(held, f"{case}.patch")
            rc = subprocess.run(["git", "apply", patch], cwd=src).returncode
            if rc != 0:
                rc = subprocess.run(["patch", "-p1", "-i", patch], cwd=src).returncode
            if rc != 0:
                raise SystemExit(f"{case}.patch does not apply")
            shutil.copy(patch, cdir)
            exp_file = os.path.join(held, f"{case}.expect.txt")
            if os.path.exists(exp_file):
                shutil.copy(exp_file, cdir)
        else:
            mutate(src, CASES[case])
            with open(os.path.join(cdir, "mutation.diff"), "w") as f:
                for path in sorted({e[0] for e in CASES[case]}):
                    subprocess.run(["diff", "-u", f"{repo}/{path}", f"{src}/{path}"], stdout=f)
        r = run_case(repo, src, cdir)
        r["case"] = case
        if not held:
            r["predicted"] = PREDICT[case]
        results[case] = r
        print(case, json.dumps(r), flush=True)
        shutil.rmtree(src)
    json.dump({"haven_commit": head, "results": results}, open(os.path.join(out, "results.json"), "w"), indent=1)
    title = "A1 case set v3, held-out cases" if held else "A1 case set v3 results"
    md = table(results, head, title, {} if held else EXPECT)
    open(os.path.join(out, "results.md"), "w").write(md)
    print(md)


if __name__ == "__main__":
    main()
