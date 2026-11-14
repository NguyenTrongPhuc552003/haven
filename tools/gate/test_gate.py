# SPDX-License-Identifier: Apache-2.0
"""Unit tests for the quality gate: recorded tool outputs mapped to the
severity scale, finding identity, baseline and stage policy."""
import json
import os
import shutil
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
            with open(p) as f:
                self.assertEqual(json.load(f)["schema"], fd.BASELINE_SCHEMA)
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


class CiOnlyTools(unittest.TestCase):
    """D16: a baseline written where a tool cannot run keeps that tool's
    entries, and only such tools' findings can be added from a CI log."""

    def run_baseline(self, d, *extra):
        import contextlib
        import io
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = gate.main(["--root", d, "baseline"] + list(extra))
        return code, out.getvalue()

    def profile(self, d, ci):
        os.makedirs(os.path.join(d, "quality"), exist_ok=True)
        with open(os.path.join(d, "quality", "gate.json"), "w") as f:
            json.dump({"stages": {"ci": ci}, "kernel": {"env": "GATE_TEST_NO_SUCH_VAR", "make": ["make"]}}, f)
        with open(os.path.join(d, "src.c"), "w") as f:
            f.write("int f(void);\nint g(void) { return f(); }\n")

    def test_entries_of_a_tool_that_did_not_run_are_kept(self):
        with tempfile.TemporaryDirectory() as d:
            self.profile(d, ["sparse"])
            kept = fd.Finding("sparse", "warning", "src.c", 1, "MAJOR", "not declared", root=d)
            fd.save_baseline(os.path.join(d, "quality", "baseline.json"), [kept])
            code, out = self.run_baseline(d)
            self.assertEqual(code, 0)
            self.assertIn("entries kept: sparse (1)", out)
            self.assertEqual(fd.load_baseline(os.path.join(d, "quality", "baseline.json")), {kept.key: 1})

    def test_reported_findings_only_for_tools_that_did_not_run(self):
        with tempfile.TemporaryDirectory() as d:
            self.profile(d, ["sparse"])
            log = os.path.join(d, "ci.log")
            with open(log, "w") as f:
                f.write("2026-11-13T15:55:56Z MAJOR    sparse/warning src.c:1: symbol 'f' was not declared\n"
                        "2026-11-13T15:55:56Z CRITICAL clang-tidy/bugprone-x src.c:2: something\n")
            code, out = self.run_baseline(d, "--add-reported", log)
            self.assertIn("adding: MAJOR    sparse/warning src.c:1", out)
            self.assertIn("refusing (the tool ran here): CRITICAL clang-tidy/bugprone-x", out)
            tools_in = {e["tool"] for e in fd.load_baseline_entries(os.path.join(d, "quality", "baseline.json"))}
            self.assertEqual(tools_in, {"sparse"})


LSAN = """==9==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 64 byte(s) in 1 object(s) allocated from:
    #0 0x7f in malloc (/lib/libasan.so.8+0xfe)
    #1 0x55 in lexer_push /repo/compiler/lexer.c:447:12
    #2 0x56 in main /repo/compiler/main.c:30:2

Indirect leak of 8 byte(s) in 1 object(s) allocated from:
    #0 0x7f in malloc (/lib/libasan.so.8+0xfe)
    #1 0x57 in tok_new /repo/compiler/lexer.c:90:9
"""


class DeepReport(unittest.TestCase):
    def test_leaks(self):
        out = [(f.rule, f.file, f.line) for f in rules.parse_sanitizers(LSAN, "/repo")]
        self.assertEqual(out, [("lsan/direct-leak", "compiler/lexer.c", 447),
                               ("lsan/indirect-leak", "compiler/lexer.c", 90)])

    def test_deep_lists_every_level_and_fail_on_new(self):
        import contextlib
        import io
        with tempfile.TemporaryDirectory() as d:
            os.makedirs(os.path.join(d, "quality"))
            with open(os.path.join(d, "quality", "gate.json"), "w") as f:
                json.dump({"stages": {"deep": ["scan"]}}, f)
            subprocess.run(["git", "init", "-q", d], check=True)
            with open(os.path.join(d, "a.txt"), "w") as f:
                f.write("see /Users/someone/x\n")
            subprocess.run(["git", "-C", d, "add", "a.txt"], check=True)
            for extra, want in (([], 0), (["--fail-on-new"], 1)):
                out = io.StringIO()
                with contextlib.redirect_stdout(out):
                    code = gate.main(["--root", d, "deep"] + extra)
                # the scan's home path is a BLOCKER: deep lists it and blocks nothing,
                # --fail-on-new turns it into a failing exit
                self.assertEqual(code, want)
                self.assertIn("BLOCKER  scan/", out.getvalue())


class Copies(unittest.TestCase):
    """L7: a repository's copy of an engine template must not drift."""

    def test_drift(self):
        class Ctx:
            stage = "ci"
            root = None
            cfg = {"copies": {"scripts/gate": "repo-gate.sh"}}
        with tempfile.TemporaryDirectory() as d:
            Ctx.root = d
            os.makedirs(os.path.join(d, "scripts"))
            with open(os.path.join(tools.TEMPLATES, "repo-gate.sh"), "rb") as f:
                body = f.read()
            with open(os.path.join(d, "scripts", "gate"), "wb") as f:
                f.write(body)
            self.assertEqual(tools.copies(Ctx(), []), [])
            with open(os.path.join(d, "scripts", "gate"), "ab") as f:
                f.write(b"# local change\n")
            out = tools.copies(Ctx(), [])
            self.assertEqual([(f.rule, f.file, f.severity) for f in out], [("copy-drift", "scripts/gate", "MAJOR")])


