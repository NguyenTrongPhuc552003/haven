# SPDX-License-Identifier: Apache-2.0
"""Unit tests for the quality gate: recorded tool outputs mapped to the
severity scale, finding identity, baseline and stage policy."""
import json
import os
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import findings as fd  # noqa: E402
import rules  # noqa: E402
import gate  # noqa: E402

# built at run time so this file holds no literal token
TOKEN = "ghp" + "_" + "A1b2C3d4E5f6G7h8I9j0K1l2M3n4O5p6Q7r8"


class Scan(unittest.TestCase):
    def test_secret_is_blocker(self):
        f = rules.scan_text("a.c", f"x = '{TOKEN}';")
        self.assertEqual([(x.rule, x.severity) for x in f], [("github-token", "BLOCKER")])

    def test_home_paths(self):
        self.assertEqual(len(rules.scan_text("a.log", "cd /Users/alice/src")), 1)
        self.assertEqual(len(rules.scan_text("a.log", "cd /home/bob/x")), 1)
        for ok in ("/Users/user/x", "/home/runner/work/x", "`/Users/...`", "/Users/<login>"):
            self.assertEqual(rules.scan_text("a.log", ok), [], ok)

    def test_mac_host_and_private_names(self):
        self.assertEqual(rules.scan_text("l", "Linux (bob@Bobs-Mac.local)")[0].rule, "mac-host-name")
        self.assertEqual(rules.scan_text("l", "(user@host)"), [])
        self.assertEqual(rules.scan_text("l", "by carol", ["carol"])[0].rule, "private-name")


class Parsers(unittest.TestCase):
    def test_cppcheck(self):
        out = ("src/a.c\t3\terror\tnullPointer\tNull pointer dereference\n"
               "src/a.c\t4\tstyle\tunusedVariable\tUnused\n"
               "drivers/linux/m.c\t1\terror\tsyntaxError\tsyntax error\n"
               "src/a.c\t0\tinformation\tmissingIncludeSystem\tx\n")
        got = [(f.rule, f.severity) for f in rules.parse_cppcheck(out)]
        self.assertEqual(got, [("nullPointer", "CRITICAL"), ("unusedVariable", "MINOR"), ("syntaxError", "INFO")])

    def test_clang_tidy(self):
        out = ("src/a.c:10:5: warning: Array access results in a null pointer dereference "
               "[clang-analyzer-core.NullDereference]\n"
               "src/a.c:11:1: warning: use of a signed integer operand [bugprone-signed-bitwise]\n"
               "src/a.c:11:1: warning: use of a signed integer operand [bugprone-signed-bitwise]\n"
               "src/a.c:12:1: warning: padding [clang-analyzer-optin.performance.Padding]\n"
               "src/a.c:13:1: warning: suspicious sizeof [bugprone-sizeof-expression]\n"
               "src/a.c:14:1: warning: memset is insecure "
               "[clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling]\n")
        got = [(f.rule, f.severity) for f in rules.parse_clang_tidy(out)]
        self.assertEqual(got, [("clang-analyzer-core.NullDereference", "CRITICAL"),
                               ("bugprone-signed-bitwise", "MAJOR"),
                               ("clang-analyzer-optin.performance.Padding", "MINOR"),
                               ("bugprone-sizeof-expression", "CRITICAL"),
                               ("clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling", "MINOR")])

    def test_lizard(self):
        out = ("12,20,90,2,30,\"f@1-30@src/a.c\",\"src/a.c\",\"f\",\"f(int a)\",1,30\n"
               "8,3,40,1,140,\"g@40-180@src/a.c\",\"src/a.c\",\"g\",\"g()\",40,180\n")
        got = [(f.rule, f.message) for f in rules.parse_lizard(out, 15, 100)]
        self.assertEqual(got, [("complexity", "f: cyclomatic complexity 20 > 15"),
                               ("function-length", "g: 140 lines > 100")])

    def test_jscpd_only_files(self):
        data = {"duplicates": [{"lines": 20, "fragment": "x", "firstFile": {"name": "a.c", "start": 1},
                                "secondFile": {"name": "b.c", "start": 5}}],
                "statistics": {"total": {"duplicatedLines": 20, "lines": 1000, "percentage": 2}}}
        self.assertEqual([f.severity for f in rules.parse_jscpd(data)], ["MAJOR", "INFO"])
        self.assertEqual([f.severity for f in rules.parse_jscpd(data, {"c.c"})], ["INFO"])

    def test_ruff(self):
        data = [{"code": "F821", "filename": "t.py", "location": {"row": 1}, "message": "undefined"},
                {"code": "F401", "filename": "t.py", "location": {"row": 2}, "message": "unused"},
                {"code": "FURB167", "filename": "t.py", "location": {"row": 3}, "message": "refurb"},
                {"code": "E501", "filename": "t.py", "location": {"row": 4}, "message": "long"}]
        self.assertEqual([f.severity for f in rules.parse_ruff(data)], ["CRITICAL", "MAJOR", "MINOR", "MINOR"])

    def test_shellcheck(self):
        data = {"comments": [{"file": "s.sh", "line": 1, "level": "error", "code": 1009, "message": "m"},
                             {"file": "s.sh", "line": 2, "level": "style", "code": 2086, "message": "m"}]}
        self.assertEqual([f.severity for f in rules.parse_shellcheck(data)], ["CRITICAL", "MINOR"])


