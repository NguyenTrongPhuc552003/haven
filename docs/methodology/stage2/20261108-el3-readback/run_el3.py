#!/usr/bin/env python3
"""EL3 read-back against monitor read-back, case by case (README.md).

Usage: run_el3.py HAVEN_REPO TFA_RELEASE_DIR OUT

TFA_RELEASE_DIR: TF-A v2.12.0 built with tools/el3-readback/ (EL3_READBACK=1).
Per case: HAVEN's demo build with the qemu-tfa profile from HEAD plus the
case's seeded fault, then read-back twice under TF-A: through the QEMU
monitor (marker "caches on") and from BL31's dump (--el3). Writes
OUT/<case>/{monitor,el3}/ and OUT/results.md.
"""
import importlib.util
import json
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
GRANT = os.path.join(HERE, "grant-demo-cpus.json")
DEV = "virtio-rng-pci,disable-legacy=on,iommu_platform=on"


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


V3 = load("v3", os.path.join(HERE, "..", "20261102-a1-cases-v3", "run_cases_v3.py")).CASES
GIC = load("gic", os.path.join(HERE, "..", "20261102-gic-readback", "run_gic.py")).CASES
S1 = os.path.join(HERE, "..", "20261107-cpu-assignment", "S1.patch")
CASES = [("E0", [], None), ("M3", V3["M3"], None), ("M4", V3["M4"], None), ("M5", V3["M5"], None),
         ("S8", V3["S8"], None), ("I1", GIC["I1"], None), ("I4", GIC["I4"], None), ("I5", GIC["I5"], None),
         ("S1", [], S1)]


def build(repo, subs, patch, out, log):
    src = tempfile.mkdtemp(prefix="el3-src-")
    subprocess.run(f"git -C {repo} archive HEAD | tar -x -C {src}", shell=True, check=True)
    for f, old, new in subs:
        p = os.path.join(src, f)
        s = open(p).read()
        assert s.count(old) == 1, (f, old[:40])
        open(p, "w").write(s.replace(old, new))
    if patch:
        subprocess.run(["git", "apply", patch], cwd=src, check=True)
    with open(log, "w") as fh:
        subprocess.run(f"cmake --preset arm64-qemu -B {out} -DHAVEN_BOOT_PROFILE=qemu-tfa && cmake --build {out}",
                       shell=True, cwd=src, stdout=fh, stderr=fh, check=True)
    shutil.rmtree(src)
    return out


def readback(repo, b, tfa, out, el3):
    cmd = [sys.executable, os.path.join(repo, "tools/readback/readback.py"), b, GRANT, out, "--tfa", tfa,
           "--device", DEV, "--wait", "40"] + (["--el3"] if el3 else ["--marker", "caches on"])
    r = subprocess.run(cmd, capture_output=True, text=True)
    with open(os.path.join(out, "readback.txt"), "w") as fh:
        fh.write(r.stdout + r.stderr)
    rj = os.path.join(out, "readback.json")
    found = sorted({(f[0], f[1]) for f in json.load(open(rj))["findings"]}) if os.path.exists(rj) else None
    at = next((l for l in r.stdout.splitlines() if l.startswith("read at")), "no read").replace("read at: ", "")
    for f in ("haven-image.bin", "stream-table.bin", "machine.dtb"):
        if os.path.exists(os.path.join(out, f)):
            os.remove(os.path.join(out, f))
    return found, at


def fmt(found):
    if found is None:
        return "probe failure"
    return "; ".join(f"{n}: {c}" for n, c in found) or "matches"


def main():
    repo, tfa, out = (os.path.abspath(x) for x in sys.argv[1:4])
    os.makedirs(out, exist_ok=True)
    work = tempfile.mkdtemp(prefix="el3-")
    rows = ["| Case | Monitor read-back | EL3 read-back | Same findings |", "| - | - | - | - |"]
    for name, subs, patch in CASES:
        d = os.path.join(out, name)
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(os.path.join(d, "monitor"))
        os.makedirs(os.path.join(d, "el3"))
        b = build(repo, subs, patch, os.path.join(work, name), os.path.join(d, "build.log"))
        mon, mon_at = readback(repo, b, tfa, os.path.join(d, "monitor"), False)
        el3, el3_at = readback(repo, b, tfa, os.path.join(d, "el3"), True)
        rows.append(f"| {name} | {fmt(mon)} ({mon_at[:30]}) | {fmt(el3)} | {'yes' if mon == el3 else 'NO'} |")
        print(rows[-1], flush=True)
    with open(os.path.join(out, "results.md"), "w") as fh:
        fh.write("# EL3 against monitor read-back\n\n" + "\n".join(rows) + "\n")
    shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
