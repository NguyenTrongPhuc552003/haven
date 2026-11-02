#!/usr/bin/env python3
"""Partition B checks: B runs on CPU 2 under stage-2 and takes its own
virtual timer interrupt (PPI 27). Seen from inside (partition B's status
record and HAVEN's log, scripts/qemu/partition-b-check.py) and from outside
(read-back v3 against the proposed grant, and against the v3 grant).

Usage: run_partb.py <haven-repo> <out-dir> [check ...]
Each check exports HEAD into a scratch copy, applies its change (each edit
must match exactly once), builds the qemu-psci image, then on cortex-a72
and max runs partition-b-check.py and read-back. Writes per-check logs,
results.json and results.md. The repository is not modified.

Expectations are Claude's, written before the first run of each control.
"""
import json
import os
import subprocess
import sys
import tempfile

HERE = "docs/methodology/stage2/20261102-partition-b"
GRANT = f"{HERE}/grant-partb.json"
GRANT_V3 = "docs/methodology/stage2/20261102-a1-cases-v3/grant-v3.json"
PART = "src/core/partition.c"
DEVICE = "virtio-rng-pci,disable-legacy=on,iommu_platform=on"
NO_CPU_INIT = [(PART, "\thv_arch_cpu_init();\n\thv_arch_phys_irq_cpu_init();\n", "")]
B_TIMER = """	{.irq_id = PART_B_VTIMER_PPI,
	 .owner_partition_id = PARTITION_B_ID,
	 .target_cpu = PART_B_CPU},"""
