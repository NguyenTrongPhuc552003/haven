# SPDX-License-Identifier: Apache-2.0
"""Severity mapping: one parser per tool, from the tool's output text to
findings. Pure functions over text, so the tests feed recorded outputs.
The scale is BLOCKER, CRITICAL, MAJOR, MINOR, INFO (docs/quality/gate.md)."""
import csv
import io
import json
import os
import re

from findings import Finding

# ---- the engine's own scan: secrets, personal names, absolute paths ----

SECRETS = (
    ("github-token", re.compile(r"\b(?:ghp|gho|ghs|ghu|ghr)_[A-Za-z0-9]{36}\b|\bgithub_pat_[A-Za-z0-9_]{60,}")),
    ("private-key", re.compile(r"-----BEGIN (?:RSA |EC |OPENSSH |DSA |)PRIVATE KEY-----")),
    ("aws-key", re.compile(r"\bAKIA[0-9A-Z]{16}\b")),
)
# /Users/<name>/ or /home/<name>/ with a real name; the placeholders the records
# use (user, runner, <login>) and documentation ellipses do not match
ABS_PATH = re.compile(r"/(?:Users|home)/(?!user\b|runner\b|ubuntu\b)[A-Za-z][A-Za-z0-9._-]*/")
MAC_HOST = re.compile(r"@[A-Za-z0-9][A-Za-z0-9-]*\.local\b")


def scan_text(path, text, private_names=()):
    """BLOCKER findings for secrets, absolute home paths, Mac host names and
    the names in private_names (set only on the owner's machine)."""
    out = []
    for n, line in enumerate(text.splitlines(), 1):
        for rule, rx in SECRETS:
            if rx.search(line):
                out.append(Finding("scan", rule, path, n, "BLOCKER", "secret pattern", code=line))
        if ABS_PATH.search(line):
            out.append(Finding("scan", "absolute-home-path", path, n, "BLOCKER",
                               "absolute home path (use a relative path or user)", code=line))
        if MAC_HOST.search(line):
            out.append(Finding("scan", "mac-host-name", path, n, "BLOCKER",
                               "Mac host name (use user@host)", code=line))
        for name in private_names:
            if name and name in line:
                out.append(Finding("scan", "private-name", path, n, "BLOCKER",
                                   "a name listed in GATE_PRIVATE_NAMES", code=line))
    return out


# ---- cppcheck: --template='{file}\t{line}\t{severity}\t{id}\t{message}' ----

CPPCHECK = {"error": "CRITICAL", "warning": "MAJOR", "performance": "MINOR",
            "portability": "MINOR", "style": "MINOR", "information": "INFO"}


CPPCHECK_UNANALYSED = {"syntaxError", "preprocessorErrorDirective", "unknownMacro"}


def parse_cppcheck(text, root="."):
    out = []
    for line in text.splitlines():
        parts = line.split("\t")
        if len(parts) != 5 or parts[2] not in CPPCHECK:
            continue
        path, ln, sev, rule, msg = parts
        if rule in ("missingInclude", "missingIncludeSystem", "checkersReport", "unmatchedSuppression"):
            continue
        # the tool could not analyse the file (a kernel module without kernel
        # headers; an #error branch for configurations the build refuses):
        # INFO, calibrated on HAVEN's first baseline
        level = "INFO" if rule in CPPCHECK_UNANALYSED else CPPCHECK[sev]
        out.append(Finding("cppcheck", rule, path, int(ln or 0), level, msg, root=root))
    return out


# ---- clang-tidy: "file:line:col: warning: message [check]" ----

