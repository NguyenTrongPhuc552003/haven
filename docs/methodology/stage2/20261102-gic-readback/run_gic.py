#!/usr/bin/env python3
"""GIC read-back feasibility: seeded interrupt-ownership faults, scored by
read-back (tools/readback, v3 with GIC), host unit tests and the QEMU boot
check.

Usage: run_gic.py <haven-repo> <out-dir> [case ...]
Each case exports HEAD into a scratch copy, applies its change (each edit
must match exactly once), builds host tests and the qemu-direct image, then
runs read-back and the boot check on cortex-a72 and max. Writes per-case
logs, results.json and results.md. The repository is not modified.
"""
import json
import os
import subprocess
import sys
import tempfile

GRANT = "docs/methodology/stage2/20261102-gic-readback/grant-gic.json"
GIC = "drivers/irqchip/gic_v3.c"
OWN = "src/core/irq/ownership.c"
PART = "src/core/partition.c"
CASES = {
    "I0": [],
    "I1": [(PART, "{.irq_id = 33U, .owner_partition_id = PARTITION_A_ID, .target_cpu = 0U},",
            "{.irq_id = 33U, .owner_partition_id = PARTITION_A_ID, .target_cpu = 2U},")],
    "I2": [(GIC, "\t\t    gicd_irouter_from_mpidr(mpidr));",
            "\t\t    gicd_irouter_from_mpidr(mpidr) | GICD_IROUTER_IRM);")],
    "I3": [(GIC, "\t\tgic_write32(gic.gicd_base, GICD_ISENABLER(irq / 32),\n\t\t\t    1U << (irq % 32));",
            "\t\tgic_write32(gic.gicd_base, GICD_ISENABLER(irq / 32),\n\t\t\t    0xffffffffU);")],
    "I4": [(GIC, "GICD_IGROUPR(i), 0xffffffff);", "GICD_IGROUPR(i), 0U);")],
    "I5": [(GIC, "gic_write32(base, GICR_ISENABLER0, 0x0000ffffU);", "gic_write32(base, GICR_ISENABLER0, 0xffffffffU);")],
    "I6": [(OWN, "\t\tgic_v3_enable_irq(route->irq_id);\n", "")],
    "C1": [(GIC, "\tgic_write64(gic.gicd_base, GICD_IROUTER(irq),\n\t\t    gicd_irouter_from_mpidr(mpidr));",
            "\t{\n\t\tuint64_t route = gicd_irouter_from_mpidr(mpidr);\n\n"
            "\t\tgic_write64(gic.gicd_base, GICD_IROUTER(irq), route);\n\t}")],
}
# Expected read-back findings (Claude's proposal; feasibility, not owner-approved)
EXPECT = {
    "I0": [],
    "I1": [("SPI 33", "interrupt routing drift")],
    "I2": [("SPI 33", "interrupt routing drift")],
    "I3": [("GIC", "interrupt enable drift")],
    "I4": [("SPI 33", "interrupt group drift")],
    "I5": [("CPU 0 PPIs", "interrupt enable drift")],
    "I6": [("SPI 33", "missing interrupt binding")],
    "C1": [],
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


def main():
    repo, out = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    only = sys.argv[3:] or list(CASES)
    head = subprocess.check_output(["git", "-C", repo, "rev-parse", "--short", "HEAD"], text=True).strip()
    readback = os.path.join(repo, "tools/readback/readback.py")
    os.makedirs(out, exist_ok=True)
    results = {}
    for case in only:
        cdir = os.path.join(out, case)
        os.makedirs(cdir, exist_ok=True)
        log = os.path.join(cdir, "build.log")
        open(log, "w").close()
        src = tempfile.mkdtemp(prefix=f"gic-{case}-")
        subprocess.run(f"git -C {repo} archive HEAD | tar -x -C {src}", shell=True, check=True)
        mutate(src, CASES[case])
        with open(os.path.join(cdir, "change.diff"), "w") as f:
            for path in sorted({e[0] for e in CASES[case]}):
                subprocess.run(["diff", "-u", f"{repo}/{path}", f"{src}/{path}"], stdout=f)
        r = {"case": case}
        r["unit_rc"] = (sh("cmake --preset host-tests && cmake --build build-host", src, log) or
                        sh("ctest --test-dir build-host --output-on-failure --timeout 60 -E bench_temporal_isolation",
                           src, os.path.join(cdir, "ctest.log")))
        r["build_rc"] = sh("cmake --preset arm64-qemu -DHAVEN_BOOT_PROFILE=qemu-direct && cmake --build build", src, log)
        for cpu in CPUS:
            c = {"readback_rc": None, "boot_rc": None, "findings": []}
            if r["build_rc"] == 0:
                rb_out = os.path.join(cdir, f"readback-{cpu}")
                p = subprocess.run([sys.executable, readback, os.path.join(src, "build"), os.path.join(repo, GRANT),
                                    rb_out, "--cpu", cpu, "--wait", "10"],
                                   capture_output=True, text=True, env=ENV, timeout=300)
                open(os.path.join(cdir, f"readback-{cpu}.txt"), "w").write(p.stdout + p.stderr)
                c["readback_rc"] = p.returncode
                try:
                    c["findings"] = [f[:2] for f in json.load(open(os.path.join(rb_out, "readback.json")))["findings"]]
                except (OSError, ValueError, KeyError):
                    pass
                c["boot_rc"] = sh(f"./scripts/qemu/qemu-direct-check.sh build {cpu}", src,
                                  os.path.join(cdir, f"boot-{cpu}.log"), 120)
            r[cpu] = c
        results[case] = r
        print(case, json.dumps(r), flush=True)
        subprocess.run(["rm", "-rf", src])
    json.dump({"haven_commit": head, "results": results}, open(os.path.join(out, "results.json"), "w"), indent=1)

    rows = [f"# GIC read-back feasibility (haven {head})", "",
            "| Case | CPU | Read-back | Expected findings present | Extra findings | Unit tests | Boot check | Read-back only |",
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
            only_rb = "control" if not exp else ("yes" if present == "yes" and unit == "pass" and boot == "pass" else "no")
            rows.append(f"| {case} | {cpu} | {rb} | {present} | {'; '.join(extra) or '-'} | {unit} | {boot} | {only_rb} |")
    open(os.path.join(out, "results.md"), "w").write("\n".join(rows) + "\n")
    print("\n".join(rows))


if __name__ == "__main__":
    main()
