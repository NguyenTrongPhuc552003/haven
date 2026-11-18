#!/usr/bin/env python3
"""run_case.py's parts that need no build or QEMU (K3 and K4 run in CI)."""

import json
import os
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import run_case  # noqa: E402

REPO = os.path.join(HERE, "..", "..")
GRANT_V3 = os.path.join(REPO, "docs/methodology/stage2/20261102-a1-cases-v3/grant-v3.json")


class Verdicts(unittest.TestCase):
    def test_by_cpu(self):
        v = run_case.by_cpu({"cortex-a72": ("pass", "ok"), "max": ("finding", "B RAM: extent drift")})
        self.assertEqual(v["verdict"], "finding")
        self.assertEqual(v["evidence"], ["cortex-a72: ok", "max: B RAM: extent drift"])
        self.assertEqual(run_case.by_cpu({"a": ("pass", ""), "b": ("error", "")})["verdict"], "error")
        self.assertEqual(run_case.by_cpu({"a": ("error", ""), "b": ("finding", "")})["verdict"], "finding")
        self.assertEqual(run_case.by_cpu({"a": ("pass", ""), "b": ("pass", "")})["verdict"], "pass")

    def test_m3_on_v3_grant(self):
        self.assertEqual(run_case.m3(GRANT_V3)["verdict"], "pass")

    def test_m3_rejects_bad_size(self):
        with open(GRANT_V3) as f:
            g = json.load(f)
        next(iter(g["partitions"].values()))[0]["size"] = "0x1001"  # partition A's RAM
        with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
            json.dump(g, f)
        try:
            self.assertEqual(run_case.m3(f.name)["verdict"], "finding")
        finally:
            os.unlink(f.name)


class Patch(unittest.TestCase):
    def test_apply_and_refuse(self):
        with tempfile.TemporaryDirectory() as d:
            src = os.path.join(d, "src")
            os.makedirs(src)
            with open(os.path.join(src, "a.c"), "w") as f:
                f.write("int x = 1;\n")
            good = os.path.join(d, "good.patch")
            with open(good, "w") as f:
                f.write("--- a/a.c\n+++ b/a.c\n@@ -1 +1 @@\n-int x = 1;\n+int x = 2;\n")
            bad = os.path.join(d, "bad.patch")
            with open(bad, "w") as f:
                f.write("--- a/a.c\n+++ b/a.c\n@@ -1 +1 @@\n-int y = 7;\n+int y = 8;\n")
            log = os.path.join(d, "patch.log")
            self.assertTrue(run_case.apply_patch(src, good, log))
            with open(os.path.join(src, "a.c")) as f:
                self.assertEqual(f.read(), "int x = 2;\n")
            self.assertFalse(run_case.apply_patch(src, bad, log))

    def test_cli_needs_case_and_grant(self):
        p = subprocess.run([sys.executable, os.path.join(HERE, "run_case.py"), "out"],
                           capture_output=True, text=True)
        self.assertEqual(p.returncode, 2)
        self.assertIn("--case", p.stderr)


if __name__ == "__main__":
    unittest.main()