TIDY_LINE = re.compile(r"^(.+?):(\d+):\d+: (warning|error): (.*) \[([\w.,-]+)\]$")
# bugprone and cert checks that are about style or naming rather than likely
# bugs: MAJOR instead of CRITICAL
# Calibrated on HAVEN's first baseline (2026-11-13, record
# .../20261113-quality-gate/): signed-bitwise is hygiene (undefined only when a
# shift reaches the sign bit), MAJOR not CRITICAL.
TIDY_MAJOR = {
    "bugprone-easily-swappable-parameters", "bugprone-reserved-identifier",
    "cert-dcl37-c", "cert-dcl51-cpp", "bugprone-narrowing-conversions",
    "bugprone-assignment-in-if-condition", "bugprone-macro-parentheses",
    "bugprone-implicit-widening-of-multiplication-result", "cert-err33-c",
    "bugprone-branch-clone", "bugprone-multi-level-implicit-pointer-conversion",
    "bugprone-signed-bitwise", "hicpp-signed-bitwise",
}


def tidy_severity(check):
    # optin.performance (padding) and security.insecureAPI (asks for the
    # Annex K *_s functions, which freestanding code does not have): MINOR
    if check.startswith(("clang-analyzer-optin.performance", "clang-analyzer-security.insecureAPI")):
        return "MINOR"
    if check.startswith("clang-analyzer-"):
        return "CRITICAL"
    if check.startswith(("bugprone-", "cert-")):
        return "MAJOR" if check in TIDY_MAJOR else "CRITICAL"
    if check.startswith("clang-diagnostic-"):
        return "MAJOR"
    return "MINOR"


def parse_clang_tidy(text, root="."):
    out, seen = [], set()
    for line in text.splitlines():
        m = TIDY_LINE.match(line.strip())
        if not m:
            continue
        path, ln, _, msg, checks = m.groups()
        check = checks.split(",")[0]
        if (path, ln, check) in seen:
            continue
        seen.add((path, ln, check))
        out.append(Finding("clang-tidy", check, path, int(ln), tidy_severity(check), msg, root=root))
    return out


# ---- lizard --csv: NLOC,CCN,token,PARAM,length,location,file,function,long_name,start,end ----

def parse_lizard(text, ccn_limit, length_limit):
    out = []
    for row in csv.reader(io.StringIO(text)):
        if len(row) < 11 or not row[1].isdigit():
            continue
        ccn, length, path, name = int(row[1]), int(row[4]), row[6], row[7]
        if ccn > ccn_limit:
            out.append(Finding("lizard", "complexity", path, int(row[9]), "MAJOR",
                               f"{name}: cyclomatic complexity {ccn} > {ccn_limit}", code=name))
        if length > length_limit:
            out.append(Finding("lizard", "function-length", path, int(row[9]), "MAJOR",
                               f"{name}: {length} lines > {length_limit}", code=name))
    return out


# ---- jscpd --reporters json: duplicates[] with firstFile/secondFile ----

def parse_jscpd(data, only_files=None):
    """MAJOR per duplicated pair (only pairs touching only_files when given),
    plus one INFO with the duplicated percentage."""
    out = []
    for d in data.get("duplicates", []):
        a, b = d["firstFile"], d["secondFile"]
        if only_files is not None and a["name"] not in only_files and b["name"] not in only_files:
            continue
        frag = d.get("fragment", "")
        out.append(Finding("jscpd", "duplicate", b["name"], b.get("start", 0), "MAJOR",
                           f"{d.get('lines', 0)} lines duplicated from {a['name']}:{a.get('start', 0)}",
                           code=frag))
    total = data.get("statistics", {}).get("total", {})
    if total:
        out.append(Finding("jscpd", "summary", "", 0, "INFO",
                           f"duplicated lines {total.get('duplicatedLines', 0)} of {total.get('lines', 0)} "
                           f"({total.get('percentage', 0)} %)", code="summary"))
    return out


# ---- ruff check --output-format json ----

RUFF_CRITICAL = {"F821", "F822", "F823", "F811", "E999"}


def ruff_severity(code):
    if code in RUFF_CRITICAL or code.startswith("E9"):
        return "CRITICAL"
    if re.match(r"F\d", code):  # Pyflakes (FURB, refurb, is not)
        return "MAJOR"
    return "MINOR"


