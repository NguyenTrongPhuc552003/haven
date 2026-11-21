#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Write row K7's test archive (two test cases, not the owner's).

docs/methodology/stage2/20261119-evaluation-campaign/c3/README.md. T1 adds
a comment holding MARKER to src/core/partition.c (it builds); T2 adds a
line holding MARKER that does not compile. Writes test-archive.b64,
TEST-SHA256 and WRONG-SHA256 (one hex digit changed) next to this file.

Usage: make_test_archive.py [REPO]
"""
import difflib
import hashlib
import json
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", ".."))
import seal  # noqa: E402

# split, so the whole marker is found nowhere in the repository's plain text
MARKER = "K7MARKER" + "C3SEALED5F1D0E9A"
TARGET = "src/core/partition.c"
CASES = {
    "T1": ("CONTROL", f"/* {MARKER}: a comment, builds */\n",
           {"M1": "pass", "M2": "pass", "M3": "pass", "M4": "pass", "M6": "pass"}),
    "T2": ("S2-EXTENT", f"int {MARKER} = ;\n",
           {"M1": "finding", "M2": "finding", "M3": "pass", "M4": "finding", "M6": "error"}),
}


def patch_for(repo, line):
    with open(os.path.join(repo, TARGET)) as f:
        old = f.readlines()
    new = old[:1] + [line] + old[1:]
    return "".join(difflib.unified_diff(old, new, "a/" + TARGET, "b/" + TARGET))


def main(argv):
    repo = os.path.abspath(argv[1] if len(argv) > 1 else os.path.join(HERE, "..", "..", "..", ".."))
    with tempfile.TemporaryDirectory() as d:
        for cid, (cls, line, expect) in CASES.items():
            with open(os.path.join(d, cid + ".patch"), "w") as f:
                f.write(patch_for(repo, line))
            with open(os.path.join(d, cid + ".json"), "w") as f:
                json.dump({"class": cls, "expect": expect}, f)
        doc, archive = seal.collect(d, prefix="T", source="row K7's test archive (not the owner's cases)")
    text = seal.encode(archive)
    digest = hashlib.sha256(archive).hexdigest()
    wrong = ("0" if digest[0] != "0" else "1") + digest[1:]
    n = len(doc["cases"])
    for name, content in (("test-archive.b64", text), ("TEST-SHA256", f"{digest}  {n}\n"),
                          ("WRONG-SHA256", f"{wrong}  {n}\n")):
        with open(os.path.join(HERE, name), "w") as f:
            f.write(content)
    print(f"test archive: {n} cases, sha256 {digest}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
