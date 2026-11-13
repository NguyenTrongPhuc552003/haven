# SPDX-License-Identifier: Apache-2.0
"""Findings, their identity, the baseline and the stage policy.

A finding's identity (key) is the SHA-256 of tool, rule, file and the
whitespace-normalised source it points at. Line numbers are not part of it,
so code that moves keeps its key. See docs/quality/gate.md.
"""
import hashlib
import json
import os

LEVELS = ("INFO", "MINOR", "MAJOR", "CRITICAL", "BLOCKER")
RANK = {name: i for i, name in enumerate(LEVELS)}
BASELINE_SCHEMA = "haven-gate-baseline/1"

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
        where = f"{self.file}:{self.line}" if self.line else self.file
        return f"{self.severity:8s} {self.tool}/{self.rule} {where}: {self.message}"


def load_baseline(path):
    if not os.path.exists(path):
        return {}
    with open(path) as f:
        data = json.load(f)
    if data.get("schema") != BASELINE_SCHEMA:
        raise SystemExit(f"gate: {path}: unknown schema {data.get('schema')!r}")
    return {e["key"]: e for e in data["findings"]}


def save_baseline(path, findings):
    """Every finding except BLOCKER (never accepted) and INFO (not a defect)."""
    keep = {f.key: f.as_dict() for f in findings if f.severity not in ("BLOCKER", "INFO")}
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "w") as f:
        json.dump({"schema": BASELINE_SCHEMA, "findings": [keep[k] for k in sorted(keep)]},
                  f, indent=1)
        f.write("\n")
    return len(keep)


def blocking(findings, baseline, stage):
    """The findings that stop this stage: BLOCKER always; new CRITICAL and new
    MAJOR from pre-push on; deep only reports."""
    if stage == "deep":
        return []
    out = []
    for f in findings:
        if f.severity == "BLOCKER":
            out.append(f)
        elif stage in ("pre-push", "ci") and f.severity in ("CRITICAL", "MAJOR") and f.key not in baseline:
            out.append(f)
    return out