def parse_ruff(data, root="."):
    out = []
    for d in data:
        code = d.get("code") or "syntax"
        path = d["filename"]
        if path.startswith(root.rstrip("/") + "/"):
            path = path[len(root.rstrip("/")) + 1:]
        out.append(Finding("ruff", code, path, d["location"]["row"], ruff_severity(code),
                           d["message"], root=root))
    return out


# ---- shellcheck -f json1 ----

SHELLCHECK = {"error": "CRITICAL", "warning": "MAJOR", "info": "MINOR", "style": "MINOR"}


def parse_shellcheck(data, root="."):
    return [Finding("shellcheck", f"SC{c['code']}", c["file"], c["line"],
                    SHELLCHECK.get(c["level"], "MINOR"), c["message"], root=root)
            for c in data.get("comments", [])]


# ---- gitleaks dir -f json ----

def parse_gitleaks(data):
    return [Finding("gitleaks", d.get("RuleID", "secret"), d.get("File", ""), d.get("StartLine", 0),
                    "BLOCKER", d.get("Description", "secret"), code=d.get("Fingerprint", ""))
            for d in data or []]


# ---- gofmt -l: the files gofmt would change ----

def parse_gofmt(text):
    return [Finding("gofmt", "unformatted", f, 0, "MAJOR", "gofmt would change this file", code=f)
            for f in text.split() if f.endswith(".go")]


# ---- golangci-lint run --output.json.path=stdout: Issues[] ----

GO_CRITICAL = {"govet", "typecheck"}


def golangci_severity(linter, text):
    if linter in GO_CRITICAL:
        return "CRITICAL"
    if linter == "staticcheck":
        # SA: likely bugs; S1, ST1, QF: simplifications and style
        return "CRITICAL" if text.startswith("SA") else "MINOR"
    return "MAJOR"  # errcheck, ineffassign, unused


def parse_golangci(data, root="."):
    out = []
    for i in data.get("Issues") or []:
        linter, text, pos = i.get("FromLinter", "golangci"), i.get("Text", ""), i.get("Pos", {})
        # staticcheck prefixes its check ("SA4006: ..."); keep it as the rule
        m = re.match(r"((?:SA|S|ST|QF)\d+): ", text)
        rule = f"{linter}/{m.group(1)}" if m else linter
        sev = golangci_severity(linter, m.group(1) if m else "")
        out.append(Finding("golangci-lint", rule, pos.get("Filename", ""), pos.get("Line", 0), sev,
                           text.splitlines()[0] if text else linter, root=root))
    return out


# ---- govulncheck -json: a stream of JSON objects; "finding" ones matter ----

def parse_govulncheck(text):
    """One finding per vulnerability: CRITICAL when a trace reaches a function
    (ELMOS calls the vulnerable code), INFO when only a module is affected."""
    dec, i, by_osv = json.JSONDecoder(), 0, {}
    while True:
        while i < len(text) and text[i].isspace():
            i += 1
        if i >= len(text):
            break
        obj, i = dec.raw_decode(text, i)
        f = obj.get("finding")
        if not f:
            continue
        called = any(fr.get("function") for fr in f.get("trace", []))
        top = (f.get("trace") or [{}])[0]
        prev = by_osv.get(f["osv"])
        if prev is None or (called and not prev[0]):
            by_osv[f["osv"]] = (called, top, f.get("fixed_version", ""))
    out = []
    for osv, (called, top, fixed) in sorted(by_osv.items()):
        where = ".".join(x for x in (top.get("package"), top.get("function")) if x) or top.get("module", "")
        msg = f"{osv} in {where} ({top.get('module', '')}@{top.get('version', '')}, fixed in {fixed or 'none'})"
        out.append(Finding("govulncheck", osv, "go.mod", 0, "CRITICAL" if called else "INFO", msg,
                           code=osv))
    return out


# ---- checkpatch.pl --terse --show-types --showfile: "file:line: LEVEL:TYPE: message" ----

