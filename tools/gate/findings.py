# SPDX-License-Identifier: Apache-2.0
"""Findings, their identity, the baseline and the stage policy.

A finding's identity (key) is the SHA-256 of tool, rule, file and the
whitespace-normalised source it points at. Line numbers are not part of it,
so code that moves keeps its key. Identical lines in one file share a key,
so the baseline keeps a count per key. See docs/quality/gate.md.
"""
import collections
import hashlib
import json
import os

LEVELS = ("INFO", "MINOR", "MAJOR", "CRITICAL", "BLOCKER")
RANK = {name: i for i, name in enumerate(LEVELS)}
BASELINE_SCHEMA = "haven-gate-baseline/2"
READ_SCHEMAS = ("haven-gate-baseline/1", BASELINE_SCHEMA)  # /1 had no counts: 1 per key

_lines = {}


def source_line(root, path, line):
    """The source text a finding points at ('' when unknown)."""
    if not line:
        return ""
    if path not in _lines:
        try:
            with open(os.path.join(root, path), errors="replace") as f:
                _lines[path] = f.read().splitlines()
        except OSError:
            _lines[path] = []
    text = _lines[path]
    return text[line - 1] if 0 < line <= len(text) else ""


def identity(tool, rule, path, code):
    norm = " ".join(code.split())
    return hashlib.sha256("\x1f".join((tool, rule, path, norm)).encode()).hexdigest()[:24]


class Finding:
    __slots__ = ("tool", "rule", "file", "line", "severity", "message", "key")

    def __init__(self, tool, rule, path, line, severity, message, code=None, root="."):
        assert severity in RANK, severity
        self.tool, self.rule, self.file, self.line = tool, rule, path, line
        self.severity, self.message = severity, message
        if code is None:
            code = source_line(root, path, line)
        self.key = identity(tool, rule, path, code)

    def as_dict(self):
        return {"key": self.key, "tool": self.tool, "rule": self.rule, "file": self.file,
                "severity": self.severity, "message": self.message}

    def __str__(self):
        where = f" {self.file}:{self.line}" if self.line else (f" {self.file}" if self.file else "")
        return f"{self.severity:8s} {self.tool}/{self.rule}{where}: {self.message}"


def load_baseline(path):
    """{key: accepted count}."""
    if not os.path.exists(path):
        return {}
    with open(path) as f:
        data = json.load(f)
    if data.get("schema") not in READ_SCHEMAS:
        raise SystemExit(f"gate: {path}: unknown schema {data.get('schema')!r}")
    return {e["key"]: e.get("count", 1) for e in data["findings"]}


def accepted(f):
    """What a baseline may hold: not BLOCKER (never accepted), not INFO (not a
    defect), not the gate's own reports (a missing or failed tool)."""
    return f.severity not in ("BLOCKER", "INFO") and f.tool != "gate"


def counts(findings):
    return collections.Counter(f.key for f in findings if accepted(f))


def save_baseline(path, findings):
    n = counts(findings)
    keep = {f.key: dict(f.as_dict(), count=n[f.key]) for f in findings if accepted(f)}
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "w") as f:
        json.dump({"schema": BASELINE_SCHEMA, "findings": [keep[k] for k in sorted(keep)]},
                  f, indent=1)
        f.write("\n")
    return len(keep), sum(n.values())


def new_keys(findings, baseline):
    """CRITICAL and MAJOR keys found more often than the baseline accepts."""
    n = collections.Counter(f.key for f in findings if f.severity in ("CRITICAL", "MAJOR"))
    return {k for k, c in n.items() if c > baseline.get(k, 0)}


def blocking(findings, baseline, stage):
    """The findings that stop this stage: BLOCKER always; from pre-push on,
    every instance of a CRITICAL or MAJOR key found more often than the
    baseline accepts; deep only reports."""
    if stage == "deep":
        return []
    new = new_keys(findings, baseline) if stage in ("pre-push", "ci") else set()
    return [f for f in findings if f.severity == "BLOCKER" or f.key in new]
