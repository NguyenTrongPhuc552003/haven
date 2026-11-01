#!/usr/bin/env python3
"""Verify the SM1 to SM3 fixes with external read-back (tools/readback, v3).

Usage: run_checks.py <haven-repo> <out-dir> [check ...]
Each check exports HEAD into a scratch copy, applies its change (each edit
must match exactly once), builds the qemu-direct image and runs, on
cortex-a72 and max: read-back with virtio-rng-pci (hardware SID 8), the QEMU
boot check, and for checks with "trace" a 6 s QEMU run that records the
SMMU command-queue trace. The host unit tests run once on HEAD. Writes
per-check logs, results.json and results.md. The repository is not modified.
"""
import json
import os
import re
import subprocess
import sys
import tempfile

HERE = "docs/methodology/stage1/20261102-smmu-bugfix"
A1 = "docs/methodology/stage1/20261101-a1-readback"
PART = "src/core/partition.c"
POLICY = "src/core/dma/smmu.c"
MM = "arch/arm64/mm.c"
MAPPED_B = "\thv_printk(\"HAVEN: partition B mapped"
INCLUDE = ("#include <haven/stage2.h>", "#include <haven/smmu.h>\n#include <haven/stage2.h>")


def grant_call(bind, access, pa="PART_A_PA_BASE"):
    """Grant the PCIe device to partition A with a 1 MiB window at A's IPA base."""
    if bind == "assign":
        sid = "\t\thv_u16 sid = 8;\n\t\tif (hv_smmu_assign_streamid(PARTITION_A_ID, sid) == HV_OK)\n"
    else:
        sid = "\t\thv_u16 sid;\n\t\tif (hv_smmu_allocate_streamid(PARTITION_A_ID, &sid) == HV_OK)\n"
    return ("\t{\n" + sid +
            "\t\t\thv_printk(\"HAVEN: DMA grant SID %u rc=%d\\n\", (unsigned)sid,\n"
            "\t\t\t\t  (int)hv_smmu_configure_dma_window(sid, PART_A_IPA_BASE, 0x100000UL,\n"
            f"\t\t\t\t\t\t\t\t      {pa}, {access}));\n\t}}\n" + MAPPED_B)


