#!/usr/bin/env python3
"""Blind test of dt_check.py: the drawn faults (draw.json) applied to the T1
tree, checked by dt_check against grant-l4.json, then booted under HAVEN.

Usage: run_dt_blind.py HAVEN_REPO KDAL_SRC IMAGE KDAL_KO OUT
Writes OUT/<Mxx>/{partition-a.dts,dt-check.txt,uart.log} and OUT/results.md.
M00 is the unchanged T1 tree (control).
"""
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location(
    "cases", os.path.join(HERE, "..", "20261106-linux-chain-cases", "run_cases.py"))
rc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rc)
sys.path.insert(0, HERE)
import dt_faults  # noqa: E402

GRANT = os.path.join(HERE, "..", "20261104-linux-partition", "grant-l4.json")
EMULATED = ["--emulated", "0x08000000+0x10000", "--emulated", "0x080a0000+0x20000"]


def outcome(log):
    if "KDAL-DEMAND-END" in log and "partition stopped by PSCI OFF" in log:
        return "completes"
    if "Kernel panic" in log:
        return "kernel panic"
    if "Booting Linux" not in log:
        return "no Linux output"
    return "does not complete"


def main():
    repo, ksrc, image, ko, out = (os.path.abspath(x) for x in sys.argv[1:6])
    os.makedirs(out, exist_ok=True)
    work = tempfile.mkdtemp(prefix="dtblind-")
    init, cpio = os.path.join(work, "init"), os.path.join(work, "initramfs.cpio")
    subprocess.run(["clang", "--target=aarch64-linux-gnu", "-ffreestanding", "-nostdlib", "-static", "-O2",
                    "-fno-stack-protector", "-fuse-ld=lld", "-Wl,-e,_start", "-o", init,
                    os.path.join(ksrc, "scripts/ci/kunit-init.c")], check=True)
    subprocess.run([sys.executable, os.path.join(ksrc, "scripts/ci/mkcpio.py"), cpio, init, ko], check=True)
    b = rc.build(repo, "", os.path.join(work, "b"), os.path.join(out, "build.log"))
    ipa = 0x48000000
    base = rc.dts(rc.CASES["T1"][2], ipa, ipa + os.path.getsize(cpio))
    with open(os.path.join(HERE, "draw.json")) as fh:
        faults = json.load(fh)["faults"]
    rows = ["| Fault | Operator | Parameter | Label | dt_check | Score | Boot | Denied accesses |",
            "| - | - | - | - | - | - | - | - |"]
    score = {"TP": 0, "FP": 0, "TN": 0, "FN": 0}
    for f in [{"name": "M00", "op": None, "operator": "none (T1)", "parameter": "-", "label": False}] + faults:
        d = os.path.join(out, f["name"])
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
        text = base if f["op"] is None else dt_faults.apply(base, f["op"], f["index"])
        dts = os.path.join(d, "partition-a.dts")
        with open(dts, "w") as fh:
            fh.write(text)
        r = subprocess.run([sys.executable, os.path.join(repo, "tools/readback/dt_check.py"), GRANT, dts,
                            "--partition", "1"] + EMULATED, capture_output=True, text=True)
        with open(os.path.join(d, "dt-check.txt"), "w") as fh:
            fh.write(r.stdout + r.stderr + f"exit {r.returncode}\n")
        flagged = r.returncode == 1
        key = ("T" if flagged == f["label"] else "F") + ("P" if flagged else "N")
        if f["op"] is not None:
            score[key] += 1
        dtb = os.path.join(work, f["name"] + ".dtb")
        subprocess.run(["dtc", "-q", "-I", "dts", "-O", "dtb", "-o", dtb, dts], check=True)
        uart = os.path.join(d, "uart.log")
        rc.boot(b, image, dtb, cpio, ipa + rc.PA_OFFSET, uart)
        with open(uart, errors="replace") as fh:
            log = fh.read()
        n = re.findall(r"Denied stage-2 .* n=(\d+)", log)
        finds = "; ".join(l.strip().replace("FINDING ", "") for l in r.stdout.splitlines() if "FINDING" in l)
        rows.append(f"| {f['name']} | {f['operator']} | {f['parameter']} | {f['label']} | "
                    f"{finds or 'matches' if r.returncode in (0, 1) else 'error: ' + r.stdout[-80:]} | "
                    f"{key if f['op'] else '-'} | {outcome(log)} | {n[-1] if n else 0} |")
        print(rows[-1], flush=True)
    rows += ["", f"Score over the drawn faults: {score}"]
    with open(os.path.join(out, "results.md"), "w") as fh:
        fh.write("# dt_check blind test runs\n\n" + "\n".join(rows) + "\n")
    shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
