#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Read and check ecosystem.lock.json, the single copy of the pins that tie
HAVEN's CI to ELMOS and KDAL (docs/methodology/stage2/20261113-e5-compat-manifest).

Usage (from the repository root):
  ecosystem.py outputs                 pins as key=value lines (for $GITHUB_OUTPUT)
  ecosystem.py check-workflow FILE...  no pinned literal in any FILE (workflows
                                       and actions); every job that uses the
                                       linux-partition action runs on the
                                       pinned runner
  ecosystem.py check-sdk DIR           the pulled SDK's manifest.json names the
                                       pinned ELMOS commit and run
  ecosystem.py check-qemu LINE         `qemu-system-aarch64 --version` (first
                                       line) is the pinned upstream version
Exit 0 if the check holds, 1 (with the reason) otherwise. Standard library only.
"""
import json
import os
import re
import sys

LOCK = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "ecosystem.lock.json")


def lock():
    with open(LOCK) as f:
        d = json.load(f)
    if d.get("schema") != "haven-ecosystem-lock/1":
        sys.exit(f"ecosystem.lock.json: unknown schema {d.get('schema')!r}")
    return d


def fail(msg):
    print(f"FAIL: {msg}")
    sys.exit(1)


def outputs(d):
    s, k = d["elmos_sdk"], d["kdal"]
    pins = {"sdk_ref": f"{s['package']}@{s['digest']}", "sdk_tag": f"{s['package']}:{s['tag']}",
            "sdk_digest": s["digest"], "kdal_repository": k["repository"], "kdal_commit": k["commit"],
            "runner": d["ci"]["runner"], "qemu": d["ci"]["qemu"]}
    for key, value in pins.items():
        print(f"{key}={value}")


# the setup action of every job that boots Linux in partition A
# (docs/methodology/stage2/20261114-console-drain)
ACTION = "./.github/actions/linux-partition"


def jobs(text):
    """(name, body) of each job in a workflow: the keys two spaces in under `jobs:`."""
    m = re.search(r"^jobs:\n(.*)", text, re.M | re.S)
    return re.findall(r"^  ([\w-]+):\n(.*?)(?=^  \S|^\S|\Z)", m.group(1), re.M | re.S) if m else []


def check_workflow(d, paths):
    runner, s, users = d["ci"]["runner"], d["elmos_sdk"], 0
    # hashes also in their short (7-digit) form; the tag as written
    literals = (("KDAL commit", d["kdal"]["commit"], True), ("SDK tag", s["tag"], False),
                ("SDK digest", s["digest"].split(":", 1)[1], True), ("ELMOS commit", s["elmos_commit"], True))
    for path in paths:
        with open(path) as f:
            text = f.read()
        for name, literal, short in literals:
            if literal in text or (short and literal[:7] in text):
                fail(f"{path} names the {name} ({literal[:12]}...) itself; read it from ecosystem.lock.json")
        for job, body in jobs(text):
            if ACTION not in body:
                continue
            users += 1
            runs = re.search(r"^    runs-on:\s*(\S+)\s*$", body, re.M)
            if not runs or runs.group(1) != runner:
                fail(f"{path}: job {job} uses {ACTION} on {runs.group(1) if runs else None!r}, pinned {runner!r}")
    if not users:
        fail(f"no job in {len(paths)} files uses {ACTION}")
    print(f"OK: {len(paths)} files take their pins from ecosystem.lock.json; "
          f"{users} jobs use {ACTION} on {runner}")


def check_sdk(d, directory):
    m = json.load(open(os.path.join(directory, "manifest.json")))
    s = d["elmos_sdk"]
    if m.get("elmos_commit") != s["elmos_commit"] or str(m.get("run")) != str(s["run"]):
        fail(f"SDK manifest: elmos {m.get('elmos_commit')} run {m.get('run')}, "
             f"pinned elmos {s['elmos_commit']} run {s['run']}")
    print(f"OK: SDK built from ELMOS {s['elmos_commit'][:7]}, run {s['run']}")


def check_qemu(d, line):
    m = re.search(r"QEMU emulator version (\d+\.\d+\.\d+)", line)
    if not m or m.group(1) != d["ci"]["qemu"]:
        fail(f"QEMU {m.group(1) if m else line!r}, pinned {d['ci']['qemu']}")
    print(f"OK: QEMU {m.group(1)} ({line.strip()})")


def main():
    d = lock()
    cmd, args = (sys.argv[1], sys.argv[2:]) if len(sys.argv) > 1 else ("", [])
    if cmd == "outputs":
        outputs(d)
    elif cmd == "check-workflow" and args:
        check_workflow(d, args)
    elif cmd == "check-sdk" and len(args) == 1:
        check_sdk(d, args[0])
    elif cmd == "check-qemu" and len(args) == 1:
        check_qemu(d, args[0])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
