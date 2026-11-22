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
  M6  read-back through the QEMU monitor (tools/readback/readback.py) of the
      qemu-psci build, the profile the grant describes (amendment 3)
  M7  read-back through the EL3 dump (m7/README.md): with --tfa DIR (TF-A
      built with tools/el3-readback/), the qemu-tfa build boots under TF-A
      and readback.py --el3 reads BL31's dump; without --tfa, n/a

Scenario 2 (--scenario linux2, s2/README.md; linux2.py) replaces M2, M4, M5
and M6 with the Linux-in-A runs; M1 and M3 are shared.

Scenario 3 (--scenario grant2, s3/README.md) is scenario 1 on builds with
HAVEN_CAMPAIGN_GRANT2=ON: M6 and M7 boot with partition A's second DMA
device (GRANT2_DEVICE, at 00:1f.7) as well; the case's grant should be
s3/grant-2.json.

M2, M4 and M6 run on every CPU model (cortex-a72 and max); a method has a
finding when any CPU has one. Each method's verdict is pass, finding, n/a
or error (the method could not judge: a probe failure, a timeout).

The job log gets the case id, each method's verdict and, for written and
random cases, the evidence of each finding or error; build logs and UART
logs go to OUT/CASE/, and the patch is never echoed (held-out cases print
verdicts only and stay sealed until the results record; their evidence
quotes no log line, row K7e of c3/README.md).

Usage: run_case.py --case ID --class CLS --source written|random|heldout|historical
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
GRANT2_DEVICE = DEVICE + ",addr=1f.7"  # stream 255 (s3/README.md)
GRANT2_FLAG = " -DHAVEN_CAMPAIGN_GRANT2=ON"
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
        return {"verdict": "finding", "evidence": [f"host-tests build exit {rc}"] + tail(log)}
    rc = sh("ctest --test-dir build-host --output-on-failure --timeout 60 -E bench_temporal_isolation",
            src, log)
    if rc == 0:
        return {"verdict": "pass", "evidence": ["ctest exit 0"]}
    with open(log) as f:
        failed = failed_tests(f.read())
    return {"verdict": "finding", "evidence": [f"ctest exit {rc}"] + [f"failed: {n}" for n in failed]}


def failed_tests(ctest_output):
    """Test names under ctest's "The following tests FAILED:" list."""
    names, inside = [], False
    for line in ctest_output.splitlines():
        if line.startswith("The following tests FAILED"):
            inside = True
        elif inside and " - " in line:
            names.append(line.split(" - ", 1)[1].split(" (", 1)[0].strip())
        elif inside:
            inside = False
    return names


# held-out cases quote no log lines: a failed build's log can quote the
# patch (c3/README.md, row K7e); run() sets this from the case's source
QUOTE_LOGS = True


def set_quoting(source):
    global QUOTE_LOGS
    QUOTE_LOGS = source != "heldout"


def tail(path, n=5):
    if not QUOTE_LOGS:
        return []
    try:
        with open(path) as f:
            return [x.rstrip() for x in f.readlines()[-n:] if x.strip()]
    except OSError:
        return []


def m6_verdict(rc, report, log):
    """readback.py's result for one CPU: a finding needs the findings it wrote.

    Exit 1 without a readback.json that lists findings is a Python error
    (C1 first run: dtc missing), so the method could not judge.
    """
    found = ["{}: {}".format(*x[:2]) for x in (report or {}).get("findings", [])]
    if rc == 0:
        return "pass", "matches the grant"
    if rc == 1 and found:
        return "finding", "; ".join(found)
    if rc == 2:
        return "error", "probe failure: " + ((report or {}).get("probe_failure") or "exit 2")
    return "error", f"exit {rc} without findings: " + " | ".join(tail(log))


def m2(src, cdir, built):
    if not built:
        return {"verdict": "finding", "evidence": ["qemu-direct build failed"] + tail(os.path.join(cdir, "build.log"))}
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


def m4(src, cdir, built):
    if not built:
        return {"verdict": "finding", "evidence": ["qemu-psci build failed"] + tail(os.path.join(cdir, "build-psci.log"))}
    res = {}
    for cpu in CPUS:
        rc = sh(f"python3 scripts/qemu/partition-b-check.py build-psci {cpu}", src,
                os.path.join(cdir, f"m4-{cpu}.log"), 120)
        res[cpu] = ("pass", "partition B result pass") if rc == 0 else (
            ("finding", "partition B check exit 1") if rc == 1 else ("error", f"exit {rc}"))
    return by_cpu(res)