class Identity(unittest.TestCase):
    def test_moving_code_keeps_the_key(self):
        a = fd.Finding("t", "r", "f.c", 3, "MAJOR", "m", code="  x  = 1;")
        b = fd.Finding("t", "r", "f.c", 30, "MAJOR", "m", code="x = 1;")
        c = fd.Finding("t", "r", "f.c", 3, "MAJOR", "m", code="x = 2;")
        self.assertEqual(a.key, b.key)
        self.assertNotEqual(a.key, c.key)

    def test_baseline_and_policy(self):
        old = fd.Finding("t", "r", "f.c", 1, "MAJOR", "m", code="old")
        new = fd.Finding("t", "r", "f.c", 2, "MAJOR", "m", code="new")
        crit = fd.Finding("t", "c", "f.c", 3, "CRITICAL", "m", code="crit")
        blk = fd.Finding("scan", "github-token", "f.c", 4, "BLOCKER", "m", code="tok")
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, "baseline.json")
            self.assertEqual(fd.save_baseline(p, [old, blk]), 1)  # BLOCKER never kept
            base = fd.load_baseline(p)
            self.assertEqual(json.load(open(p))["schema"], fd.BASELINE_SCHEMA)
        allf = [old, new, crit, blk]
        self.assertEqual(fd.blocking(allf, base, "pre-commit"), [blk])
        self.assertEqual(fd.blocking(allf, base, "pre-push"), [new, crit, blk])
        self.assertEqual(fd.blocking(allf, base, "ci"), [new, crit, blk])
        self.assertEqual(fd.blocking(allf, base, "deep"), [])


if __name__ == "__main__":
    unittest.main()


class BaseRef(unittest.TestCase):
    def test_pushed_remote_then_newest_main(self):
        with tempfile.TemporaryDirectory() as d:
            env = dict(os.environ, GIT_AUTHOR_NAME="t", GIT_AUTHOR_EMAIL="t@t",
                       GIT_COMMITTER_NAME="t", GIT_COMMITTER_EMAIL="t@t")

            def run(*a, when=None):
                e = dict(env, GIT_COMMITTER_DATE=when) if when else env
                return subprocess.run(["git", "-C", d] + list(a), env=e, check=True,
                                      capture_output=True, text=True).stdout.strip()
            run("init", "-q")
            run("commit", "-q", "--allow-empty", "-m", "old", when="2026-01-01T00:00:00")
            run("update-ref", "refs/remotes/stale/main", "HEAD")
            run("commit", "-q", "--allow-empty", "-m", "new", when="2026-06-01T00:00:00")
            run("update-ref", "refs/remotes/fresh/main", "HEAD")
            self.assertEqual(gate.base_ref(d), "fresh/main")
            self.assertEqual(gate.base_ref(d, "stale"), "stale/main")
            self.assertEqual(gate.base_ref(d, "absent"), "fresh/main")