CHECKPATCH_LINE = re.compile(r"^(.+?):(\d+): (ERROR|WARNING|CHECK):([A-Z0-9_]+): (.*)$")
CHECKPATCH = {"ERROR": "MAJOR", "WARNING": "MINOR", "CHECK": "INFO"}


def parse_checkpatch(text, root="."):
    out = []
    for line in text.splitlines():
        m = CHECKPATCH_LINE.match(line.strip())
        if m:
            path, ln, level, kind, msg = m.groups()
            out.append(Finding("checkpatch", kind, path, int(ln), CHECKPATCH[level], msg, root=root))
    return out


# ---- sanitizers: AddressSanitizer and UBSan reports (log files or output) ----

ASAN_HEAD = re.compile(r"ERROR: AddressSanitizer: ([\w-]+)")
ASAN_FRAME = re.compile(r"#\d+ 0x[0-9a-f]+ in (\S+) (\S+?):(\d+)(?::\d+)?$")
UBSAN_LINE = re.compile(r"^(\S+?):(\d+):\d+: runtime error: (.*)$")


def _in_tree(path, root, resolve=None):
    path = os.path.relpath(path, root) if os.path.isabs(path) else path
    if path.startswith(".."):
        return None
    return resolve(path) if resolve else path


def parse_sanitizers(text, root=".", resolve=None):
    """One CRITICAL per report, placed at its first stack frame inside the
    repository (AddressSanitizer) or at the reported line (UBSan). resolve
    maps a path as the symbolizer printed it (macOS's prints a base name)
    to the repository path."""
    out, lines = [], text.splitlines()
    for i, line in enumerate(lines):
        m = ASAN_HEAD.search(line)
        if m:
            where = ("", 0)
            for fr in lines[i + 1:i + 60]:
                f = ASAN_FRAME.search(fr.strip())
                if f and _in_tree(f.group(2), root):
                    where = (_in_tree(f.group(2), root, resolve), int(f.group(3)))
                    break
            out.append(Finding("sanitizer", f"asan/{m.group(1)}", where[0], where[1], "CRITICAL",
                               line.split("ERROR: ", 1)[1][:200], root=root))
            continue
        m = UBSAN_LINE.match(line.strip())
        if m and _in_tree(m.group(1), root):
            out.append(Finding("sanitizer", "ubsan", _in_tree(m.group(1), root, resolve), int(m.group(2)), "CRITICAL",
                               m.group(3)[:200], root=root))
    return out


# ---- go test -race: data races and failing tests ----

GO_FRAME = re.compile(r"^\s+(\S+\.go):(\d+)(?: \+0x[0-9a-f]+)?$")


def parse_go_test(text, tool, root="."):
    out, lines = [], text.splitlines()
    for i, line in enumerate(lines):
        if line.startswith("WARNING: DATA RACE"):
            where = ("", 0)
            for fr in lines[i + 1:i + 40]:
                f = GO_FRAME.match(fr)
                if f and _in_tree(f.group(1), root):
                    where = (_in_tree(f.group(1), root), int(f.group(2)))
                    break
            out.append(Finding(tool, "data-race", where[0], where[1], "CRITICAL", "data race", root=root))
        m = re.match(r"^\s*--- FAIL: (\S+)", line)
        if m:
            out.append(Finding(tool, "test-failed", "", 0, "MAJOR", m.group(1), code=m.group(1)))
    return out


# ---- go test -fuzz: a failing input ----

def parse_go_fuzz(text, target, package="."):
    """The failing input's path is relative to the package; the finding's is
    relative to the repository."""
    m = re.search(r"Failing input written to (\S+)", text)
    if not m and "--- FAIL" not in text:
        return []
    why = next((ln.strip() for ln in text.splitlines() if ln.strip().startswith(("panic:", "Error", "fuzz_"))), "")
    path = os.path.normpath(os.path.join(package, m.group(1))) if m else ""
    return [Finding("go-fuzz", target, path, 0, "CRITICAL", (why or "fuzz target failed")[:200], code=target)]