def device_args(devices):
    return [a for d in devices for a in ("--device", d)]


def m6(src, cdir, grant, built, devices=(DEVICE,)):
    """Read-back of the qemu-psci build (amendment 3: the profile the grant describes)."""
    if not built:
        return {"verdict": "error", "evidence": ["qemu-psci build failed: nothing to read"]}
    script = os.path.join(HERE, "..", "readback", "readback.py")
    res, seconds = {}, 0.0
    for cpu in CPUS:
        out = os.path.join(cdir, f"m6-{cpu}")
        cmd = [sys.executable, script, os.path.join(src, "build-psci"), grant, out, "--cpu", cpu,
               "--wait", "20"] + device_args(devices) + ["--marker", MARKER]
        start = time.monotonic()
        with open(os.path.join(cdir, f"m6-{cpu}.log"), "w") as f:
            try:
                rc = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT, timeout=300).returncode
            except subprocess.TimeoutExpired:
                rc = 124
        seconds += time.monotonic() - start
        try:
            with open(os.path.join(out, "readback.json")) as f:
                report = json.load(f)
        except (OSError, ValueError):
            report = None
        res[cpu] = m6_verdict(rc, report, os.path.join(cdir, f"m6-{cpu}.log"))
    r = by_cpu(res)
    r["seconds"] = round(seconds, 1)
    return r


def evidence_lines(case):
    """Evidence of each finding or error, for the job log.

    Evidence names regions, tests and log lines, never patch text; held-out
    cases still print nothing but verdicts (they stay sealed until the
    results record).
    """
    if case["source"] == "heldout":
        return []
    return [f"{m} {r['verdict']}: {e}" for m, r in case["methods"].items()
            if r["verdict"] in ("finding", "error") for e in r.get("evidence", [])]


