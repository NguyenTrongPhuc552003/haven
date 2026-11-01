#!/usr/bin/env python3
"""Run the frozen A1 case set (CASES.md) and score it.

Usage: run_cases.py <haven-repo> <nm> <out-dir> [case ...]
For each case: export HEAD into a scratch copy, apply the seeded change (it
must match exactly once), build host tests and the qemu-direct image, then run
the three checks on cortex-a72 and max: read-back (frozen readback.py), host
unit tests (ctest, as in CI) and the QEMU boot check. Writes per-case logs and
results.md. The repository itself is never modified.
"""
import json
import os
import re
import subprocess
import sys
import tempfile

A1 = "docs/methodology/stage1/20261101-a1-readback"
MM = "arch/arm64/mm.c"
S2 = "arch/arm64/include/asm/s2_attrs.h"
PG = "arch/arm64/include/asm/page.h"
# anchored on cur_pa so it hits hv_arch_stage2_map, not the unmap path
END = "\tuint64_t cur_pa = pa;\n\tuint64_t end_ipa = ipa + size;\n"
BLK = "\t\t\t(*l2)[l2_idx] = (cur_pa & HV_BLOCK_MASK_L2) |\n\t\t\t\t\tS2_DESC_BLOCK | hw_attrs | S2_AF;\n"

# (file, old, new); old must occur exactly once
CASES = {
    "D1": [(MM, BLK, BLK.replace("(cur_pa & HV_BLOCK_MASK_L2)", "((cur_pa + HV_BLOCK_SIZE_L2) & HV_BLOCK_MASK_L2)"))],
    "D2": [(MM, END, END.replace("ipa + size;", "ipa + size + ((size >= HV_BLOCK_SIZE_L2) ? HV_BLOCK_SIZE_L2 : 0);"))],
    "D3": [(S2, "\tif ((attrs & HV_MEM_DEVICE) != 0U) {\n", "\tif (((attrs & HV_MEM_DEVICE) != 0U) && 0) {\n")],
    "D4": [(MM, END, END.replace("ipa + size;", "ipa + size - ((size >= HV_BLOCK_SIZE_L2) ? HV_BLOCK_SIZE_L2 : 0);"))],
    "D5": [(S2, "\thw |= ((attrs & HV_MEM_W) != 0U) ?",
            "\thw |= (((attrs & HV_MEM_W) != 0U) && ((attrs & HV_MEM_DEVICE) != 0U)) ?")],
    "D6": [(MM, BLK, BLK.replace("S2_DESC_BLOCK | hw_attrs | S2_AF;", "S2_DESC_BLOCK | hw_attrs;"))],
    "D7": [(PG, "#define S2_XN_NEVER (2UL << S2_XN_SHIFT)", "#define S2_XN_NEVER (3UL << S2_XN_SHIFT)")],
    "C1": [],
    "C2": [(S2, "re:hw \\|= \\(\\(attrs & HV_MEM_W\\) != 0U\\) \\? \\(uint64_t\\)S2AP_RW :\\s*\\(uint64_t\\)S2AP_RO;",
            "hw |= (uint64_t)S2AP_RW;")],
    "C3": [(MM, BLK, "\t\t\tuint64_t desc = (cur_pa & HV_BLOCK_MASK_L2) | S2_DESC_BLOCK |\n"
                     "\t\t\t\t\thw_attrs | S2_AF;\n\t\t\t(*l2)[l2_idx] = desc;\n")],
}
# Expected read-back findings (approved verdicts in CASES.md), as (region, class)
EXPECT = {
    "D1": [("A RAM", "association drift"), ("*", "cross-partition alias")],
    "D2": [("*", "extent drift|extra binding")],
    "D3": [("A UART", "memory-type drift"), ("A UART", "shareability drift")],
    "D4": [("A RAM", "missing binding")],
    "D5": [("A RAM", "access drift"), ("B RAM", "access drift")],
    "D6": [("A RAM", "access-flag drift"), ("B RAM", "access-flag drift")],
    "D7": [("A UART", "execute drift")],
    "C1": [], "C2": [], "C3": [],
}
CPUS = ["cortex-a72", "max"]
ENV = dict(os.environ, PATH="/opt/homebrew/bin:/usr/local/bin:" + os.environ.get("PATH", ""))


def sh(cmd, cwd, log, timeout=900):
    with open(log, "a") as f:
        f.write(f"$ {cmd}\n")
        f.flush()
        try:
            return subprocess.run(cmd, cwd=cwd, shell=True, stdout=f, stderr=subprocess.STDOUT,
                                  env=ENV, timeout=timeout).returncode
        except subprocess.TimeoutExpired:
            f.write("TIMEOUT\n")
            return 124


def mutate(src, edits):
    for path, old, new in edits:
        p = os.path.join(src, path)
        s = open(p).read()
        if old.startswith("re:"):
            s2, n = re.subn(old[3:], new, s)
        else:
            n = s.count(old)
            s2 = s.replace(old, new)
        if n != 1:
            raise SystemExit(f"mutation does not match exactly once in {path} (found {n})")
        open(p, "w").write(s2)


