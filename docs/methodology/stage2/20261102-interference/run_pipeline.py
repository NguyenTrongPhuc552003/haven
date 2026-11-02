#!/usr/bin/env python3
"""Interference pipeline in QEMU: does every piece run and report?

Usage: run_pipeline.py <haven-repo> <out-dir> [config ...]
Builds HEAD with a partition A load (none, CPU loop, memory stream) and,
for one configuration, partition B's budget equal to its period, runs
scripts/qemu/partition-b-check.py on cortex-a72 and max, and collects
partition B's latency probe. The check is that the pipeline works: both
guests run with caches on, the probe takes all 500 samples, the load
starts. The latency values are QEMU's on a shared host and are reported,
never interpreted (docs/methodology/stage2/20261102-interference/PLAN.md).
"""
import json
import os
import re
import subprocess
import sys
import tempfile

PART = "src/core/partition.c"
B_BUDGET = "\t.budget_ns = 2000000ULL, /*  2 ms */"
CONFIGS = {
    "none": ("0", []),
    "cpu": ("1", []),
    "mem": ("2", []),
    "none-B-unthrottled": ("0", [(PART, B_BUDGET, "\t.budget_ns = 10000000ULL, /* = period */")]),
}
LOAD_LINE = {"0": None, "1": "PARTITION_A: load cpu running", "2": "PARTITION_A: load memory stream running"}
CPUS = ["cortex-a72", "max"]
TOOLCHAIN = "/opt/homebrew/Cellar/aarch64-unknown-linux-gnu/15.2.0/toolchain/bin"
ENV = dict(os.environ, PATH=f"/opt/homebrew/bin:/usr/local/bin:{TOOLCHAIN}:" + os.environ.get("PATH", ""))


def main():
    repo, out = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    only = sys.argv[3:] or list(CONFIGS)
    head = subprocess.check_output(["git", "-C", repo, "rev-parse", "--short", "HEAD"], text=True).strip()
    os.makedirs(out, exist_ok=True)
    results = {}
    for name in only:
        load, edits = CONFIGS[name]
        cdir = os.path.join(out, name)
        os.makedirs(cdir, exist_ok=True)
        src = tempfile.mkdtemp(prefix=f"ifp-{name}-")
        subprocess.run(f"git -C {repo} archive HEAD | tar -x -C {src}", shell=True, check=True)
        for path, old, new in edits:
            p = os.path.join(src, path)
            s = open(p).read()
            assert s.count(old) == 1, f"edit does not apply in {path}"
            open(p, "w").write(s.replace(old, new))
        with open(os.path.join(cdir, "build.log"), "w") as log:
            rc = subprocess.run("cmake --preset arm64-qemu -DHAVEN_BOOT_PROFILE=qemu-psci "
                                f"-DHAVEN_DEMO_A_LOAD={load} && cmake --build build", cwd=src, shell=True,
                                stdout=log, stderr=subprocess.STDOUT, env=ENV).returncode
        r = {"config": name, "load": load, "build_rc": rc}
        for cpu in CPUS:
            c = {}
            if rc == 0:
                p = subprocess.run([sys.executable, os.path.join(src, "scripts/qemu/partition-b-check.py"),
                                    os.path.join(src, "build"), cpu, "40"],
                                   capture_output=True, text=True, env=ENV, timeout=120)
                open(os.path.join(cdir, f"partb-{cpu}.txt"), "w").write(p.stdout + p.stderr)
                st = dict(re.findall(r"(\w+)=(\d+)", next((l for l in p.stdout.splitlines()
                                                           if l.startswith("partition B status")), "")))
                c = {k: int(v) for k, v in st.items()}
                c["partb_rc"] = p.returncode
                c["caches_a"] = "PARTITION_A: caches on" in p.stdout
                c["load_seen"] = LOAD_LINE[load] is None or LOAD_LINE[load] in p.stdout
                c["ok"] = (p.returncode == 0 and c["caches_a"] and c["load_seen"] and
                           c.get("lat_samples") == 500 and (c.get("sctlr_el1", 0) & 5) == 5)
            r[cpu] = c
        results[name] = r
        print(name, json.dumps(r), flush=True)
        subprocess.run(["rm", "-rf", src])
    json.dump({"haven_commit": head, "results": results}, open(os.path.join(out, "results.json"), "w"), indent=1)
    rows = [f"# Interference pipeline in QEMU (haven {head}); latency values are host-bound, not results", "",
            "| Config | CPU | Pipeline ok | B samples | B latency avg / max (us) | < 1 ms | 1 to 10 ms | >= 10 ms |",
            "| - | - | - | - | - | - | - | - |"]
    for name, r in results.items():
        for cpu in CPUS:
            c = r[cpu]
            if not c:
                rows.append(f"| {name} | {cpu} | build failed | - | - | - | - | - |")
                continue
            lt1 = c.get("lat_lt10us", 0) + c.get("lat_lt100us", 0) + c.get("lat_lt1ms", 0)
            rows.append(f"| {name} | {cpu} | {'yes' if c['ok'] else 'no'} | {c.get('lat_samples')} | "
                        f"{c.get('lat_avg_ns', 0) // 1000} / {c.get('lat_max_ns', 0) // 1000} | {lt1} | "
                        f"{c.get('lat_lt10ms', 0)} | {c.get('lat_ge10ms', 0)} |")
    open(os.path.join(out, "results.md"), "w").write("\n".join(rows) + "\n")
    print("\n".join(rows))


if __name__ == "__main__":
    main()