GRANT_RW = [(PART, *INCLUDE), (PART, MAPPED_B, grant_call("assign", "HV_DMA_RW"))]
GRANT_RO = [(PART, *INCLUDE), (PART, MAPPED_B, grant_call("assign", "HV_DMA_RO"))]
# (edits, grant file, QEMU SMMU stage, trace, expected read-back findings as (name, class))
CHECKS = {
    "B0": ([], f"{A1}/grant-qemu-v2.json", "nested", True, []),
    "B1": ([], f"{A1}/grant-qemu-v2.json", "1", False, []),
    "F1": (GRANT_RW, f"{HERE}/grant-sid8-A-rw.json", "nested", True, []),
    "F2": (GRANT_RO, f"{HERE}/grant-sid8-A-r.json", "nested", False, []),
    "F3": (GRANT_RW, f"{HERE}/grant-sid8-A-rw.json", "1", False,
           [("pci-1af4:1005", "DMA enforcement drift")]),
    "M1": (GRANT_RW + [(POLICY, "\t\t\trc = smmu_v3_set_ste_partition(streamid, vttbr, vmid);",
                        "\t\t\textern uint64_t hv_arch_stage2_vttbr(uint32_t, uint32_t);\n"
                        "\t\t\trc = smmu_v3_set_ste_partition(\n\t\t\t\tstreamid,\n"
                        "\t\t\t\thv_arch_stage2_vttbr(\n\t\t\t\t\tsmmu_state.streamid_to_partition[streamid],\n"
                        "\t\t\t\t\tvmid),\n\t\t\t\tvmid);")],
           f"{HERE}/grant-sid8-A-rw.json", "nested", False, [("SID 8", "DMA extent drift")]),
    "M2": ([(PART, *INCLUDE), (PART, MAPPED_B, grant_call("allocate", "HV_DMA_RW"))],
           f"{HERE}/grant-sid8-A-rw.json", "nested", False,
           [("pci-1af4:1005", "DMA enforcement drift"), ("SMMU", "extra stream binding")]),
    "M3": (GRANT_RO + [(MM, "\t\t\t\t  ((uint64_t)access << S2AP_SHIFT));",
                        "\t\t\t\t  ((uint64_t)3U << S2AP_SHIFT));")],
           f"{HERE}/grant-sid8-A-r.json", "nested", False, [("SID 8", "DMA access drift")]),
    "M4": ([(PART, *INCLUDE), (PART, MAPPED_B, grant_call("assign", "HV_DMA_RW", "PART_A_PA_BASE + 0x100000UL"))],
           f"{HERE}/grant-sid8-A-rw.json", "nested", False, [("SID 8", "DMA association drift")]),
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


def trace(src, cdir, stage):
    log = os.path.join(cdir, "smmu-trace.log")
    machine = "virt" if stage == "nested" else "virt-9.1"
    glob = "-global arm-smmuv3.stage=2 " if stage == "2" else ""
    cmd = (f"timeout 6 qemu-system-aarch64 -machine {machine},virtualization=on,gic-version=3,iommu=smmuv3 "
           f"{glob}-cpu cortex-a72 -smp 4 -m 2G -display none -monitor none "
           f"-nic none -device virtio-rng-pci -serial file:{cdir}/trace-uart.log "
           f"-d guest_errors,unimp -D {log} -trace 'smmuv3_cmdq*' -trace smmuv3_decode_cd "
           f"-device loader,file=build/haven.bin,addr=0x80000000,force-raw=on,cpu-num=0 "
           f"-device loader,file=build/guest_a.bin,addr=0x80800000,force-raw=on "
           f"-device loader,file=build/guest_b.bin,addr=0xA0800000,force-raw=on")
    sh(cmd, src, os.path.join(cdir, "trace-cmd.log"), 30)
    lines = open(log).read().splitlines() if os.path.exists(log) else []
    return [x for x in lines if "opcode" in x or "error" in x.lower() or "cfgi" in x or "tlbi" in x]


def main():
    repo, out = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    only = sys.argv[3:] or list(CHECKS)
    head = subprocess.check_output(["git", "-C", repo, "rev-parse", "--short", "HEAD"], text=True).strip()
    readback = os.path.join(repo, "tools/readback/readback.py")
    os.makedirs(out, exist_ok=True)
    unit_log = os.path.join(out, "ctest.log")
    open(unit_log, "w").close()
    src0 = tempfile.mkdtemp(prefix="smmufix-unit-")
    subprocess.run(f"git -C {repo} archive HEAD | tar -x -C {src0}", shell=True, check=True)
    unit_rc = (sh("cmake --preset host-tests && cmake --build build-host", src0, unit_log) or
               sh("ctest --test-dir build-host --output-on-failure --timeout 60 -E bench_temporal_isolation",
                  src0, unit_log))
    subprocess.run(["rm", "-rf", src0])
    results = {}
    for name in only:
        edits, grant, stage, want_trace, expect = CHECKS[name]
        cdir = os.path.join(out, name)
        os.makedirs(cdir, exist_ok=True)
        log = os.path.join(cdir, "build.log")
        open(log, "w").close()
        src = tempfile.mkdtemp(prefix=f"smmufix-{name}-")
        subprocess.run(f"git -C {repo} archive HEAD | tar -x -C {src}", shell=True, check=True)
        mutate(src, edits)
        with open(os.path.join(cdir, "change.diff"), "w") as f:
            for path in sorted({e[0] for e in edits}):
                subprocess.run(["diff", "-u", f"{repo}/{path}", f"{src}/{path}"], stdout=f)
        r = {"check": name, "grant": grant, "smmu_stage": stage, "expect": expect}
        r["build_rc"] = sh("cmake --preset arm64-qemu -DHAVEN_BOOT_PROFILE=qemu-direct && cmake --build build", src, log)
        if r["build_rc"] == 0 and want_trace:
            r["trace"] = trace(src, cdir, stage)
        for cpu in CPUS:
            c = {"readback_rc": None, "boot_rc": None, "findings": [], "grant_line": None}
            if r["build_rc"] == 0:
                rb_out = os.path.join(cdir, f"readback-{cpu}")
                args = [sys.executable, readback, os.path.join(src, "build"), os.path.join(repo, grant), rb_out,
                        "--cpu", cpu, "--wait", "8", "--device", "virtio-rng-pci", "--smmu-stage", stage]
                p = subprocess.run(args, capture_output=True, text=True, env=ENV, timeout=240)
                open(os.path.join(cdir, f"readback-{cpu}.txt"), "w").write(p.stdout + p.stderr)
                c["readback_rc"] = p.returncode
                try:
                    c["findings"] = [f[:2] for f in json.load(open(os.path.join(rb_out, "readback.json")))["findings"]]
                except (OSError, ValueError, KeyError):
                    pass
                uart = os.path.join(rb_out, "uart.log")
                if os.path.exists(uart):
                    m = re.search(r"HAVEN: DMA grant SID \d+ rc=-?\d+", open(uart, errors="replace").read())
                    c["grant_line"] = m.group(0) if m else None
                c["boot_rc"] = sh(f"./scripts/qemu/qemu-direct-check.sh build {cpu}", src,
                                  os.path.join(cdir, f"boot-{cpu}.log"), 120)
            r[cpu] = c
        results[name] = r
        print(name, json.dumps(r), flush=True)
        subprocess.run(["rm", "-rf", src])
    json.dump({"haven_commit": head, "unit_rc": unit_rc, "results": results},
              open(os.path.join(out, "results.json"), "w"), indent=1)

    rows = [f"# SMMU fix checks (haven {head}; host unit tests: {'pass' if unit_rc == 0 else 'fail'})", "",
            "| Check | CPU | SMMU stage | HAVEN grant call | Read-back | Expected findings | Extra findings | Boot check |",
            "| - | - | - | - | - | - | - | - |"]
    for name, r in results.items():
        exp = [tuple(e) for e in r["expect"]]
        for cpu in CPUS:
            c = r[cpu]
            got = [tuple(x) for x in c["findings"]]
            rb = {0: "matches grant", 1: "findings", 2: "probe failure", None: "not run"}.get(c["readback_rc"], "?")
            if c["readback_rc"] in (2, None):
                ok = "no verdict"
            elif not exp:
                ok = "yes" if c["readback_rc"] == 0 else "no"
            else:
                ok = "yes" if all(e in got for e in exp) else "no"
            extra = sorted({f"{n}: {k}" for n, k in got if (n, k) not in exp})
            boot = {0: "pass", None: "not run"}.get(c["boot_rc"], "fail")
            rows.append(f"| {name} | {cpu} | {r['smmu_stage']} | {c['grant_line'] or '-'} | {rb} | {ok} | "
                        f"{'; '.join(extra) or '-'} | {boot} |")
    open(os.path.join(out, "results.md"), "w").write("\n".join(rows) + "\n")
    print("\n".join(rows))


if __name__ == "__main__":
    main()
