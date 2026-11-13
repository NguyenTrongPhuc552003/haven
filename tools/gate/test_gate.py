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
import tools  # noqa: E402

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
            self.assertEqual(fd.save_baseline(p, [old, blk]), (1, 1))  # BLOCKER never kept
            base = fd.load_baseline(p)
            self.assertEqual(json.load(open(p))["schema"], fd.BASELINE_SCHEMA)
        allf = [old, new, crit, blk]
        self.assertEqual(fd.blocking(allf, base, "pre-commit"), [blk])
        self.assertEqual(fd.blocking(allf, base, "pre-push"), [new, crit, blk])
        self.assertEqual(fd.blocking(allf, base, "ci"), [new, crit, blk])
        self.assertEqual(fd.blocking(allf, base, "deep"), [])

    def test_identical_findings_are_counted(self):
        # L1: two findings on identical lines of one file share a key; a third
        # copy must block although its key is known
        a = fd.Finding("t", "r", "f.c", 1, "MAJOR", "m", code="x <<= 1;")
        b = fd.Finding("t", "r", "f.c", 9, "MAJOR", "m", code="x <<= 1;")
        c = fd.Finding("t", "r", "f.c", 20, "MAJOR", "m", code="x <<= 1;")
        tool = fd.Finding("gate", "tool-failed", "", 0, "MAJOR", "m", code="ruff")
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, "baseline.json")
            self.assertEqual(fd.save_baseline(p, [a, b, tool]), (1, 2))  # the gate's own finding not kept
            base = fd.load_baseline(p)
            with open(p, "w") as f:
                json.dump({"schema": "haven-gate-baseline/1", "findings": [{"key": a.key}]}, f)
            old = fd.load_baseline(p)
        self.assertEqual(base, {a.key: 2})
        self.assertEqual(old, {a.key: 1})
        self.assertEqual(fd.blocking([a, b], base, "pre-push"), [])
        self.assertEqual(fd.blocking([a, b, c], base, "pre-push"), [a, b, c])
        self.assertEqual(fd.blocking([a, b], old, "ci"), [a, b])
        self.assertEqual(fd.blocking([tool], base, "pre-push"), [tool])


class ToolFailure(unittest.TestCase):
    def test_failure_without_findings_is_reported(self):
        # L2: an error exit with nothing parsed must not look like a clean run
        f = fd.Finding("t", "r", "f.c", 1, "MAJOR", "m", code="x")
        self.assertEqual(tools.checked("cppcheck", 0, "", []), [])
        self.assertEqual(tools.checked("cppcheck", 1, "", [f]), [f])
        out = tools.checked("clang-tidy", 1, "Error: no checks enabled.", [])
        self.assertEqual([(x.tool, x.rule, x.severity) for x in out], [("gate", "tool-failed", "MAJOR")])
        self.assertIn("no checks enabled", out[0].message)


# Recorded on the Mac (2026-11-13): golangci-lint 2.15.0 and checkpatch.pl
# from Linux v6.18 on small probe files. The govulncheck "finding" objects
# follow its documented v1.0.0 protocol (a probe module had none).
GOLANGCI = {"Issues": [
     {
      "FromLinter": "errcheck",
      "Text": "Error return value of `os.Remove` is not checked",
      "Pos": {
       "Filename": "a.go",
       "Line": 9
      }
     },
     {
      "FromLinter": "govet",
      "Text": "printf: fmt.Printf format %d reads arg #1, but call has 0 args",
      "Pos": {
       "Filename": "a.go",
       "Line": 13
      }
     },
     {
      "FromLinter": "ineffassign",
      "Text": "ineffectual assignment to x",
      "Pos": {
       "Filename": "a.go",
       "Line": 10
      }
     },
     {
      "FromLinter": "unused",
      "Text": "func unused is unused",
      "Pos": {
       "Filename": "a.go",
       "Line": 16
      }
     }
    ]}
