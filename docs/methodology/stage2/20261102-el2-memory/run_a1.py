#!/usr/bin/env python3
"""EL2 stage-1 checks (FINDINGS A1, option A).

Usage: run_a1.py <haven-repo> <out-dir> [check ...]
Each check exports HEAD into a scratch copy, applies its change (each edit
must match exactly once), builds the qemu-psci image, then on cortex-a72 and
max runs scripts/qemu/partition-b-check.py and read-back v5 against
grant-partb-v4.json. A check passes when every expected UART line is
present, every forbidden one absent, partition B's stage matches, and the
read-back finding kinds equal the expectation. Expectations are Claude's,
committed before the first run.
"""
import json
import os
import re
import subprocess
import sys
import tempfile

GRANT = "docs/methodology/stage2/20261102-readback-v4/grant-partb-v4.json"
PLAT = "src/platform/qemu-virt/platform.c"
BOOT = "arch/arm64/boot.S"
IMG = "\t{HAVEN_LOAD_PA, HAVEN_SIZE, HV_EL2_NORMAL},\n"
COUNT = "const uint32_t hv_el2_region_count =\n\tsizeof(hv_el2_regions) / sizeof(hv_el2_regions[0]);"
TTBR = "        msr     ttbr0_el2, x6\n        isb\n        tlbi    alle2"
ON = "EL2 stage-1 on (identity map, image Normal WB, MMIO Device)"
OFF = "EL2 stage-1 off (no EL2 map on this platform)"
READY = "CPU 2 ready, partition B started"
LATE = "CPU 2 not ready within 100 ms - partition B not started"
A_DONE = "PARTITION_A: IRQ test done"
# name: (edits, UART lines required, UART lines forbidden, B stage, read-back findings, read-back exit)
CHECKS = {
    "N1": ([], [ON, READY, A_DONE], [LATE, OFF], 5, [], 0),
    # Partition B's first 2 MiB of RAM also mapped at EL2 as Normal memory.
    "N2": ([(PLAT, IMG, IMG + "\t{0xA0800000UL, 0x200000UL, HV_EL2_NORMAL},\n")],
           [ON, READY, A_DONE], [LATE], 5, [("EL2 / partition 2", "partition memory mapped at EL2")], 1),
    # CPU 2 loads a wrong TTBR0_EL2: it faults as soon as translation is on,
    # so the secondary path really runs translated; partition B never starts.
    "N3": ([(BOOT, TTBR, "        add     x6, x6, #0x1000\n" + TTBR)], [ON, LATE, A_DONE], [READY], 0, [], 0),
    # The Normal region does not cover Haven's image: build refuses (rc -4).
    "N4": ([(PLAT, IMG, "\t{HAVEN_LOAD_PA, HAVEN_SIZE / 2U, HV_EL2_NORMAL},\n")],
           ["EL2 map rejected (rc=-4)"], [ON, READY, A_DONE], 0, None, None),
    # No EL2 map: translation stays off and everything else still works.
    "N5": ([(PLAT, COUNT, "const uint32_t hv_el2_region_count = 0U;")], [OFF, READY, A_DONE], [ON, LATE], 5, [], 0),
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


def main():
    repo, out = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    only = sys.argv[3:] or list(CHECKS)
    head = subprocess.check_output(["git", "-C", repo, "rev-parse", "--short", "HEAD"], text=True).strip()
    os.makedirs(out, exist_ok=True)
    results = {}
    for name in only:
        edits, want, forbid, stage, want_rb, want_rc = CHECKS[name]
        cdir = os.path.join(out, name)
        os.makedirs(cdir, exist_ok=True)
        log = os.path.join(cdir, "build.log")
        open(log, "w").close()
        src = tempfile.mkdtemp(prefix=f"a1-{name}-")
        subprocess.run(f"git -C {repo} archive HEAD | tar -x -C {src}", shell=True, check=True)
        mutate(src, edits)
        with open(os.path.join(cdir, "change.diff"), "w") as f:
            for path in sorted({e[0] for e in edits}):
                subprocess.run(["diff", "-u", f"{repo}/{path}", f"{src}/{path}"], stdout=f)
        r = {"check": name, "want": want, "forbid": forbid, "stage": stage, "want_rb": want_rb, "want_rc": want_rc}
        r["build_rc"] = sh("cmake --preset arm64-qemu -DHAVEN_BOOT_PROFILE=qemu-psci && cmake --build build", src, log)
        for cpu in CPUS:
            c = {}
            if r["build_rc"] == 0:
                p = subprocess.run([sys.executable, os.path.join(src, "scripts/qemu/partition-b-check.py"),
                                    os.path.join(src, "build"), cpu, "20"],
                                   capture_output=True, text=True, env=ENV, timeout=120)
                open(os.path.join(cdir, f"partb-{cpu}.txt"), "w").write(p.stdout + p.stderr)
                uart = p.stdout.split("---------------------")[0]
                m = re.search(r"stage=(\d+)", p.stdout)
                c["stage"] = int(m.group(1)) if m else None
                c["present"] = [w for w in want if w in uart]
                c["forbidden_seen"] = [w for w in forbid if w in uart]
                c["rb_rc"], c["rb"] = None, None
                if want_rb is not None:
                    rb = os.path.join(cdir, f"readback-{cpu}")
                    q = subprocess.run([sys.executable, os.path.join(repo, "tools/readback/readback.py"),
                                        os.path.join(src, "build"), os.path.join(repo, GRANT), rb, "--cpu", cpu,
                                        "--wait", "20", "--device", DEVICE, "--marker", "IRQ test done"],
                                       capture_output=True, text=True, env=ENV, timeout=300)
                    open(rb + ".txt", "w").write(q.stdout + q.stderr)
                    c["rb_rc"] = q.returncode
                    try:
                        c["rb"] = [x[:2] for x in json.load(open(os.path.join(rb, "readback.json")))["findings"]]
                    except (OSError, ValueError, KeyError):
                        pass
                rb_ok = want_rb is None or (c["rb"] is not None and c["rb_rc"] == want_rc and
                                            sorted({tuple(x) for x in c["rb"]}) == sorted(set(want_rb)))
                c["ok"] = (len(c["present"]) == len(want) and not c["forbidden_seen"] and
                           c["stage"] == stage and rb_ok)
            r[cpu] = c
        results[name] = r
        print(name, json.dumps(r), flush=True)
        subprocess.run(["rm", "-rf", src])
    json.dump({"haven_commit": head, "results": results}, open(os.path.join(out, "results.json"), "w"), indent=1)
    rows = [f"# EL2 stage-1 checks (haven {head})", "",
            "| Check | CPU | UART lines | Forbidden seen | B stage (exp) | Read-back findings | As expected |",
            "| - | - | - | - | - | - | - |"]
    for name, r in results.items():
        for cpu in CPUS:
            c = r[cpu]
            if not c:
                rows.append(f"| {name} | {cpu} | build failed | - | - | - | no |")
                continue
            rb = "not run" if r["want_rb"] is None else (
                "no report" if c["rb"] is None else ("; ".join(f"{a}: {b}" for a, b in c["rb"]) or "none"))
            rows.append(f"| {name} | {cpu} | {len(c['present'])} of {len(r['want'])} | "
                        f"{'; '.join(c['forbidden_seen']) or '-'} | {c['stage']} ({r['stage']}) | {rb} | "
                        f"{'yes' if c['ok'] else 'no'} |")
    open(os.path.join(out, "results.md"), "w").write("\n".join(rows) + "\n")
    print("\n".join(rows))


if __name__ == "__main__":
    main()