def findings(stdout):
    out = []
    for line in stdout.splitlines():
        m = re.match(r"\s*DRIFT (.+?): (.+?) \(", line)
        if m:
            out.append((m.group(1), m.group(2)))
        elif "DRIFT cross-partition alias" in line:
            out.append(("*", "cross-partition alias"))
    return out


def matched(expect, got):
    ok = []
    for region, cls in expect:
        ok.append(any(re.fullmatch(cls, c) and (region == "*" or r == region) for r, c in got))
    return all(ok)


def main():
    repo, nm, out = sys.argv[1:4]
    only = sys.argv[4:] or list(CASES)
    out = os.path.abspath(out)
    readback = os.path.join(repo, A1, "readback.py")
    expected_g = os.path.join(repo, A1, "expected-G.json")
    head = subprocess.check_output(["git", "-C", repo, "rev-parse", "--short", "HEAD"], text=True).strip()
    results = {}
    for case in only:
        cdir = os.path.join(out, case)
        os.makedirs(cdir, exist_ok=True)
        log = os.path.join(cdir, "build.log")
        open(log, "w").close()
        src = tempfile.mkdtemp(prefix=f"a1-{case}-")
        subprocess.run(f"git -C {repo} archive HEAD | tar -x -C {src}", shell=True, check=True)
        mutate(src, CASES[case])
        subprocess.run(f"cd {src} && git init -q && git add -A && git -c user.name=a1 -c user.email=a1@x commit -qm {case}",
                       shell=True, check=True)
        with open(os.path.join(cdir, "mutation.diff"), "w") as f:
            for path, _, _ in CASES[case]:
                subprocess.run(f"diff -u {repo}/{path} {src}/{path}", shell=True, stdout=f)
        r = {"case": case}
        r["unit_rc"] = (sh("cmake --preset host-tests && cmake --build build-host", src, log) or
                        sh("ctest --test-dir build-host --output-on-failure --timeout 60 -E bench_temporal_isolation",
                           src, os.path.join(cdir, "ctest.log")))
        r["build_rc"] = sh("cmake --preset arm64-qemu -DHAVEN_BOOT_PROFILE=qemu-direct && cmake --build build", src, log)
        for cpu in CPUS:
            if r["build_rc"] != 0:
                r[cpu] = {"readback_rc": None, "boot_rc": None, "findings": []}
                continue
            rb_out = os.path.join(cdir, f"readback-{cpu}")
            p = subprocess.run([sys.executable, readback, os.path.join(src, "build"), nm, expected_g, rb_out, cpu],
                               capture_output=True, text=True, env=ENV, timeout=180)
            open(os.path.join(cdir, f"readback-{cpu}.txt"), "w").write(p.stdout + p.stderr)
            boot = sh(f"./scripts/qemu/qemu-direct-check.sh build {cpu}", src, os.path.join(cdir, f"boot-{cpu}.log"), 120)
            r[cpu] = {"readback_rc": p.returncode, "boot_rc": boot, "findings": findings(p.stdout)}
        results[case] = r
        print(case, json.dumps(r), flush=True)
        subprocess.run(["rm", "-rf", src])
    json.dump({"haven_commit": head, "results": results}, open(os.path.join(out, "results.json"), "w"), indent=1)

    rows = ["| Case | CPU | Read-back | Expected findings present | Extra findings | Unit tests | Boot check | Counts for A1 |",
            "| - | - | - | - | - | - | - | - |"]
    for case, r in results.items():
        for cpu in CPUS:
            c = r[cpu]
            rb = {0: "H matches G", 1: "drift", 2: "probe failure", None: "not run"}.get(c["readback_rc"], str(c["readback_rc"]))
            exp = EXPECT[case]
            if c["readback_rc"] == 2 or c["readback_rc"] is None:
                present = "no verdict"
            elif not exp:
                present = "yes" if c["readback_rc"] == 0 else "no (false drift)"
            else:
                present = "yes" if matched(exp, c["findings"]) else "no"
            extra = sorted({f"{reg}: {cls}" for reg, cls in c["findings"]
                            if not any(re.fullmatch(e[1], cls) and (e[0] == "*" or e[0] == reg) for e in exp)})
            unit = "pass" if r["unit_rc"] == 0 else "fail"
            boot = {0: "pass", None: "not run"}.get(c["boot_rc"], "fail")
            counts = "yes" if (exp and present == "yes" and unit == "pass" and boot == "pass") else ("control" if not exp else "no")
            rows.append(f"| {case} | {cpu} | {rb} | {present} | {'; '.join(extra) or '-'} | {unit} | {boot} | {counts} |")
    open(os.path.join(out, "results.md"), "w").write(
        f"# A1 case-set results (haven {head})\n\n" + "\n".join(rows) + "\n")
    print("\n".join(rows))


if __name__ == "__main__":
    main()
