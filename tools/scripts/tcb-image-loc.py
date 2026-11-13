#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Count the code that goes into haven.elf: the trusted code of a build.

Usage: tcb-image-loc.py BUILD_DIR [--by-file]

Reads the compiler's dependency records from Ninja (`ninja -t deps`) for
every object of the `haven` target (not the demo guests, not host tests),
keeps the repository's .c, .h and .S files among them, and counts them
with cloc (code lines: no blanks, no comments). Unlike tcb-loc.sh, which
counts whole directories, this counts exactly the files the hypervisor
image is built from, including the headers they include. Code under an
#ifdef that is off in this build still counts (cloc reads files, not the
preprocessor's output).

Needs ninja and cloc on PATH; BUILD_DIR must be a built Ninja tree.
"""
import csv
import io
import os
import re
import subprocess
import sys


def image_files(build, root):
    out = subprocess.run(["ninja", "-C", build, "-t", "deps"], capture_output=True, text=True, check=True).stdout
    files, obj = set(), ""
    for line in out.splitlines():
        if not line.startswith(" "):
            obj = line.split(":")[0]
            continue
        if "haven.dir" not in obj:
            continue
        p = line.strip()
        p = os.path.realpath(p if os.path.isabs(p) else os.path.join(build, p))
        if p.startswith(root + os.sep) and re.search(r"\.(c|h|S)$", p):
            files.add(p)
    return sorted(files)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    build = os.path.realpath(sys.argv[1])
    root = os.path.realpath(os.path.join(os.path.dirname(__file__), "..", ".."))
    files = image_files(build, root)
    if not files:
        print("no dependency records: build the tree first")
        return 2
    args = ["cloc", "--quiet", "--csv"] + (["--by-file"] if "--by-file" in sys.argv else []) + files
    rows = list(csv.reader(io.StringIO(subprocess.run(args, capture_output=True, text=True, check=True).stdout)))
    for r in rows:
        if len(r) >= 5 and r[1].startswith("/"):
            print(f"{os.path.relpath(r[1], root):50s} {r[4]:>6s}")
    # cloc's CSV puts "SUM" in the second column ("N,SUM,...") but, with
    # --by-file, in the first ("SUM,,...")
    total = [r for r in rows if len(r) >= 5 and "SUM" in (r[0], r[1])][-1]
    print(f"{len(files)} files, {total[4]} code lines, {total[3]} comment lines, {total[2]} blank lines")
    return 0


if __name__ == "__main__":
    sys.exit(main())
