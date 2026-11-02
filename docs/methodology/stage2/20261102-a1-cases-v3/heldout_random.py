#!/usr/bin/env python3
"""Draw held-out faults at random instead of choosing them (mutation sampling).

Usage: heldout_random.py <haven-repo> <out-dir> <seed-text> [count]

The owner asked Claude to write and run the held-out cases. To keep the
property that the tool's author does not pick the faults, they are drawn
from every single-point mutation of the enforcement code, with a seed fixed
before the list exists:

  1. enumerate all mutants of the files below, in a fixed order, with five
     operators: change a numeric constant (+1, or one page for multiples
     of 0x1000), swap a comparison (< <= > >= == !=), swap && and ||,
     delete a statement that only calls a function, swap | and &;
  2. shuffle the list with random.Random(sha256(seed-text)) and walk it;
  3. keep a mutant if both the host tests build and the ARM64 image build
     (a fault that does not compile tests the compiler, not the method);
     record every skipped one;
  4. stop at count (default 4); write NAME.patch for each, plus draw.json.

Expectations are written by hand after the draw and before the run
(EXPECT-random.md), then everything is frozen and run with
run_cases_v3.py --held-out.
"""
import hashlib
import json
import os
import random
import re
import shutil
import subprocess
import sys
import tempfile

FILES = [
    "src/core/partition.c", "src/platform/qemu-virt/memory.h", "arch/arm64/include/asm/s2_attrs.h",
    "arch/arm64/mm.c", "src/core/mm/stage2.c", "src/core/dma/smmu.c", "drivers/iommu/smmu_v3.c",
    "drivers/pci/ecam.c", "drivers/irqchip/gic_v3.c", "src/core/irq/ownership.c", "arch/arm64/irq.c",
]
TOOLCHAIN = "/opt/homebrew/Cellar/aarch64-unknown-linux-gnu/15.2.0/toolchain/bin"
ENV = dict(os.environ, PATH=f"/opt/homebrew/bin:/usr/local/bin:{TOOLCHAIN}:" + os.environ.get("PATH", ""))
CMP = {"<": "<=", "<=": "<", ">": ">=", ">=": ">", "==": "!=", "!=": "=="}


def code_lines(text):
    """(index, line) for code lines: not blank, comment or preprocessor, except
    #define, which carries the memory map's addresses and sizes."""
    out, in_comment = [], False
    for i, line in enumerate(text.split("\n")):
        s = line.strip()
        if in_comment:
            if "*/" in s:
                in_comment = False
            continue
        if s.startswith("/*"):
            in_comment = "*/" not in s
            continue
        if not s or s.startswith("//") or s.startswith("*"):
            continue
        if s.startswith("#") and not s.startswith("#define "):
            continue
        out.append((i, line))
    return out


def mutants_of(path, text):
    lines = text.split("\n")
    out = []
    for i, line in code_lines(text):
        code = line.split("/*")[0]
        for m in re.finditer(r"\b(0x[0-9a-fA-F]+|\d+)(U?L*|UL+|ULL)?\b", code):
            v = int(m.group(1), 0)
            nv = v + 0x1000 if v and v % 0x1000 == 0 else v + 1
            lit = (hex(nv) if m.group(1).lower().startswith("0x") else str(nv)) + (m.group(2) or "")
            out.append((path, i, m.start(), m.end(), lit, f"constant {m.group(0)} -> {lit}"))
        for m in re.finditer(r"(?<![<>=!&|])(<=|>=|==|!=|<|>)(?![<>=])", code):
            if "->" in code[max(0, m.start() - 1):m.end() + 1]:
                continue
            out.append((path, i, m.start(), m.end(), CMP[m.group(1)], f"comparison {m.group(1)} -> {CMP[m.group(1)]}"))
        for m in re.finditer(r"&&|\|\|", code):
            rep = "||" if m.group(0) == "&&" else "&&"
            out.append((path, i, m.start(), m.end(), rep, f"logic {m.group(0)} -> {rep}"))
        for m in re.finditer(r"(?<![|&])(\||&)(?![|&=])", code):
            if m.group(0) == "&" and re.match(r"\s*&\w", code[m.start():]) and not re.search(r"[\w)\]]\s*$", code[:m.start()]):
                continue  # address-of, not bitwise and
            rep = "&" if m.group(0) == "|" else "|"
            out.append((path, i, m.start(), m.end(), rep, f"bitwise {m.group(0)} -> {rep}"))
        if re.fullmatch(r"\s*(\(void\))?\w+\(.*\);\s*", code) and not re.match(r"\s*return\b", code):
            out.append((path, i, 0, len(line), "", f"delete call: {line.strip()}"))
    return out


def apply(src, mut):
    path, i, a, b, rep, _ = mut
    p = os.path.join(src, path)
    lines = open(p).read().split("\n")
    lines[i] = lines[i][:a] + rep + lines[i][b:]
    open(p, "w").write("\n".join(lines))


def builds(src, log):
    with open(log, "a") as f:
        for cmd in ("cmake --preset host-tests && cmake --build build-host",
                    "cmake --preset arm64-qemu -DHAVEN_BOOT_PROFILE=qemu-direct && cmake --build build"):
            if subprocess.run(cmd, cwd=src, shell=True, stdout=f, stderr=subprocess.STDOUT, env=ENV).returncode:
                return False
    return True


def main():
    repo, out, seed_text = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2]), sys.argv[3]
    count = int(sys.argv[4]) if len(sys.argv) > 4 else 4
    head = subprocess.check_output(["git", "-C", repo, "rev-parse", "HEAD"], text=True).strip()
    seed = hashlib.sha256(seed_text.encode()).hexdigest()
    allm = []
    for path in FILES:
        allm += mutants_of(path, open(os.path.join(repo, path)).read())
    order = list(range(len(allm)))
    random.Random(seed).shuffle(order)
    os.makedirs(out, exist_ok=True)
    draw = {"haven_commit": head, "seed_text": seed_text, "seed_sha256": seed, "mutants_total": len(allm),
            "kept": [], "skipped": []}
    for k in order:
        if len(draw["kept"]) == count:
            break
        mut = allm[k]
        src = tempfile.mkdtemp(prefix="heldout-")
        subprocess.run(f"git -C {repo} archive HEAD | tar -x -C {src}", shell=True, check=True)
        apply(src, mut)
        entry = {"index": k, "file": mut[0], "line": mut[1] + 1, "mutation": mut[5]}
        if builds(src, os.path.join(out, "draw-build.log")):
            name = f"R{len(draw['kept']) + 1}"
            entry["name"] = name
            diff = subprocess.run(["diff", "-u", "--label", f"a/{mut[0]}", "--label", f"b/{mut[0]}",
                                   os.path.join(repo, mut[0]), os.path.join(src, mut[0])],
                                  capture_output=True, text=True).stdout
            open(os.path.join(out, f"{name}.patch"), "w").write(diff)
            draw["kept"].append(entry)
        else:
            draw["skipped"].append(entry)
        shutil.rmtree(src)
    json.dump(draw, open(os.path.join(out, "draw.json"), "w"), indent=1)
    print(json.dumps(draw, indent=1))


if __name__ == "__main__":
    main()
