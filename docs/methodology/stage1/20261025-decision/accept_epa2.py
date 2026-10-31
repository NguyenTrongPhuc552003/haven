#!/usr/bin/env python3
"""Focused EP-A2 acceptance of the actual workspace source in isolated builds."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import time

ap = argparse.ArgumentParser()
ap.add_argument("--haven", required=True, type=Path)
ap.add_argument("--work", required=True, type=Path)
a = ap.parse_args()
repo, work = a.haven.resolve(), a.work.resolve()
work.mkdir(parents=True, exist_ok=False)
commands = []


def run(name, argv):
    argv = list(map(str, argv))
    start = time.monotonic()
    result = subprocess.run(argv, cwd=work, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=120, check=False)
    (work/(name+".log")).write_bytes(result.stdout)
    commands.append(dict(name=name, argv=argv, cwd=str(work), timeout_s=120,
                         returncode=result.returncode, elapsed_s=time.monotonic()-start))
    (work/"commands.json").write_text(json.dumps(commands, indent=2)+"\n")
    assert result.returncode == 0, name
    return result.stdout.decode(errors="replace")


for tool in ["aarch64-linux-gnu-gcc", "cmake", "ninja"]:
    run("version-"+tool, [tool, "--version"])
run("tracked-diff-check", ["git", "-C", repo, "diff", "--check"])
run("source-revision", ["git", "-C", repo, "rev-parse", "HEAD"])
run("complete-source-diff", ["git", "-C", repo, "diff", "--binary"])
run("epa2-source-diff", ["git", "-C", repo, "diff", "--", "src/common/string.c"])
results = {}
for name, strict in [("default", False), ("strict", True)]:
    build = work/name
    cfg = ["cmake", "-S", repo, "-B", build, "-G", "Ninja",
           "-DCMAKE_TOOLCHAIN_FILE="+str(repo/"cmake/arm64.cmake"),
           "-DPLATFORM=qemu-virt", "-DCMAKE_BUILD_TYPE=MinSizeRel"]
    if strict:
        cfg.append("-DCMAKE_C_FLAGS=-mstrict-align")
    run("configure-"+name, cfg)
    run("build-"+name, ["cmake", "--build", build, "--parallel", "4"])
    obj = build/"CMakeFiles/haven.dir/src/common/string.c.obj"
    syms = run("symbols-"+name, ["aarch64-linux-gnu-nm", "--defined-only", obj])
    assert re.search(r"\bT memcpy$", syms, re.M)
    for other in ["memset", "memmove", "memcmp"]:
        assert not re.search(r"\bT "+other+r"$", syms, re.M), other
    for what, path in [("object", obj), ("image", build/"haven.elf")]:
        unresolved = run("undefined-"+what+"-"+name, ["aarch64-linux-gnu-nm", "-u", path])
        assert not unresolved.strip(), unresolved
    dis = run("call-graph-"+name, ["aarch64-linux-gnu-objdump", "-dr", obj])
    funcs = dict(re.findall(r"^[0-9a-f]+ <([^>]+)>:\n(.*?)(?=^[0-9a-f]+ <|\Z)", dis, re.M | re.S))
    assert "memcpy" in funcs and "hv_memcpy" in funcs
    assert "hv_memcpy" in funcs["memcpy"]
    for symbol, body in funcs.items():
        if symbol.startswith("hv_"):
            assert not re.search(r"R_AARCH64_(?:CALL26|JUMP26)\s+(?:memcpy|memset|memmove|memcmp)\b", body), symbol
    results[name] = dict(build="PASS", standard_export="memcpy only",
                         unresolved_symbols=[], memory_helper_recursion=False,
                         images={p.name:hashlib.sha256(p.read_bytes()).hexdigest()
                                 for p in build.glob("*.bin")})

(work/"summary.json").write_text(json.dumps({
    "result":"PASS", "source":str(repo), "builds":results,
    "helper_source_sha256":hashlib.sha256((repo/"src/common/string.c").read_bytes()).hexdigest(),
    "semantics":"Reuse unchanged helper/GCC15.2 default+strict target and native execution evidence from Stage0",
    "not_run":["QEMU", "Linux/vGIC", "KDAL module", "physical boards"],
    "open_risk":"A1 alignment/boot/initialization remains independent and unresolved",
}, indent=2)+"\n")
print("PASS: actual-tree default/strict builds, symbols, undefined references and helper call graph")
