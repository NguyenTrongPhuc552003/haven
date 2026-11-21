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



class ReadbackVerdict(unittest.TestCase):
    """The C1 first run's defect 1: exit 1 from a Python error is not a finding."""

    def test_finding_needs_findings(self):
        rep = {"findings": [["B RAM", "extent drift", "size 0x4200000"]]}
        self.assertEqual(run_case.m6_verdict(1, rep, "/nonexistent"), ("finding", "B RAM: extent drift"))
        self.assertEqual(run_case.m6_verdict(0, {"findings": []}, "/nonexistent"), ("pass", "matches the grant"))

    def test_python_error_is_error(self):
        with tempfile.NamedTemporaryFile("w", suffix=".log", delete=False) as f:
            f.write("read at: marker\nTraceback (most recent call last):\n"
                    "FileNotFoundError: [Errno 2] No such file or directory: 'dtc'\n")
        try:
            v, e = run_case.m6_verdict(1, None, f.name)
        finally:
            os.unlink(f.name)
        self.assertEqual(v, "error")
        self.assertIn("No such file or directory: 'dtc'", e)

    def test_probe_failure(self):
        v, e = run_case.m6_verdict(2, {"findings": [], "probe_failure": "HAVEN halted"}, "/nonexistent")
        self.assertEqual((v, e), ("error", "probe failure: HAVEN halted"))



class Evidence(unittest.TestCase):
    def test_heldout_quotes_no_log(self):
        """Row K7e: a held-out case's evidence holds no line of its logs."""
        with tempfile.TemporaryDirectory() as d:
            log = os.path.join(d, "build.log")
            with open(log, "w") as f:
                f.write("src/core/partition.c:2: error\nint secret_patch_text = ;\n")
            try:
                run_case.set_quoting("heldout")
                self.assertEqual(run_case.tail(log), [])
                self.assertNotIn("secret_patch_text", run_case.m6_verdict(1, None, log)[1])
                run_case.set_quoting("written")
                self.assertIn("int secret_patch_text = ;", run_case.tail(log))
            finally:
                run_case.set_quoting("written")

    def test_printed_except_heldout(self):
        case = {"source": "written", "methods": {
            "M1": {"verdict": "pass", "evidence": ["ctest exit 0"]},
            "M6": {"verdict": "finding", "evidence": ["max: B RAM: extent drift"]},
            "M7": {"verdict": "n/a", "evidence": ["not wired"]}}}
        self.assertEqual(run_case.evidence_lines(case), ["M6 finding: max: B RAM: extent drift"])
        case["source"] = "heldout"
        self.assertEqual(run_case.evidence_lines(case), [])


class CtestNames(unittest.TestCase):
    """The C1 first run's defect 2: M1 names the failing tests."""

    def test_failed_tests(self):
        out = ("97% tests passed, 1 tests failed out of 30\n\nTotal Test time (real) =   0.18 sec\n\n"
               "The following tests FAILED:\n\t  9 - test_partition_layout (Failed)\n"
               "\t 12 - test_vgic (SEGFAULT)\nErrors while running CTest\n")
        self.assertEqual(run_case.failed_tests(out), ["test_partition_layout", "test_vgic"])
        self.assertEqual(run_case.failed_tests("100% tests passed\n"), [])


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