def run(args):
    set_quoting(args.source)
    repo = os.path.abspath(args.repo)
    out = os.path.abspath(args.out)
    cdir = os.path.join(out, args.case)
    os.makedirs(cdir, exist_ok=True)
    src = export(repo)
    try:
        if args.patch and not apply_patch(src, os.path.abspath(args.patch), os.path.join(cdir, "patch.log")):
            raise SystemExit(f"{args.case}: the patch does not apply")
        grant = os.path.abspath(args.grant)
        if args.scenario == "linux2":
            methods = linux2_methods(args, src, cdir, grant)
        else:
            methods = boot_methods(src, cdir, grant, args.tfa, args.scenario == "grant2")
    finally:
        shutil.rmtree(src, ignore_errors=True)
    commit = subprocess.run(["git", "-C", repo, "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
    case = {"schema": SCHEMA, "case": args.case, "source": args.source, "class": args.cls,
            "scenario": args.scenario, "equivalent": False, "haven_commit": commit, "patched": bool(args.patch),
            "methods": methods}
    with open(os.path.join(out, f"{args.case}.json"), "w") as f:
        json.dump(case, f, indent=1)
        f.write("\n")
    print(args.case, " ".join(f"{m}={r['verdict']}" for m, r in methods.items()))
    for line in evidence_lines(case):
        print("  " + line)
    return case


def boot_methods(src, cdir, grant, tfa=None, grant2=False):
    """Scenario 1: one boot of the bare test guests, both CPU models (scenario 3 with grant2)."""
    flag = GRANT2_FLAG if grant2 else ""
    devices = (DEVICE, GRANT2_DEVICE) if grant2 else (DEVICE,)
    built = sh(f"cmake --preset arm64-qemu -DHAVEN_BOOT_PROFILE=qemu-direct{flag} && cmake --build build",
               src, os.path.join(cdir, "build.log")) == 0
    psci = sh(f"cmake --preset arm64-qemu -B build-psci -DHAVEN_BOOT_PROFILE=qemu-psci{flag} && cmake --build build-psci",
              src, os.path.join(cdir, "build-psci.log")) == 0
    return {
        "M1": m1(src, cdir),
        "M2": m2(src, cdir, built),
        "M3": m3(grant),
        "M4": m4(src, cdir, psci),
        "M5": {"verdict": "n/a", "evidence": ["no Linux partition in this case"]},
        "M6": m6(src, cdir, grant, psci, devices),
        "M7": m7(src, cdir, grant, tfa, flag, devices),
    }


# The unchanged tree under TF-A already reports this (the prototype's E0,
# docs/methodology/stage2/20261108-el3-readback/; m7/README.md, results):
# M7 counts only findings other than it.
# Retired with the fix of 20261122-firmware-redistributor (HAVEN sets up a
# started CPU's redistributor again), which removes that finding; kept as
# a parameter so a later baseline is a one-line change (row F5).
E0_FINDING = "CPU 2 PPI 27: missing interrupt binding"
M7_BASELINE = ()


def m7_against_baseline(verdict, evidence, baseline=None):
    """M7's verdict and evidence with the TF-A baseline findings taken out."""
    baseline = M7_BASELINE if baseline is None else baseline
    if verdict != "finding":
        return verdict, evidence
    rest = [f.strip() for f in evidence.split(";") if f.strip() and f.strip() not in baseline]
    return ("finding", "; ".join(rest)) if rest else ("pass", "the TF-A baseline finding only (E0)")


def m7(src, cdir, grant, tfa, flag="", devices=(DEVICE,)):
    """EL3 read-back of the qemu-tfa build under TF-A (m7/README.md)."""
    if not tfa:
        return {"verdict": "n/a", "evidence": ["EL3 read-back not wired yet"]}
    if sh(f"cmake --preset arm64-qemu -B build-tfa -DHAVEN_BOOT_PROFILE=qemu-tfa{flag} && cmake --build build-tfa",
          src, os.path.join(cdir, "build-tfa.log")) != 0:
        return {"verdict": "error", "evidence": ["qemu-tfa build failed: nothing to read"]}
    out, log = os.path.join(cdir, "m7"), os.path.join(cdir, "m7.log")
    cmd = [sys.executable, os.path.join(HERE, "..", "readback", "readback.py"), os.path.join(src, "build-tfa"),
           grant, out, "--tfa", os.path.abspath(tfa), "--el3", "--wait", "40"] + device_args(devices)
    start = time.monotonic()
    with open(log, "w") as f:
        try:
            rc = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT, timeout=300).returncode
        except subprocess.TimeoutExpired:
            rc = 124
    try:
        with open(os.path.join(out, "readback.json")) as f:
            report = json.load(f)
    except (OSError, ValueError):
        report = None
    v, e = m7_against_baseline(*m6_verdict(rc, report, log))
    return {"verdict": v, "evidence": [e], "seconds": round(time.monotonic() - start, 1)}


def linux2_methods(args, src, cdir, grant):
    """Scenario 2 (s2/README.md): Linux in partition A with two vCPUs."""
    import linux2
    methods = {"M1": m1(src, cdir), "M3": m3(grant)}
    methods.update(linux2.methods(src, cdir, os.path.abspath(args.image), os.path.abspath(args.kdal),
                                  os.path.abspath(args.ko), m6_verdict))
    methods["M7"] = {"verdict": "n/a", "evidence": ["EL3 read-back not wired yet"]}
    return {m: methods[m] for m in ("M1", "M2", "M3", "M4", "M5", "M6", "M7")}


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--case", required=True)
    ap.add_argument("--class", dest="cls", required=True)
    ap.add_argument("--source", required=True, choices=["written", "random", "heldout", "historical"])
    ap.add_argument("--grant", required=True)
    ap.add_argument("--patch")
    ap.add_argument("--repo", default=os.path.join(HERE, "..", ".."))
    ap.add_argument("--scenario", choices=["boot", "linux2", "grant2"], default="boot",
                    help="boot: scenario 1; linux2: Linux in A with two vCPUs (needs --image, --kdal, --ko); "
                         "grant2: scenario 3, scenario 1 with grant 2 (s3/README.md)")
    ap.add_argument("--tfa", help="boot: TF-A release directory with the EL3 read-back service, for M7")
    ap.add_argument("--image", help="linux2: the SDK kernel Image")
    ap.add_argument("--kdal", help="linux2: KDAL's source tree (scripts/ci/mkcpio.py)")
    ap.add_argument("--ko", help="linux2: kdal.ko built against the SDK kernel")
    ap.add_argument("out")
    args = ap.parse_args(argv)
    if args.scenario == "linux2" and not (args.image and args.kdal and args.ko):
        ap.error("--scenario linux2 needs --image, --kdal and --ko")
    run(args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
