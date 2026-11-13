# SPDX-License-Identifier: Apache-2.0
"""Runners: each takes the context and the files of its kind and returns
findings, using the parsers in rules.py. A tool that is not installed, or
that fails without reporting anything, gives one finding of its own (see
missing() and checked())."""
import json
import os
import shutil
import subprocess
import tempfile

import rules
from findings import Finding

C_EXT = (".c", ".h")
PY_EXT = (".py",)
SH_EXT = (".sh",)


def run(cmd, cwd, env=None):
    p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, env=env)
    return p.returncode, p.stdout, p.stderr


def which(ctx, name):
    """A tool from PATH or from the configured extra directories."""
    for d in ctx.cfg.get("tool_dirs", []):
        p = os.path.join(os.path.expanduser(d), name)
        if os.access(p, os.X_OK):
            return p
    return shutil.which(name)


VERSION_ARG = {"gitleaks": ["version"]}


def version_check(ctx, tool):
    """A finding when the installed tool is not the pinned version (MAJOR in
    CI, where the pins are installed; INFO elsewhere): other versions report
    other findings, so one baseline needs one set of versions."""
    want = ctx.cfg.get("versions", {}).get(tool)
    exe = which(ctx, tool)
    if not want or not exe:
        return []
    _, so, se = run([exe] + VERSION_ARG.get(tool, ["--version"]), ctx.root)
    if want in so + se:
        return []
    got = (so + se).strip().splitlines()[0] if (so + se).strip() else "?"
    sev = "MAJOR" if ctx.stage == "ci" else "INFO"
    return [Finding("gate", "tool-version", "", 0, sev, f"{tool}: pinned {want}, found {got}", code=tool)]


def missing(ctx, tool):
    sev = "MAJOR" if ctx.stage == "ci" else "INFO"
    return [Finding("gate", "missing-tool", "", 0, sev, f"{tool} is not installed; its checks did not run",
                    code=tool)]


def failed(tool, why):
    """MAJOR in every stage: the tool is installed but its checks did not run,
    so a stage must not pass as if they had."""
    return [Finding("gate", "tool-failed", "", 0, "MAJOR", f"{tool}: {why.strip()[-300:]}", code=tool)]


def checked(tool, code, err, found):
    """The parsed findings, or a tool-failed finding when the tool exited with
    an error and reported nothing (a crash or a bad option looks like a clean
    run otherwise)."""
    return failed(tool, f"exit {code}: {err}") if code and not found else found


def scan(ctx, files):
    names = [n for n in os.environ.get("GATE_PRIVATE_NAMES", "").split(",") if n]
    out = []
    excl = tuple(ctx.cfg.get("scan_exclude", []))
    for f in files:
        if f.startswith(excl):
            continue
        try:
            with open(os.path.join(ctx.root, f), "rb") as fh:
                data = fh.read()
        except OSError:
            continue
        if b"\0" in data[:8192]:
            continue
        out += rules.scan_text(f, data.decode("utf-8", "replace"), names)
    return out


def cppcheck(ctx, files):
    files = [f for f in files if f.endswith(".c")]
    if not files:
        return []
    exe = which(ctx, "cppcheck")
    if not exe:
        return missing(ctx, "cppcheck")
    inc = sum((["-I", d] for d in ctx.cfg.get("c_include", [])), [])
    code, _, err = run([exe, "--enable=warning,performance,portability", "--inline-suppr", "--quiet",
                     "--suppress=missingIncludeSystem", "--suppress=missingInclude",
                     "--template={file}\t{line}\t{severity}\t{id}\t{message}"] + inc + files, ctx.root)
    return checked("cppcheck", code, err, rules.parse_cppcheck(err, ctx.root))


def clang_tidy(ctx, files):
    files = [f for f in files if f.endswith(".c")]
    conf = ctx.cfg.get("clang_tidy")
    if not files or not conf:
        return []
    exe = which(ctx, "clang-tidy")
    if not exe:
        return missing(ctx, "clang-tidy")
    build = os.path.join(ctx.root, conf["build"])
    if not os.path.exists(os.path.join(build, "compile_commands.json")):
        code, _, err = run(conf["configure"], ctx.root)
        if code:
            return [Finding("gate", "configure-failed", "", 0, "MAJOR", err.strip()[-300:], code="configure")]
    with open(os.path.join(build, "compile_commands.json")) as f:
        known = {os.path.relpath(e["file"], ctx.root) if os.path.isabs(e["file"]) else e["file"]
                 for e in json.load(f)}
    files = [f for f in files if f in known]
    if not files:
        return []
    extra = sum((["--extra-arg=" + a] for a in conf.get("extra_args", [])), [])
    code, so, err = run([exe, "-p", build, "--quiet", f"--checks={conf['checks']}",
                    f"--header-filter={conf.get('header_filter', '.*')}"] + extra + files, ctx.root)
    rel = [line.replace(ctx.root.rstrip("/") + "/", "") for line in so.splitlines()]
    return checked("clang-tidy", code, err, rules.parse_clang_tidy("\n".join(rel), ctx.root))


