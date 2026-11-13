# SPDX-License-Identifier: Apache-2.0
"""Severity mapping: one parser per tool, from the tool's output text to
findings. Pure functions over text, so the tests feed recorded outputs.
The scale is BLOCKER, CRITICAL, MAJOR, MINOR, INFO (docs/quality/gate.md)."""
import csv
import io
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
