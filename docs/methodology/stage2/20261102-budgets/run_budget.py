#!/usr/bin/env python3
"""Budget enforcement checks: CPU time budgets enforced by Haven's EL2 timer.

Usage: run_budget.py <haven-repo> <out-dir> [check ...]
Each check exports HEAD into a scratch copy, applies its change (each edit
must match exactly once), builds the qemu-psci image, then on cortex-a72 and
max runs scripts/qemu/partition-b-check.py (both partitions measure their
own duty cycle over 200 ms) and read-back v6 against grant-budget.json.

QEMU's timing is not the board's (the EL2 timer interrupt arrives 0.5 to 2
ms late under TCG on the Mac, observed 2026-11-02), so duty is judged with
wide bands, fixed here before the first run: throttled means duty at most
900 per mille with at least 5 gaps in 200 ms (B: 150 to 400 per mille, at
least 10 gaps); unthrottled means duty at least 990 per mille with no gap.
Expectations are Claude's.
"""
import json
import os
import re
import subprocess
import sys
import tempfile

GRANT = "docs/methodology/stage2/20261102-budgets/grant-budget.json"
BUDGET = "arch/arm64/budget_hw.c"
PART = "src/core/partition.c"
GUARD = "\tif (cpu >= HV_MAX_CPUS || budget_ns == 0U || budget_ns >= period_ns)\n\t\treturn 0; /* nothing to enforce */"
B_BUDGET = "\t.budget_ns = 2000000ULL, /*  2 ms */"
PRIO = "gic_v3_set_ppi_priority(cpu, HV_CNTHP_PPI, GIC_PRIO_HYPERVISOR)"
DUTY_A = re.compile(r"PARTITION_A: duty (\d+) permille, longest gap (\d+) us, gaps (\d+)")
THROTTLED_A = ("throttled", 0, 900, 5)
THROTTLED_B = ("throttled", 150, 400, 10)
FREE = ("free", 990, 1000, 0)
# name: (edits, A band, B band, read-back findings)
CHECKS = {
    "P1": ([], THROTTLED_A, THROTTLED_B, []),
    # Enforcement off: the EL2 timer is never armed on any CPU.
    "P2": ([(BUDGET, GUARD, "\tif (1)\n\t\treturn 0; /* control: enforcement off */")], FREE, FREE,
           [("CPU 0 PPI 26", "missing interrupt binding"), ("CPU 2 PPI 26", "missing interrupt binding")]),
    # B's budget equals its period: nothing to enforce on CPU 2 only.
    "P3": ([(PART, B_BUDGET, "\t.budget_ns = 10000000ULL, /* control: = period */")], THROTTLED_A, FREE,
           [("CPU 2 PPI 26", "missing interrupt binding")]),
    # EL2 timer at priority 0xf0, equal to Haven's priority mask: the timer
    # interrupt is never taken, so nothing is throttled.
    "P4": ([(BUDGET, PRIO, "gic_v3_set_ppi_priority(cpu, HV_CNTHP_PPI, 0xf0)")], FREE, FREE,
           [("CPU 0 PPI 26", "interrupt priority drift"), ("CPU 2 PPI 26", "interrupt priority drift")]),
}
DEVICE = "virtio-rng-pci,disable-legacy=on,iommu_platform=on"
CPUS = ["cortex-a72", "max"]
TOOLCHAIN = "/opt/homebrew/Cellar/aarch64-unknown-linux-gnu/15.2.0/toolchain/bin"
ENV = dict(os.environ, PATH=f"/opt/homebrew/bin:/usr/local/bin:{TOOLCHAIN}:" + os.environ.get("PATH", ""))


def sh(cmd, cwd, log, timeout=900):
    with open(log, "a") as f:
        f.write(f"$ {cmd}\n")
        f.flush()
        return subprocess.run(cmd, cwd=cwd, shell=True, stdout=f, stderr=subprocess.STDOUT,
                              env=ENV, timeout=timeout).returncode


def mutate(src, edits):
    for path, old, new in edits:
        p = os.path.join(src, path)
        s = open(p).read()
        if s.count(old) != 1:
            raise SystemExit(f"edit does not match exactly once in {path} (found {s.count(old)})")
        open(p, "w").write(s.replace(old, new))


def in_band(duty, band):
    if duty is None:
        return False
    _, lo, hi, min_gaps = band
    permille, _, gaps = duty
    if band[0] == "free":
        return permille >= lo and gaps == 0
    return lo <= permille <= hi and gaps >= min_gaps


