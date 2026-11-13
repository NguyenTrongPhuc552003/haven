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
import collections
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


def tool_paths(cfg, name):
    """The paths a tool reads: the profile's "tool_paths" entry, else its
    languages' paths; None means every file of the stage."""
    if name in cfg.get("tool_paths", {}):
        return cfg["tool_paths"][name]
    keys = tools.PATH_KEYS.get(name)
    return None if keys is None else [p for k in keys for p in cfg.get(k, [])]


def collect(ctx, files):
    out, checked = [], set()
    for name in ctx.cfg["stages"][ctx.stage if ctx.stage != "baseline" else "ci"]:
        paths = tool_paths(ctx.cfg, name)
        sel = files if paths is None else in_paths(files, paths)
        for t in [name] + tools.NEEDS.get(name, []):
            if t not in checked:
                checked.add(t)
                out += tools.version_check(ctx, t)
        out += tools.RUNNERS[name](ctx, sel)
    return out


def report(found, block, new_keys, stage):
    counts = {lv: 0 for lv in fd.LEVELS}
    for f in found:
        counts[f.severity] += 1
    new = sum(1 for f in found if f.key in new_keys)
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
        old = collections.Counter(fd.load_baseline(os.path.join(root, BASELINE)))
        now = fd.counts(found)
        blockers = [f for f in found if f.severity == "BLOCKER"]
        for f in blockers + [f for f in found if f.tool == "gate" and f.severity != "INFO"]:
            print(f)
        print(f"gate baseline: {sum(now.values())} findings, {len(now)} keys "
              f"({sum((now - old).values())} not in the current baseline, "
              f"{sum((old - now).values())} fixed); BLOCKER {len(blockers)} (never accepted into a baseline)")
        if "--check" not in args:
            keys, n = fd.save_baseline(os.path.join(root, BASELINE), found)
            print(f"gate baseline: wrote {BASELINE} ({n} findings, {keys} keys)")
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
    new = fd.new_keys(found, baseline)
    report(found, block, new, cmd)
    if out_json:
        with open(out_json, "w") as f:
            json.dump({"stage": cmd, "findings": [dict(x.as_dict(), line=x.line, new=x.key in new)
                                                  for x in found],
                       "blocking": [x.key for x in block]}, f, indent=1)
    return 1 if block else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
