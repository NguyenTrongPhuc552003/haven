#!/usr/bin/env python3
"""Reruns cases E0, I1 and S1 of ../20261108-el3-readback/ with read-back v12a.

Usage: rerun.py HAVEN_REPO TFA_RELEASE_DIR OUT
"""
import importlib.util
import os
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("el3", os.path.join(HERE, "..", "20261108-el3-readback", "run_el3.py"))
el3 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(el3)


def main():
    repo, tfa, out = (os.path.abspath(x) for x in sys.argv[1:4])
    os.makedirs(out, exist_ok=True)
    work = tempfile.mkdtemp(prefix="v12a-")
    rows = ["| Case | Monitor read-back | EL3 read-back | Same findings |", "| - | - | - | - |"]
    for name, subs, patch in el3.CASES:
        if name not in ("E0", "I1", "S1"):
            continue
        d = os.path.join(out, name)
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(os.path.join(d, "monitor"))
        os.makedirs(os.path.join(d, "el3"))
        b = el3.build(repo, subs, patch, os.path.join(work, name), os.path.join(d, "build.log"))
        mon, mon_at = el3.readback(repo, b, tfa, os.path.join(d, "monitor"), False)
        e3, _ = el3.readback(repo, b, tfa, os.path.join(d, "el3"), True)
        rows.append(f"| {name} | {el3.fmt(mon)} ({mon_at[:30]}) | {el3.fmt(e3)} | {'yes' if mon == e3 else 'NO'} |")
        print(rows[-1], flush=True)
    with open(os.path.join(out, "results.md"), "w") as fh:
        fh.write("# Read-back v12a reruns\n\n" + "\n".join(rows) + "\n")
    shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