CHECKPATCH = """t.c:1: WARNING:SPDX_LICENSE_TAG: Missing or malformed SPDX-License-Identifier tag in line 1
t.c:2: ERROR:OPEN_BRACE: open brace '{' following function definitions go on the next line
t.c:3: CHECK:SPACING: spaces preferred around that '+'
"""
GOVULN = """{"config": {"protocol_version": "v1.0.0"}}
{"finding": {"osv": "GO-2026-0001", "fixed_version": "v1.2.4",
 "trace": [{"module": "example.com/m", "version": "v1.2.3"}]}}
{"finding": {"osv": "GO-2026-0001", "fixed_version": "v1.2.4",
 "trace": [{"module": "example.com/m", "version": "v1.2.3", "package": "example.com/m/p", "function": "Parse"}]}}
{"finding": {"osv": "GO-2026-0002", "trace": [{"module": "example.com/n", "version": "v0.1.0"}]}}
"""


class GoAndKernel(unittest.TestCase):
    def test_gofmt(self):
        out = rules.parse_gofmt("core/a.go\ncmd/b.go\n")
        self.assertEqual([(f.file, f.severity) for f in out], [("core/a.go", "MAJOR"), ("cmd/b.go", "MAJOR")])

    def test_golangci(self):
        out = {f.rule: f.severity for f in rules.parse_golangci(GOLANGCI)}
        self.assertEqual(out, {"errcheck": "MAJOR", "govet": "CRITICAL", "ineffassign": "MAJOR",
                               "unused": "MAJOR"})
        sa = rules.parse_golangci({"Issues": [{"FromLinter": "staticcheck", "Text": "SA4006: x never used",
                                               "Pos": {"Filename": "a.go", "Line": 3}},
                                              {"FromLinter": "staticcheck", "Text": "S1002: omit comparison",
                                               "Pos": {"Filename": "a.go", "Line": 4}}]})
        self.assertEqual([(f.rule, f.severity) for f in sa],
                         [("staticcheck/SA4006", "CRITICAL"), ("staticcheck/S1002", "MINOR")])

    def test_govulncheck(self):
        out = {f.rule: f.severity for f in rules.parse_govulncheck(GOVULN)}
        self.assertEqual(out, {"GO-2026-0001": "CRITICAL", "GO-2026-0002": "INFO"})

    def test_checkpatch(self):
        out = [(f.rule, f.line, f.severity) for f in rules.parse_checkpatch(CHECKPATCH)]
        self.assertEqual(out, [("SPDX_LICENSE_TAG", 1, "MINOR"), ("OPEN_BRACE", 2, "MAJOR"),
                               ("SPACING", 3, "INFO")])

    def test_tool_paths(self):
        cfg = {"c_paths": ["src", "tools"], "go_paths": ["core"], "tool_paths": {"checkpatch": ["src"]}}
        self.assertEqual(gate.tool_paths(cfg, "checkpatch"), ["src"])
        self.assertEqual(gate.tool_paths(cfg, "cppcheck"), ["src", "tools"])
        self.assertEqual(gate.tool_paths(cfg, "lizard"), ["src", "tools", "core"])
        self.assertIsNone(gate.tool_paths(cfg, "jscpd"))


# Formats of AddressSanitizer, UBSan, the Go race detector and Go fuzzing
# as printed by clang 23 and Go 1.27 (trimmed).
ASAN = """==1==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x6020 at pc 0x1 bp 0x2 sp 0x3
WRITE of size 4 at 0x6020 thread T0
    #0 0x100 in __asan_memcpy (libclang_rt.asan_osx_dynamic.dylib:arm64e+0x1)
    #1 0x101 in fill /repo/tests/unit/test_x.c:12:5
    #2 0x102 in main /repo/tests/unit/test_x.c:30:2
"""
UBSAN = "/repo/src/core/a.c:7:9: runtime error: left shift of 1 by 31 places cannot be represented in type 'int'\n"
RACE = """==================
WARNING: DATA RACE
Write at 0x00c by goroutine 8:
  example.com/m/p.TestRace.func1()
      /repo/p/race_test.go:11 +0x3c
--- FAIL: TestRace (0.00s)
    testing.go:1490: race detected during execution of test
"""
FUZZ = """--- FAIL: FuzzParseSums (0.21s)
    --- FAIL: FuzzParseSums (0.00s)
        fuzz_test.go:101: unsafe name accepted: "/"

    Failing input written to testdata/fuzz/FuzzParseSums/a8c0651f3f18f888
"""


