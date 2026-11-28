#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Whether a pull request changes only documentation that nothing runs.

Usage: docs_only.py BASE HEAD   (prints code=true or code=false)

The ci workflow skips its build, boot and host-test jobs when this prints
code=false: every changed file is Markdown, and no script, test, tool,
workflow or CMake file names it as a quoted file name (code reads a
Markdown file only that way; test_cix_layout.py, for example, parses
"cix-p1-loading.md"). A bare "README.md" names only the top-level one.
Anything else, deleted or renamed files included, prints code=true.
"""
import subprocess
import sys

SEARCHED = ["scripts", "tests", "tools", ".github", "cmake", "CMakeLists.txt", "CMakePresets.json",
            ":(exclude)scripts/ci/test_docs_only.py"]  # its examples are data, not reads


def changed(base, head):
    out = subprocess.run(["git", "diff", "--name-only", f"{base}...{head}"],
                         capture_output=True, text=True, check=True).stdout
    return [line for line in out.splitlines() if line]


def literals():
    """Every quoted *.md file name in the searched files, without quotes."""
    out = subprocess.run(["git", "grep", "-h", "-o", "-E", r"[A-Za-z0-9_./-]*\.md[\"']", "--"] + SEARCHED,
                         capture_output=True, text=True).stdout
    return {line[:-1].lstrip("./") for line in out.splitlines() if line[:-1].lstrip("./")}


def named(path, names):
    base = path.rsplit("/", 1)[-1]
    for n in names:
        if "/" in n:
            if path.endswith(n) or n.endswith(path):
                return True
        elif n == base and (base != "README.md" or "/" not in path):
            return True
    return False


def named_by_code(path, _names=[]):
    if not _names:
        _names.append(literals())
    return named(path, _names[0])


def code_changed(files, named=named_by_code):
    if not files:
        return True
    return any(not f.endswith(".md") or named(f) for f in files)


def main(argv):
    if len(argv) != 2:
        sys.exit(__doc__)
    print(f"code={'true' if code_changed(changed(argv[0], argv[1])) else 'false'}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
