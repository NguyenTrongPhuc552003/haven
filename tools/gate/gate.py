#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""The quality gate (docs/quality/gate.md).

Usage, from a repository's root (through its scripts/gate):
  gate pre-commit          staged files; blocks on BLOCKER
  gate pre-push            files changed against main; blocks on BLOCKER,
                           new CRITICAL, new MAJOR
  gate ci                  whole tree, same policy as pre-push
  gate deep                whole tree with the slow tools; reports only
  gate baseline [--check]  write quality/baseline.json from the whole tree
                           (--check: report how it would change, write nothing)
  gate doctor              list the tools found and missing
  gate install             point git at the repository's hooks
Options: --root DIR (default: current directory), --json FILE (write the report),
  --remote NAME (pre-push: compare with NAME/main; the hook passes the remote).
Standard library only; Python 3.9 or later.
"""
import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import findings as fd  # noqa: E402
import tools  # noqa: E402

CONFIG = "quality/gate.json"
BASELINE = "quality/baseline.json"
STAGES = ("pre-commit", "pre-push", "ci", "deep")


class Ctx:
    def __init__(self, root, cfg, stage):
        self.root, self.cfg, self.stage = root, cfg, stage


def git(root, *args):
    return subprocess.run(["git", "-C", root] + list(args), capture_output=True, text=True).stdout


def tracked(root, cfg):
    excl = tuple(cfg.get("exclude", []))
    return [f for f in git(root, "ls-files").splitlines() if f and not f.startswith(excl)]


def base_ref(root, remote=None):
    """main on the remote being pushed to; without one, the newest main among
    the remotes (a clone may keep an old remote whose main is far behind)."""
    if remote and git(root, "rev-parse", "--verify", "--quiet", f"{remote}/main").strip():
        return f"{remote}/main"
    refs = git(root, "for-each-ref", "--format=%(refname:short)", "refs/remotes/*/main").split()
    if not refs:
        return "main" if git(root, "rev-parse", "--verify", "--quiet", "main").strip() else None
    return max(refs, key=lambda r: int(git(root, "log", "-1", "--format=%ct", r).strip() or 0))


def files_for(root, cfg, stage, remote=None):
    if stage == "pre-commit":
        names = git(root, "diff", "--cached", "--name-only", "--diff-filter=ACM").splitlines()
    elif stage == "pre-push":
        ref = base_ref(root, remote)
        mb = git(root, "merge-base", "HEAD", ref).strip() if ref else ""
        names = git(root, "diff", "--name-only", "--diff-filter=ACM", f"{mb}...HEAD").splitlines() if mb else []
    else:
        return tracked(root, cfg)
    excl = tuple(cfg.get("exclude", []))
    return [n for n in names if n and not n.startswith(excl) and os.path.isfile(os.path.join(root, n))]


def in_paths(files, paths):
    return [f for f in files if any(f == p or f.startswith(p.rstrip("/") + "/") for p in paths)]


def collect(ctx, files):
    out = []
    kinds = {"cppcheck": "c_paths", "clang-tidy": "c_paths", "lizard": "c_paths",
             "ruff": "py_paths", "shellcheck": "sh_paths"}
    for name in ctx.cfg["stages"][ctx.stage if ctx.stage != "baseline" else "ci"]:
        sel = in_paths(files, ctx.cfg.get(kinds[name], [])) if name in kinds else files
        out += tools.version_check(ctx, name)
        out += tools.RUNNERS[name](ctx, sel)
    return out


def report(found, block, baseline, stage):
    counts = {lv: 0 for lv in fd.LEVELS}
    new = 0
    for f in found:
        counts[f.severity] += 1
        if f.severity in ("CRITICAL", "MAJOR") and f.key not in baseline:
            new += 1
    for f in found:
        if f.severity == "INFO":
            print(f"INFO     {f.tool}/{f.rule}: {f.message}")
    for f in block:
        print(f)
    summary = ", ".join(f"{lv} {counts[lv]}" for lv in reversed(fd.LEVELS))
    print(f"gate {stage}: {summary}; new CRITICAL or MAJOR {new}; blocking {len(block)}")
    print(f"gate {stage}: {'FAIL' if block else 'PASS'}")


def main(argv):
    root, out_json, remote = ".", None, None
    args = []
    it = iter(argv)
    for a in it:
        if a == "--root":
            root = next(it)
        elif a == "--json":
            out_json = next(it)
        elif a == "--remote":
            remote = next(it)
        else:
            args.append(a)
    if not args:
        print(__doc__)
        return 2
    cmd = args[0]
    root = os.path.abspath(root)
    if cmd == "install":
        hooks = ".githooks"
        subprocess.run(["git", "-C", root, "config", "core.hooksPath", hooks], check=True)
        print(f"gate: core.hooksPath set to {hooks}")
        return 0
    with open(os.path.join(root, CONFIG)) as f:
        cfg = json.load(f)
    if cmd == "doctor":
        names = sorted({t for s in cfg["stages"].values() for t in s} - {"scan", "tcb"})
        for t in names:
            ctx = Ctx(root, cfg, "doctor")
            exe = tools.which(ctx, t)
            bad = tools.version_check(ctx, t)
            print(f"{t:12s} {exe or 'MISSING'}{'  (' + bad[0].message + ')' if bad else ''}")
        return 0
    if cmd == "baseline":
        ctx = Ctx(root, cfg, "baseline")
        found = collect(ctx, tracked(root, cfg))
        old = fd.load_baseline(os.path.join(root, BASELINE))
        keys = {f.key for f in found if f.severity not in ("BLOCKER", "INFO")}
        blockers = [f for f in found if f.severity == "BLOCKER"]
        for f in blockers:
            print(f)
        print(f"gate baseline: {len(keys)} findings ({len(keys - set(old))} not in the current baseline, "
              f"{len(set(old) - keys)} fixed); BLOCKER {len(blockers)} (never accepted into a baseline)")
        if "--check" not in args:
            n = fd.save_baseline(os.path.join(root, BASELINE), found)
            print(f"gate baseline: wrote {BASELINE} ({n} findings)")
        if out_json:
            with open(out_json, "w") as f:
                json.dump([dict(x.as_dict(), line=x.line) for x in found], f, indent=1)
        return 1 if blockers else 0
    if cmd not in STAGES:
        print(__doc__)
        return 2
    ctx = Ctx(root, cfg, cmd)
    baseline = fd.load_baseline(os.path.join(root, BASELINE))
    found = collect(ctx, files_for(root, cfg, cmd, remote))
    block = fd.blocking(found, baseline, cmd)
    report(found, block, baseline, cmd)
    if out_json:
        with open(out_json, "w") as f:
            json.dump({"stage": cmd, "findings": [dict(x.as_dict(), line=x.line, new=x.key not in baseline)
                                                  for x in found],
                       "blocking": [x.key for x in block]}, f, indent=1)
    return 1 if block else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