class Deep(unittest.TestCase):
    def test_sanitizers(self):
        out = [(f.rule, f.file, f.line, f.severity) for f in rules.parse_sanitizers(ASAN + UBSAN, "/repo")]
        self.assertEqual(out, [("asan/heap-buffer-overflow", "tests/unit/test_x.c", 12, "CRITICAL"),
                               ("ubsan", "src/core/a.c", 7, "CRITICAL")])
        short = ASAN.replace("/repo/tests/unit/test_x.c", "test_x.c")
        out = rules.parse_sanitizers(short, "/repo", lambda p: "tests/unit/" + p if "/" not in p else p)
        self.assertEqual(out[0].file, "tests/unit/test_x.c")

    def test_race(self):
        out = [(f.rule, f.file, f.line, f.severity) for f in rules.parse_go_test(RACE, "go-race", "/repo")]
        self.assertEqual(out, [("data-race", "p/race_test.go", 11, "CRITICAL"), ("test-failed", "", 0, "MAJOR")])

    def test_fuzz(self):
        out = rules.parse_go_fuzz(FUZZ, "FuzzParseSums", "./core/domain/demo")
        self.assertEqual([(f.rule, f.file, f.severity) for f in out],
                         [("FuzzParseSums", "core/domain/demo/testdata/fuzz/FuzzParseSums/a8c0651f3f18f888",
                           "CRITICAL")])
        self.assertIn("unsafe name", out[0].message)
        self.assertEqual(rules.parse_go_fuzz("ok\n", "FuzzX"), [])


# Kernel checks: lines in the forms sparse 0.6 and coccicheck MODE=report
# print (from their documentation; recorded output replaces them after the
# first CI run).
SPARSE = """/k/src/core/chardev.c:88:13: warning: incorrect type in argument 1 (different address spaces)
/k/src/core/chardev.c:90:9: warning: unused variable 'x' [-Wunused-variable]
/k/src/core/kdal.c:12:1: error: undefined identifier 'foo'
/usr/src/linux/include/linux/x.h:1:1: warning: outside the tree
"""
COCCI = """/k/src/core/registry.c:40:8-14: WARNING: kzalloc should be used for buf, instead of kmalloc/memset
/k/src/core/debugfs.c:12:1-5: ERROR: missing put_device; call of_find_device_by_node on line 10
please check for false positives in the output before submitting a patch.
"""


class Kernel(unittest.TestCase):
    def test_sparse(self):
        out = [(f.rule, f.file, f.line, f.severity) for f in rules.parse_sparse(SPARSE, "/k")]
        self.assertEqual(out, [("warning", "src/core/chardev.c", 88, "MAJOR"),
                               ("error", "src/core/kdal.c", 12, "CRITICAL")])

    def test_coccinelle(self):
        out = [(f.rule, f.file, f.line, f.severity) for f in rules.parse_coccinelle(COCCI, "/k")]
        self.assertEqual(out, [("warning", "src/core/registry.c", 40, "MINOR"),
                               ("error", "src/core/debugfs.c", 12, "MAJOR")])

    def test_not_run_without_a_tree(self):
        class Ctx:
            cfg = {"kernel": {"env": "GATE_TEST_NO_SUCH_VAR", "make": ["make"]}}
            stage, root = "ci", "."
        res, out = tools.kernel_make(Ctx(), "sparse", [])
        self.assertIsNone(res)
        self.assertEqual([(f.rule, f.severity) for f in out], [("not-run", "MAJOR")])


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
