#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Run one shard of a cases file through run_case.py (campaign.yml's case jobs).

Usage: run_shard.py CASES SHARD SHARDS OUT [run_case.py options...]
Case i belongs to shard i % SHARDS. Options after OUT (for example
--scenario linux2 --image ... --kdal ... --ko ...) go to every run_case.py call.
"""
import json
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))


def main(argv):
    if len(argv) < 4:
        sys.exit(__doc__)
    cases, shard, shards, out, extra = argv[0], int(argv[1]), int(argv[2]), argv[3], argv[4:]
    with open(cases) as f:
        d = json.load(f)
    start = time.monotonic()
    for i, c in enumerate(d["cases"]):
        if i % shards != shard:
            continue
        cmd = [sys.executable, os.path.join(HERE, "run_case.py"), "--case", c["case"], "--class", c["class"],
               "--source", c["source"], "--grant", c.get("grant", d["grant"])] + extra + [out]
        if c.get("patch"):
            cmd[-1:-1] = ["--patch", c["patch"]]
        subprocess.run(cmd, check=True)
    print(f"shard {shard}: {time.monotonic() - start:.0f} s")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
