#!/usr/bin/env python3
"""Secondary-CPU checks for FINDINGS B3: CPU map, entry checks, ready handshake.

Usage: run_b3.py <haven-repo> <out-dir> [check ...]
Each check exports HEAD into a scratch copy, applies its change (each edit
must match exactly once), builds the qemu-psci image and runs
scripts/qemu/partition-b-check.py on cortex-a72 and max. A check passes when
every expected UART line is present, every forbidden one absent, and
partition B's status record has the expected stage. Expectations are
Claude's, committed before the first run.
"""
import json
import os
import re
import subprocess
import sys
import tempfile

PLAT = "src/platform/qemu-virt/platform.c"
PART = "src/core/partition.c"
TABLE = "const uint64_t hv_cpu_mpidr[] = {0x0, 0x1, 0x2, 0x3};"
COUNT = "const uint32_t hv_cpu_count = 4U;"
GATE = "\t/* FINDINGS B3: enter B only on the primary's go;"
DELAY = ("\t{ /* control: report ready 300 ms late */\n"
         "\t\tuint64_t t0, t, f;\n"
         "\t\t__asm__ volatile(\"mrs %0, cntfrq_el0\" : \"=r\"(f));\n"
         "\t\t__asm__ volatile(\"mrs %0, cntpct_el0\" : \"=r\"(t0));\n"
         "\t\tdo {\n"
         "\t\t\t__asm__ volatile(\"isb\\n\\tmrs %0, cntpct_el0\" : \"=r\"(t));\n"
         "\t\t} while (t - t0 < (f / 10U) * 3U);\n"
         "\t}\n" + GATE)
READY = "CPU 2 ready, partition B started"
LATE = "CPU 2 not ready within 100 ms - partition B not started"
NO_PSCI = "PSCI CPU_ON failed for CPU 2 - partition B inactive"
NO_CPU = "no CPU 2 in the CPU map - partition B not started"
A_DONE = "PARTITION_A: IRQ test done"
# name: (edits, lines that must appear, lines that must not, B stage expected)
CHECKS = {
    "L1": ([], [READY, A_DONE], [LATE, NO_PSCI], 5),
    # CPU 2's MPIDR also given to entry 1: CPU 2 resolves to index 1, waits
    # on mailbox 1 that is never filled, never reports ready for index 2.
    "L2": ([(PLAT, TABLE, TABLE.replace("0x1,", "0x2,"))], [LATE, A_DONE], [READY], 0),
    # CPU 2 reports ready 300 ms after the primary's 100 ms deadline.
    "L3": ([(PART, GATE, DELAY)], [LATE, A_DONE], [READY], 0),
    # The boot CPU is not entry 0 of the map: Haven refuses to start.
    "L4": ([(PLAT, TABLE, "const uint64_t hv_cpu_mpidr[] = {0x1, 0x0, 0x2, 0x3};")],
           ["boot CPU is not entry 0 of the CPU map"], [READY, A_DONE], 0),
    # The map has no CPU 2. First run (runs-first/) expected CPU_ON to
    # fail; instead the ready deadline fired, because the C side ignored
    # the platform's count (GCC folded a weak const default), started
    # MPIDR 2, and boot.S parked it. Haven now refuses before CPU_ON.
    "L5": ([(PLAT, COUNT, "const uint32_t hv_cpu_count = 2U;")], [NO_CPU, A_DONE], [READY, LATE, NO_PSCI], 0),
}
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


def main():
    repo, out = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    only = sys.argv[3:] or list(CHECKS)
    head = subprocess.check_output(["git", "-C", repo, "rev-parse", "--short", "HEAD"], text=True).strip()
    os.makedirs(out, exist_ok=True)
    results = {}
    for name in only:
        edits, want, forbid, stage = CHECKS[name]
        cdir = os.path.join(out, name)
        os.makedirs(cdir, exist_ok=True)
        log = os.path.join(cdir, "build.log")
        open(log, "w").close()
        src = tempfile.mkdtemp(prefix=f"b3-{name}-")
        subprocess.run(f"git -C {repo} archive HEAD | tar -x -C {src}", shell=True, check=True)
        mutate(src, edits)
        with open(os.path.join(cdir, "change.diff"), "w") as f:
            for path in sorted({e[0] for e in edits}):
                subprocess.run(["diff", "-u", f"{repo}/{path}", f"{src}/{path}"], stdout=f)
        r = {"check": name, "want": want, "forbid": forbid, "stage": stage}
        r["build_rc"] = sh("cmake --preset arm64-qemu -DHAVEN_BOOT_PROFILE=qemu-psci && cmake --build build", src, log)
        for cpu in CPUS:
            c = {}
            if r["build_rc"] == 0:
                p = subprocess.run([sys.executable, os.path.join(src, "scripts/qemu/partition-b-check.py"),
                                    os.path.join(src, "build"), cpu, "20"],
                                   capture_output=True, text=True, env=ENV, timeout=120)
                open(os.path.join(cdir, f"partb-{cpu}.txt"), "w").write(p.stdout + p.stderr)
                m = re.search(r"stage=(\d+)", p.stdout)
                c["stage"] = int(m.group(1)) if m else None
                # Match the UART only: the check list printed after it
                # repeats some of these lines as labels (first-run bug).
                uart = p.stdout.split("---------------------")[0]
                c["present"] = [w for w in want if w in uart]
                c["forbidden_seen"] = [w for w in forbid if w in uart]
                c["ok"] = len(c["present"]) == len(want) and not c["forbidden_seen"] and c["stage"] == stage
            r[cpu] = c
        results[name] = r
        print(name, json.dumps(r), flush=True)
        subprocess.run(["rm", "-rf", src])
    json.dump({"haven_commit": head, "results": results}, open(os.path.join(out, "results.json"), "w"), indent=1)
    rows = [f"# Secondary-CPU checks (haven {head})", "",
            "| Check | CPU | Expected lines present | Forbidden lines seen | B stage (expected) | As expected |",
            "| - | - | - | - | - | - |"]
    for name, r in results.items():
        for cpu in CPUS:
            c = r[cpu]
            if not c:
                rows.append(f"| {name} | {cpu} | build failed | - | - | no |")
                continue
            rows.append(f"| {name} | {cpu} | {len(c['present'])} of {len(r['want'])} | "
                        f"{'; '.join(c['forbidden_seen']) or '-'} | {c['stage']} ({r['stage']}) | "
                        f"{'yes' if c['ok'] else 'no'} |")
    open(os.path.join(out, "results.md"), "w").write("\n".join(rows) + "\n")
    print("\n".join(rows))


if __name__ == "__main__":
    main()
