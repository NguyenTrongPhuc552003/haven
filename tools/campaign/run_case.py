#!/usr/bin/env python3
"""Run one campaign case: a patch on a clean copy, every method, one JSON file.

docs/methodology/stage2/20261119-evaluation-campaign/README.md, step C1
(row K3: the unchanged tree is clean; row K4: v3's case M5). The successor
of case set v3's run_cases_v3.py, one case per call so CI can shard.

The repository's HEAD is exported (git archive) into a scratch directory,
the patch (if any) is applied there, and the methods run on that copy:

  M1  host unit tests (CI's host-tests preset and ctest)
  M2  QEMU boot check of the qemu-direct build (scripts/qemu/qemu-direct-check.sh)
  M3  structural check of the grant: format 1 to 2 and back (tools/readback/grant.py
      rejects sizes off 4 KiB and DMA windows outside a mapped region); a guest
      device tree check (dt_check.py) applies only to Linux partitions
  M4  the partitions' own checks: partition B's result block in the qemu-psci
      build (scripts/qemu/partition-b-check.py)
  M5  demand check: Linux cases only
  M6  read-back through the QEMU monitor (tools/readback/readback.py)
  M7  read-back through the EL3 dump: not wired yet (it needs the TF-A boot
      profile; a later C1 step), so n/a

M2, M4 and M6 run on every CPU model (cortex-a72 and max); a method has a
finding when any CPU has one. Each method's verdict is pass, finding, n/a
or error (the method could not judge: a probe failure, a timeout).

Nothing is printed but the case id and each method's verdict: build logs
and UART logs go to OUT/CASE/, and the patch is never echoed (held-out
cases stay sealed until the results record).

Usage: run_case.py --case ID --class CLS --source written|random|heldout
                   --grant GRANT [--patch FILE] [--repo DIR] OUT
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "readback"))
import grant as grant_mod  # noqa: E402

CPUS = ["cortex-a72", "max"]
DEVICE = "virtio-rng-pci,disable-legacy=on,iommu_platform=on"
MARKER = "IRQ test done"
SCHEMA = "haven-campaign-case/1"


def sh(cmd, cwd, log, timeout=900):
    """Run a shell command with its output appended to log; return its exit code."""
    with open(log, "a") as f:
        f.write(f"$ {cmd}\n")
        f.flush()
        try:
            return subprocess.run(cmd, cwd=cwd, shell=True, stdout=f, stderr=subprocess.STDOUT,
                                  timeout=timeout).returncode
        except subprocess.TimeoutExpired:
            f.write("TIMEOUT\n")
            return 124


def export(repo):
    src = tempfile.mkdtemp(prefix="campaign-")
    subprocess.run(f"git -C '{repo}' archive HEAD | tar -x -C '{src}'", shell=True, check=True)
    return src


def apply_patch(src, patch, log):
    """git apply, then patch -p1; output only to the log."""
    with open(log, "a") as f:
        for cmd in (["git", "apply", patch], ["patch", "-p1", "-i", patch]):
            if subprocess.run(cmd, cwd=src, stdout=f, stderr=subprocess.STDOUT).returncode == 0:
                return True
    return False


def by_cpu(results):
    """Per-CPU (verdict, evidence) into one method verdict."""
    verdicts = [v for v, _ in results.values()]
    if "finding" in verdicts:
        v = "finding"
    elif "error" in verdicts:
        v = "error"
    else:
        v = "pass"
    return {"verdict": v, "evidence": [f"{cpu}: {e}" for cpu, (_, e) in results.items()]}


def m1(src, cdir):
    log = os.path.join(cdir, "m1.log")
    rc = sh("cmake --preset host-tests && cmake --build build-host", src, log)
    if rc != 0:
        return {"verdict": "finding", "evidence": [f"host-tests build exit {rc}"]}
    rc = sh("ctest --test-dir build-host --output-on-failure --timeout 60 -E bench_temporal_isolation",
            src, log)
    return {"verdict": "pass" if rc == 0 else "finding", "evidence": [f"ctest exit {rc}"]}


def m2(src, cdir, built):
    if not built:
        return {"verdict": "finding", "evidence": ["qemu-direct build failed"]}
    res = {}
    for cpu in CPUS:
        rc = sh(f"./scripts/qemu/qemu-direct-check.sh build {cpu}", src, os.path.join(cdir, f"m2-{cpu}.log"), 120)
        res[cpu] = ("pass", "markers present") if rc == 0 else (
            ("finding", "marker missing") if rc == 1 else ("error", f"exit {rc}"))
    return by_cpu(res)


def m3(grant):
    try:
        grant_mod.to_v1(grant_mod.to_v2(grant_mod.load(grant)))
    except (ValueError, KeyError, TypeError) as e:
        return {"verdict": "finding", "evidence": [f"grant: {e}"]}
    return {"verdict": "pass", "evidence": ["grant: format 1 to 2 and back", "device tree: n/a (no Linux partition)"]}


def m4(src, cdir):
    log = os.path.join(cdir, "m4-build.log")
    if sh("cmake --preset arm64-qemu -B build-psci -DHAVEN_BOOT_PROFILE=qemu-psci && cmake --build build-psci",
          src, log) != 0:
        return {"verdict": "finding", "evidence": ["qemu-psci build failed"]}
    res = {}
    for cpu in CPUS:
        rc = sh(f"python3 scripts/qemu/partition-b-check.py build-psci {cpu}", src,
                os.path.join(cdir, f"m4-{cpu}.log"), 120)
        res[cpu] = ("pass", "partition B result pass") if rc == 0 else ("finding", f"exit {rc}")
    return by_cpu(res)


def m6(src, cdir, grant, built):
    if not built:
        return {"verdict": "error", "evidence": ["qemu-direct build failed: nothing to read"]}
    script = os.path.join(HERE, "..", "readback", "readback.py")
    res, seconds = {}, 0.0
    for cpu in CPUS:
        out = os.path.join(cdir, f"m6-{cpu}")
        cmd = [sys.executable, script, os.path.join(src, "build"), grant, out, "--cpu", cpu,
               "--wait", "20", "--device", DEVICE, "--marker", MARKER]
        start = time.monotonic()
        with open(os.path.join(cdir, f"m6-{cpu}.log"), "w") as f:
            try:
                rc = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT, timeout=300).returncode
            except subprocess.TimeoutExpired:
                rc = 124
        seconds += time.monotonic() - start
        try:
            with open(os.path.join(out, "readback.json")) as f:
                found = ["{}: {}".format(*x[:2]) for x in json.load(f)["findings"]]
        except (OSError, ValueError, KeyError):
            found = []
        if rc == 0:
            res[cpu] = ("pass", "matches the grant")
        elif rc == 1:
            res[cpu] = ("finding", "; ".join(found) or "findings")
        else:
            res[cpu] = ("error", f"probe failure (exit {rc})")
    r = by_cpu(res)
    r["seconds"] = round(seconds, 1)
    return r


def run(args):
    repo = os.path.abspath(args.repo)
    out = os.path.abspath(args.out)
    cdir = os.path.join(out, args.case)
    os.makedirs(cdir, exist_ok=True)
    src = export(repo)
    try:
        if args.patch and not apply_patch(src, os.path.abspath(args.patch), os.path.join(cdir, "patch.log")):
            raise SystemExit(f"{args.case}: the patch does not apply")
        grant = os.path.abspath(args.grant)
        built = sh("cmake --preset arm64-qemu -DHAVEN_BOOT_PROFILE=qemu-direct && cmake --build build",
                   src, os.path.join(cdir, "build.log")) == 0
        methods = {
            "M1": m1(src, cdir),
            "M2": m2(src, cdir, built),
            "M3": m3(grant),
            "M4": m4(src, cdir),
            "M5": {"verdict": "n/a", "evidence": ["no Linux partition in this case"]},
            "M6": m6(src, cdir, grant, built),
            "M7": {"verdict": "n/a", "evidence": ["EL3 read-back not wired yet"]},
        }
    finally:
        shutil.rmtree(src, ignore_errors=True)
    commit = subprocess.run(["git", "-C", repo, "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
    case = {"schema": SCHEMA, "case": args.case, "source": args.source, "class": args.cls,
            "equivalent": False, "haven_commit": commit, "patched": bool(args.patch), "methods": methods}
    with open(os.path.join(out, f"{args.case}.json"), "w") as f:
        json.dump(case, f, indent=1)
        f.write("\n")
    print(args.case, " ".join(f"{m}={r['verdict']}" for m, r in methods.items()))
    return case


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--case", required=True)
    ap.add_argument("--class", dest="cls", required=True)
    ap.add_argument("--source", required=True, choices=["written", "random", "heldout"])
    ap.add_argument("--grant", required=True)
    ap.add_argument("--patch")
    ap.add_argument("--repo", default=os.path.join(HERE, "..", ".."))
    ap.add_argument("out")
    run(ap.parse_args(argv))
    return 0


if __name__ == "__main__":
    sys.exit(main())
