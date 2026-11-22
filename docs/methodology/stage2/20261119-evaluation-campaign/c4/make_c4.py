#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Write tools/campaign/cases/c4-boot.json (campaign step C4, README.md here):
the written cases with W03, W07, W12 and W23 replaced by their corrected
forms, then the random and the historical cases, unchanged.

Usage: make_c4.py [--check]   (--check: exit 1 if the file differs)
"""
import json
import os
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), *[".."] * 5))
CASES = os.path.join(REPO, "tools", "campaign", "cases")


def load(name):
    with open(os.path.join(CASES, name)) as f:
        return json.load(f)


def build():
    w, c, r, h = (load(n) for n in ("c2-written.json", "c2-corrected.json", "c2-random.json", "historical.json"))
    replaced = {x["case"][:-1] for x in c["cases"]}  # C2-W03r -> C2-W03
    cases = [x for x in w["cases"] if x["case"] not in replaced] + c["cases"] + r["cases"] + h["cases"]
    expect = {k: v for k, v in w["expect"].items() if k not in replaced}
    expect.update(c["expect"])
    grants = {d["grant"] for d in (w, c, r, h)}
    assert len(grants) == 1, grants
    return {"schema": "haven-campaign-cases/1",
            "source": "campaign step C4 (docs/methodology/stage2/20261119-evaluation-campaign/c4/): "
                      "written (corrected where C2 corrected them), random and historical cases",
            "grant": grants.pop(), "cases": cases, "expect": expect}


def main(argv):
    text = json.dumps(build(), indent=1) + "\n"
    path = os.path.join(CASES, "c4-boot.json")
    if "--check" in argv:
        with open(path) as f:
            return 0 if f.read() == text else 1
    with open(path, "w") as f:
        f.write(text)
    print(f"{len(json.loads(text)['cases'])} cases")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