OWNER_A = [(PART, B_TIMER, B_TIMER.replace("PARTITION_B_ID", "PARTITION_A_ID"))]
WRONG_CPU = [(PART, B_TIMER, B_TIMER.replace(".target_cpu = PART_B_CPU", ".target_cpu = 0U"))]
# (edits, partition-b-check passes?, missing check names expected,
#  read-back findings expected against grant-partb, against grant-v3)
CHECKS = {
    "K1": ([], True, [], [], [("CPU 2 PPIs", "interrupt enable drift")]),
    # B2 reintroduced: CPU 2 never runs B. Read-back reads configuration,
    # not whether a CPU entered its partition: no finding expected.
    "K2": (NO_CPU_INIT, False, ["B's read of A's IPA denied on CPU 2", "B status magic",
                                "B reached stage 5 (done)"], [], [("CPU 2 PPIs", "interrupt enable drift")]),
    # PPI 27 on CPU 2 owned by A: HAVEN refuses to inject into B and
    # disables the PPI. Read-back does not flag a granted PPI that is
    # disabled (it only flags enabled PPIs without a grant): no finding.
    "K3": (OWNER_A, False, ["B timer interrupts >= 8", "no undelivered interrupt"], [], []),
    # PPI 27 granted on the wrong CPU: it never fires on CPU 2.
    "K4": (WRONG_CPU, False, ["B timer interrupts >= 8"], [("CPU 0 PPIs", "interrupt enable drift")],
           [("CPU 0 PPIs", "interrupt enable drift")]),
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


def readback(repo, src, grant, out, cpu):
    p = subprocess.run([sys.executable, os.path.join(repo, "tools/readback/readback.py"), os.path.join(src, "build"),
                        os.path.join(repo, grant), out, "--cpu", cpu, "--wait", "20", "--device", DEVICE,
                        "--marker", "IRQ test done"], capture_output=True, text=True, env=ENV, timeout=300)
    open(out + ".txt", "w").write(p.stdout + p.stderr)
    try:
        return p.returncode, [tuple(f[:2]) for f in json.load(open(os.path.join(out, "readback.json")))["findings"]]
    except (OSError, ValueError, KeyError):
        return p.returncode, None


def main():
    repo, out = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    only = sys.argv[3:] or list(CHECKS)
    head = subprocess.check_output(["git", "-C", repo, "rev-parse", "--short", "HEAD"], text=True).strip()
    os.makedirs(out, exist_ok=True)
    results = {}
    for name in only:
        edits, want_pass, want_missing, want_rb, want_rb_v3 = CHECKS[name]
        cdir = os.path.join(out, name)
        os.makedirs(cdir, exist_ok=True)
        log = os.path.join(cdir, "build.log")
        open(log, "w").close()
        src = tempfile.mkdtemp(prefix=f"partb-{name}-")
        subprocess.run(f"git -C {repo} archive HEAD | tar -x -C {src}", shell=True, check=True)
        mutate(src, edits)
        with open(os.path.join(cdir, "change.diff"), "w") as f:
            for path in sorted({e[0] for e in edits}):
                subprocess.run(["diff", "-u", f"{repo}/{path}", f"{src}/{path}"], stdout=f)
        r = {"check": name, "expect_pass": want_pass, "expect_missing": want_missing,
             "expect_readback": want_rb, "expect_readback_v3": want_rb_v3}
        r["build_rc"] = sh("cmake --preset arm64-qemu -DHAVEN_BOOT_PROFILE=qemu-psci && cmake --build build", src, log)
        for cpu in CPUS:
            c = {}
            if r["build_rc"] == 0:
                p = subprocess.run([sys.executable, os.path.join(src, "scripts/qemu/partition-b-check.py"),
                                    os.path.join(src, "build"), cpu, "30"],
                                   capture_output=True, text=True, env=ENV, timeout=120)
                open(os.path.join(cdir, f"partb-{cpu}.txt"), "w").write(p.stdout + p.stderr)
                c["partb_rc"] = p.returncode
                c["missing"] = [l[8:] for l in p.stdout.splitlines() if l.startswith("MISSING ")]
                c["status"] = next((l for l in p.stdout.splitlines() if l.startswith("partition B status")), "")
                c["readback_rc"], c["findings"] = readback(repo, src, GRANT, os.path.join(cdir, f"readback-{cpu}"), cpu)
                c["readback_v3_rc"], c["findings_v3"] = readback(repo, src, GRANT_V3,
                                                                 os.path.join(cdir, f"readback-v3-{cpu}"), cpu)
            r[cpu] = c
        results[name] = r
        print(name, json.dumps(r), flush=True)
        subprocess.run(["rm", "-rf", src])
    json.dump({"haven_commit": head, "results": results}, open(os.path.join(out, "results.json"), "w"), indent=1)

    def verdict(got, want):
        if got is None:
            return "no report"
        return "yes" if sorted(set(got)) == sorted(set(want)) else "no"

    rows = [f"# Partition B checks (haven {head})", "",
            "| Check | CPU | Partition B check | As expected | Missing | Read-back (grant-partb) | As expected "
            "| Read-back (grant-v3) | As expected |",
            "| - | - | - | - | - | - | - | - | - |"]
    for name, r in results.items():
        for cpu in CPUS:
            c = r[cpu]
            if not c:
                rows.append(f"| {name} | {cpu} | build failed | no | - | - | - | - | - |")
                continue
            passed = c["partb_rc"] == 0
            pok = passed == r["expect_pass"] and all(m in c["missing"] for m in r["expect_missing"])
            fmt = lambda f: "; ".join(f"{a}: {b}" for a, b in f) if f else ("none" if f is not None else "no report")
            rows.append(f"| {name} | {cpu} | {'PASS' if passed else 'FAIL'} | {'yes' if pok else 'no'} | "
                        f"{'; '.join(c['missing']) or '-'} | {fmt(c['findings'])} | "
                        f"{verdict(c['findings'], r['expect_readback'])} | {fmt(c['findings_v3'])} | "
                        f"{verdict(c['findings_v3'], r['expect_readback_v3'])} |")
    open(os.path.join(out, "results.md"), "w").write("\n".join(rows) + "\n")
    print("\n".join(rows))


if __name__ == "__main__":
    main()