def main():
    repo, out = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    only = sys.argv[3:] or list(CHECKS)
    head = subprocess.check_output(["git", "-C", repo, "rev-parse", "--short", "HEAD"], text=True).strip()
    os.makedirs(out, exist_ok=True)
    results = {}
    for name in only:
        edits, band_a, band_b, want_rb = CHECKS[name]
        cdir = os.path.join(out, name)
        os.makedirs(cdir, exist_ok=True)
        log = os.path.join(cdir, "build.log")
        open(log, "w").close()
        src = tempfile.mkdtemp(prefix=f"bud-{name}-")
        subprocess.run(f"git -C {repo} archive HEAD | tar -x -C {src}", shell=True, check=True)
        mutate(src, edits)
        with open(os.path.join(cdir, "change.diff"), "w") as f:
            for path in sorted({e[0] for e in edits}):
                subprocess.run(["diff", "-u", f"{repo}/{path}", f"{src}/{path}"], stdout=f)
        r = {"check": name, "band_a": band_a, "band_b": band_b, "want_rb": want_rb}
        r["build_rc"] = sh("cmake --preset arm64-qemu -DHAVEN_BOOT_PROFILE=qemu-psci && cmake --build build", src, log)
        for cpu in CPUS:
            c = {}
            if r["build_rc"] == 0:
                p = subprocess.run([sys.executable, os.path.join(src, "scripts/qemu/partition-b-check.py"),
                                    os.path.join(src, "build"), cpu, "30"],
                                   capture_output=True, text=True, env=ENV, timeout=120)
                open(os.path.join(cdir, f"partb-{cpu}.txt"), "w").write(p.stdout + p.stderr)
                m = DUTY_A.search(p.stdout)
                c["duty_a"] = [int(m.group(1)), int(m.group(2)), int(m.group(3))] if m else None
                mb = re.search(r"duty_permille=(\d+), max_gap_us=(\d+), gaps=(\d+)", p.stdout)
                c["duty_b"] = [int(mb.group(1)), int(mb.group(2)), int(mb.group(3))] if mb else None
                c["partb_rc"] = p.returncode
                c["budget_stats"] = re.findall(r"Haven budget CPU \d+: [^\n]+", p.stdout)
                rb = os.path.join(cdir, f"readback-{cpu}")
                q = subprocess.run([sys.executable, os.path.join(repo, "tools/readback/readback.py"),
                                    os.path.join(src, "build"), os.path.join(repo, GRANT), rb, "--cpu", cpu,
                                    "--wait", "20", "--device", DEVICE, "--marker", "IRQ test done"],
                                   capture_output=True, text=True, env=ENV, timeout=300)
                open(rb + ".txt", "w").write(q.stdout + q.stderr)
                try:
                    c["rb"] = [x[:2] for x in json.load(open(os.path.join(rb, "readback.json")))["findings"]]
                except (OSError, ValueError, KeyError):
                    c["rb"] = None
                c["ok"] = (c["partb_rc"] == 0 and in_band(c["duty_a"], band_a) and in_band(c["duty_b"], band_b)
                           and c["rb"] is not None and sorted({tuple(x) for x in c["rb"]}) == sorted(set(want_rb)))
            r[cpu] = c
        results[name] = r
        print(name, json.dumps(r), flush=True)
        subprocess.run(["rm", "-rf", src])
    json.dump({"haven_commit": head, "results": results}, open(os.path.join(out, "results.json"), "w"), indent=1)
    fmt = lambda d: "-" if d is None else f"{d[0]} permille, gap {d[1]} us, {d[2]} gaps"
    rows = [f"# Budget checks (haven {head})", "",
            "| Check | CPU | Partition A duty | Partition B duty | Read-back findings | As expected |",
            "| - | - | - | - | - | - |"]
    for name, r in results.items():
        for cpu in CPUS:
            c = r[cpu]
            if not c:
                rows.append(f"| {name} | {cpu} | build failed | - | - | no |")
                continue
            rb = "no report" if c["rb"] is None else ("; ".join(f"{a}: {b}" for a, b in c["rb"]) or "none")
            rows.append(f"| {name} | {cpu} | {fmt(c['duty_a'])} | {fmt(c['duty_b'])} | {rb} | "
                        f"{'yes' if c['ok'] else 'no'} |")
    open(os.path.join(out, "results.md"), "w").write("\n".join(rows) + "\n")
    print("\n".join(rows))


if __name__ == "__main__":
    main()