def lizard(ctx, files):
    files = [f for f in files if f.endswith(C_EXT)]
    if not files:
        return []
    exe = which(ctx, "lizard")
    if not exe:
        return missing(ctx, "lizard")
    lim = ctx.cfg.get("limits", {})
    code, so, err = run([exe, "--csv", "-l", "c"] + files, ctx.root)
    return checked("lizard", code, err, rules.parse_lizard(so, lim.get("ccn", 15), lim.get("length", 100)))


def jscpd(ctx, files):
    exe = which(ctx, "jscpd")
    if not exe:
        return missing(ctx, "jscpd")
    lim = ctx.cfg.get("limits", {})
    paths = [p for p in ctx.cfg.get("c_paths", []) + ctx.cfg.get("py_paths", [])
             if os.path.exists(os.path.join(ctx.root, p))]
    with tempfile.TemporaryDirectory() as out:
        code, _, err = run([exe, "--silent", "--absolute", "--min-lines", str(lim.get("dup_lines", 15)), "--reporters", "json",
             "--output", out, "--format", ctx.cfg.get("jscpd_formats", "c,python")] +
            sum((["--ignore", g] for g in ctx.cfg.get("jscpd_ignore", [])), []) + paths, ctx.root)
        report = os.path.join(out, "jscpd-report.json")
        if not os.path.exists(report):
            return failed("jscpd", f"exit {code}, no report: {err}")
        with open(report) as f:
            data = json.load(f)
    # jscpd names files relative to the path it was given, which loses the
    # top directory when several are given: report absolute paths, then make
    # them relative to the repository root
    for d in data.get("duplicates", []):
        for side in ("firstFile", "secondFile"):
            d[side]["name"] = os.path.relpath(d[side]["name"], ctx.root)
    only = None if ctx.stage in ("ci", "deep", "baseline") else set(files)
    return rules.parse_jscpd(data, only)


def ruff(ctx, files):
    files = [f for f in files if f.endswith(PY_EXT)]
    if not files:
        return []
    exe = which(ctx, "ruff")
    if not exe:
        return missing(ctx, "ruff")
    code, so, err = run([exe, "check", "--no-cache", "--output-format", "json", "--exit-zero"] + files, ctx.root)
    if code:
        return failed("ruff", f"exit {code}: {err}")
    return rules.parse_ruff(json.loads(so or "[]"), ctx.root)


def shellcheck(ctx, files):
    files = [f for f in files if f.endswith(SH_EXT) or f in ctx.cfg.get("sh_files", [])]
    if not files:
        return []
    exe = which(ctx, "shellcheck")
    if not exe:
        return missing(ctx, "shellcheck")
    # shellcheck exits 1 when it reports findings, 2 and above when it fails
    code, so, err = run([exe, "-f", "json1", "-x"] + files, ctx.root)
    if code > 1:
        return failed("shellcheck", f"exit {code}: {err}")
    return rules.parse_shellcheck(json.loads(so or "{}"), ctx.root)


def gitleaks(ctx, files):
    exe = which(ctx, "gitleaks")
    if not exe:
        return missing(ctx, "gitleaks")
    with tempfile.TemporaryDirectory() as out:
        rep = os.path.join(out, "r.json")
        conf = [] if not os.path.exists(os.path.join(ctx.root, ".gitleaks.toml")) else ["-c", ".gitleaks.toml"]
        code, _, err = run([exe, "dir", "--no-banner", "--exit-code", "0", "-f", "json", "-r", rep] + conf + ["."],
                           ctx.root)
        if code or not os.path.exists(rep):
            return failed("gitleaks", f"exit {code}: {err}")
        with open(rep) as f:
            return rules.parse_gitleaks(json.load(f))


def tcb(ctx, files):
    conf = ctx.cfg.get("tcb")
    if not conf:
        return []
    build = os.path.join(ctx.root, conf["build"])
    if not os.path.exists(os.path.join(build, "build.ninja")):
        run(conf["configure"], ctx.root)
    code, _, err = run(["cmake", "--build", build], ctx.root)
    if code:
        return [Finding("gate", "build-failed", "", 0, "BLOCKER", err.strip()[-300:], code="build")]
    _, so, _ = run(["python3", conf["tool"], build], ctx.root)
    last = so.strip().splitlines()[-1] if so.strip() else "no count"
    return [Finding("tcb", "trusted-code", "", 0, "INFO", last, code="tcb")]


RUNNERS = {"scan": scan, "cppcheck": cppcheck, "clang-tidy": clang_tidy, "lizard": lizard,
           "jscpd": jscpd, "ruff": ruff, "shellcheck": shellcheck, "gitleaks": gitleaks, "tcb": tcb}
