#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Rows C1, C3 and C5 of README.md: boot Linux in partition A many times.

Usage: stress.py run HAVEN_REPO KDAL_SRC IMAGE KDAL_KO ROUNDS OUT
       stress.py sum DIR
run: builds the initramfs once and HAVEN once for N = 2 and N = 3
(../20261112-multi-vcpu-n/run_n.py's steps), boots both ROUNDS times and
checks each boot against scripts/qemu/linux-partition-check.py's markers.
Writes OUT/results.json and, per failing boot, OUT/fail-r<round>-N<n>.log.
sum: adds up DIR/*/results.json (one per shard) and prints the totals,
also to $GITHUB_STEP_SUMMARY when set; exits 1 if any boot failed.
"""
import glob
import importlib.util
import json
import os
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
NS = (2, 3)
SIGNATURE = "lost after KDAL-DEMAND-BEGIN"


def load(name, *path):
    spec = importlib.util.spec_from_file_location(name, os.path.join(HERE, *path))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


run_n = load("run_n", "..", "20261112-multi-vcpu-n", "run_n.py")
check = load("lpcheck", "..", "..", "..", "..", "scripts", "qemu", "linux-partition-check.py")


def kind(text):
    """The record's signature, or "other"."""
    return SIGNATURE if "KDAL-DEMAND-BEGIN" in text and "KDAL-DEMAND-END" not in text else "other"


def run(repo, ksrc, image, ko, rounds, out):
    os.makedirs(out, exist_ok=True)
    work = tempfile.mkdtemp(prefix="stress-")
    cpio = run_n.initramfs(ksrc, ko, work)
    built = {}
    for n in NS:
        os.makedirs(os.path.join(out, f"N{n}"), exist_ok=True)
        built[n] = run_n.prepare(repo, cpio, n, os.path.join(out, f"N{n}"), work)
    uart = os.path.join(work, "uart.log")
    seconds, failures = {n: [] for n in NS}, []
    for r in range(1, rounds + 1):
        for n in NS:
            b, dtb = built[n]
            seconds[n].append(run_n.boot(b, image, dtb, cpio, uart))
            text = open(uart, errors="replace").read()
            bad = check.problems(text, n)
            if bad:
                log = f"fail-r{r}-N{n}.log"
                shutil.copy(uart, os.path.join(out, log))
                failures.append({"round": r, "n": n, "kind": kind(text), "problems": bad, "log": log})
                print(f"round {r}, N = {n}: FAIL ({kind(text)}): {'; '.join(bad)}", flush=True)
        if r % 10 == 0 or r == rounds:
            print(f"round {r} of {rounds}: {len(failures)} failing boots", flush=True)
    with open(os.path.join(out, "results.json"), "w") as fh:
        json.dump({"rounds": rounds, "seconds": {str(n): seconds[n] for n in NS}, "failures": failures}, fh, indent=1)
    shutil.rmtree(work, ignore_errors=True)


def total(directory):
    files = sorted(glob.glob(os.path.join(directory, "*", "results.json")))
    if not files:
        sys.exit(f"no results.json under {directory}")
    seconds, failures = {n: [] for n in NS}, []
    for f in files:
        with open(f) as fh:
            r = json.load(fh)
        shard = os.path.basename(os.path.dirname(f))
        for n in NS:
            seconds[n] += r["seconds"][str(n)]
        failures += [dict(x, shard=shard) for x in r["failures"]]
    boots = sum(len(s) for s in seconds.values())
    lines = [f"# Linux-partition stress: {len(files)} shards, {boots} boots, {len(failures)} failing", "",
             "| N | boots | failing | mean s per boot |", "| - | - | - | - |"]
    for n in NS:
        bad = sum(1 for x in failures if x["n"] == n)
        mean = sum(seconds[n]) / len(seconds[n]) if seconds[n] else 0.0
        lines.append(f"| {n} | {len(seconds[n])} | {bad} | {mean:.2f} |")
    lines.append("")
    for x in failures:
        lines.append(f"- {x['shard']}, round {x['round']}, N = {x['n']} ({x['kind']}): {'; '.join(x['problems'])}")
    text = "\n".join(lines)
    print(text)
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a") as fh:
            fh.write(text + "\n")
    return 1 if failures else 0


def main():
    if len(sys.argv) == 8 and sys.argv[1] == "run":
        repo, ksrc, image, ko = (os.path.abspath(x) for x in sys.argv[2:6])
        run(repo, ksrc, image, ko, int(sys.argv[6]), os.path.abspath(sys.argv[7]))
    elif len(sys.argv) == 3 and sys.argv[1] == "sum":
        sys.exit(total(sys.argv[2]))
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
