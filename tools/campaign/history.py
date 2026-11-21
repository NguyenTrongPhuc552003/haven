#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Historical faults: HAVEN's own fixes of enforcement code, reverted.

docs/methodology/stage2/20261119-evaluation-campaign/c3/README.md, "Owner
decision (2026-11-22)". The rule, fixed before extraction: every
non-merge commit up to UPTO whose subject starts with "fix:" and that
changes the enforcement files (mutate.py's FILES); commits changing more
than MAX_LINES lines there are rewrites and left out; each case is the
reverse of the commit's diff, limited to those files, applied to HEAD
(git apply, then git apply -3 in a scratch clone); one that applies
neither way is "code moved". Writes OUT/H<n>.patch (oldest commit first)
and OUT/history.json.

Usage: history.py REPO OUT [UPTO]
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from mutate import FILES  # noqa: E402

MAX_LINES = 100


def git(repo, *args, check=True):
    return subprocess.run(["git", "-C", repo, *args], capture_output=True, text=True, check=check).stdout


def candidates(repo, upto):
    """(sha, subject, changed lines) of the rule's commits, oldest first."""
    out = []
    for line in git(repo, "log", "--no-merges", "--reverse", "--format=%H%x09%s", upto, "--", *FILES).splitlines():
        sha, subject = line.split("\t", 1)
        if not subject.startswith("fix:"):
            continue
        lines = sum(int(a) + int(b) for a, b, *_ in (x.split("\t") for x in
                    git(repo, "diff", "--numstat", f"{sha}^", sha, "--", *FILES).splitlines()) if a != "-")
        out.append((sha, subject, lines))
    return out


def reverse_on_head(repo, sha, scratch):
    """The revert of SHA (enforcement files only) as a patch on HEAD, or None."""
    rev = git(repo, "diff", sha, f"{sha}^", "--", *FILES)
    path = os.path.join(scratch, "rev.patch")
    with open(path, "w") as f:
        f.write(rev)
    work = os.path.join(scratch, "clone")
    if not os.path.isdir(work):
        subprocess.run(["git", "clone", "-q", "--shared", repo, work], check=True)
    git(work, "checkout", "-q", "--detach", git(repo, "rev-parse", "HEAD").strip())
    git(work, "reset", "-q", "--hard")
    for mode in ([], ["-3"]):
        if subprocess.run(["git", "-C", work, "apply", *mode, path], capture_output=True).returncode == 0:
            if mode and git(work, "diff", "--name-only", "--diff-filter=U", check=False).strip():
                break  # a 3-way apply with conflicts is not a clean revert
            patch = git(work, "diff", "HEAD")
            git(work, "reset", "-q", "--hard")
            return patch, ("apply" if not mode else "apply -3")
        git(work, "reset", "-q", "--hard")
    return None, None


def main(argv):
    if len(argv) not in (2, 3):
        sys.exit(__doc__)
    repo, out = os.path.abspath(argv[0]), os.path.abspath(argv[1])
    upto = argv[2] if len(argv) == 3 else "HEAD"
    os.makedirs(out, exist_ok=True)
    result = {"rule": "c3/README.md, owner decision 2026-11-22", "upto": git(repo, "rev-parse", upto).strip(),
              "head": git(repo, "rev-parse", "HEAD").strip(), "max_lines": MAX_LINES, "commits": []}
    scratch = tempfile.mkdtemp(prefix="history-", dir=out)
    try:
        n = 0
        for sha, subject, lines in candidates(repo, upto):
            e = {"commit": sha, "subject": subject, "lines": lines}
            if lines > MAX_LINES:
                e["status"] = "rewrite (left out)"
            else:
                patch, how = reverse_on_head(repo, sha, scratch)
                if patch is None:
                    e["status"] = "code moved"
                else:
                    n += 1
                    e["status"], e["name"], e["applied_with"] = "kept", f"H{n}", how
                    with open(os.path.join(out, f"H{n}.patch"), "w") as f:
                        f.write(patch)
            result["commits"].append(e)
    finally:
        shutil.rmtree(scratch, ignore_errors=True)
    with open(os.path.join(out, "history.json"), "w") as f:
        json.dump(result, f, indent=1)
        f.write("\n")
    for e in result["commits"]:
        print(f"{e['commit'][:7]} {e['lines']:4} {e.get('name', '-'):4} {e['status']}: {e['subject'][:70]}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