class GitleaksTracked(unittest.TestCase):
    """L6: gitleaks sees only the files it is given (tracked files)."""

    @unittest.skipUnless(shutil.which("gitleaks"), "gitleaks not installed")
    def test_untracked_file_is_not_scanned(self):
        class Ctx:
            stage, cfg = "ci", {}
        import hashlib
        # built at run time (no secret-like text in the file); random enough
        # for gitleaks' entropy check
        token = "ghp_" + hashlib.sha256(b"gate test").hexdigest()[:36].replace("a", "Q")
        with tempfile.TemporaryDirectory() as d:
            Ctx.root = d
            for name in ("tracked.txt", "untracked.txt"):
                with open(os.path.join(d, name), "w") as f:
                    f.write(f"token = {token}\n" if name == "untracked.txt" else "nothing here\n")
            self.assertEqual(tools.gitleaks(Ctx(), ["tracked.txt"]), [])
            self.assertTrue(tools.gitleaks(Ctx(), ["tracked.txt", "untracked.txt"]))


# Recorded on the Mac (clang 23.1.5, libFuzzer): a seeded abort (trimmed).
LIBFUZZER = """==38861== ERROR: libFuzzer: deadly signal
    #4 0x000102230c08 in LLVMFuzzerTestOneInput seedfuzz.c:20
SUMMARY: libFuzzer: deadly signal
MS: 0 ; base unit: 0000000000000000000000000000000000000000
artifact_prefix='/repo/build-fuzz/artifacts-fuzz_lex_parse/'; Test unit written to /repo/build-fuzz/artifacts-fuzz_lex_parse/crash-2453036b25
"""
LIBFUZZER_ASAN = """==7==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x60 at pc 0x1
SUMMARY: AddressSanitizer: heap-buffer-overflow /repo/compiler/lexer.c:40 in lx_cur
Test unit written to ./crash-aa
"""


class LibFuzzer(unittest.TestCase):
    def test_reports(self):
        out = rules.parse_libfuzzer(LIBFUZZER, "fuzz_lex_parse", "/repo")
        self.assertEqual([(f.rule, f.file, f.severity) for f in out],
                         [("fuzz_lex_parse/deadly-signal", "build-fuzz/artifacts-fuzz_lex_parse/crash-2453036b25",
                           "CRITICAL")])
        asan = rules.parse_libfuzzer(LIBFUZZER_ASAN, "fuzz_lex_parse", "/repo")
        self.assertEqual(asan[0].rule, "fuzz_lex_parse/heap-buffer-overflow")
        # a clean run: its run count as INFO (L10)
        done = rules.parse_libfuzzer("Done 137436 runs in 61 second(s)\n", "fuzz_lex_parse")
        self.assertEqual([(f.rule, f.severity) for f in done], [("fuzz_lex_parse/summary", "INFO")])
        self.assertIn("137436 runs in 61 s", done[0].message)
        self.assertEqual(rules.parse_libfuzzer("INFO: Seed: 1\n", "fuzz_lex_parse"), [])


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


class Stale(unittest.TestCase):
    """L9: the clang-tidy database is remade when a CMake input is newer."""

    def test_newer_cmake_input(self):
        with tempfile.TemporaryDirectory() as d:
            subprocess.run(["git", "-C", d, "init", "-q"], check=True)
            os.makedirs(os.path.join(d, "sub", "build"))
            cml = os.path.join(d, "sub", "CMakeLists.txt")
            db = os.path.join(d, "sub", "build", "compile_commands.json")
            for path in (cml, db):
                with open(path, "w") as f:
                    f.write("x\n")
            subprocess.run(["git", "-C", d, "add", "sub/CMakeLists.txt"], check=True)
            ctx = gate.Ctx(d, {}, "ci")
            os.utime(cml, (1000, 1000))
            os.utime(db, (2000, 2000))
            self.assertFalse(tools.stale(ctx, db))
            os.utime(cml, (3000, 3000))
            self.assertTrue(tools.stale(ctx, db))
            os.remove(db)
            self.assertTrue(tools.stale(ctx, db))


class Doctor(unittest.TestCase):
    """doctor lists the programs a runner calls, not the runner's name."""

    def test_programs(self):
        cfg = {"libfuzzer": {"cc": "clang-18"}, "checkpatch": {"dir": "/opt/cp"}}
        self.assertEqual(tools.programs(cfg, "libfuzzer"), ["clang-18", "cmake"])
        self.assertEqual(tools.programs(cfg, "go-race"), ["go"])
        self.assertEqual(tools.programs(cfg, "coccinelle"), ["make", "spatch"])
        self.assertEqual(tools.programs(cfg, "checkpatch"), ["perl", os.path.join("/opt/cp", "checkpatch.pl")])
        self.assertEqual(tools.programs(cfg, "copies"), [])

    def test_no_runner_name_as_program(self):
        cfg = {"stages": {"deep": ["copies", "sanitizers", "libfuzzer", "go-race"]},
               "libfuzzer": {"cc": "no-such-cc-for-the-test"}}
        lines = gate.doctor(".", cfg)
        self.assertIn("copies         no program", lines)
        cols = {tuple(line.split()[:2]) for line in lines}
        for pair in (("libfuzzer", "no-such-cc-for-the-test"), ("libfuzzer", "cmake"), ("go-race", "go"),
                     ("sanitizers", "cmake")):
            self.assertIn(pair, cols)
        self.assertIn("MISSING", next(x for x in lines if "no-such-cc-for-the-test" in x))


if __name__ == "__main__":
    unittest.main()
