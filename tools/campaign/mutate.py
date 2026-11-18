#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Draw faults at random from every single-point mutation of HAVEN's
enforcement code (docs/methodology/stage2/20261119-evaluation-campaign,
step C1). Moved from ../../docs/methodology/stage2/20261102-a1-cases-v3/
heldout_random.py with the enumeration and the draw unchanged (row K1),
and the build check using the toolchain on PATH instead of a Mac path.

Usage: mutate.py REPO OUT SEED_TEXT [COUNT] [--files PATH...]
       mutate.py REPO - SEED_TEXT --order N [--files PATH...]
  1. enumerate all mutants of the files, in a fixed order, with five
     operators: change a numeric constant (+1, or one page for multiples
     of 0x1000), swap a comparison (< <= > >= == !=), swap && and ||,
     delete a statement that only calls a function, swap | and &;
  2. shuffle the list with random.Random(sha256(SEED_TEXT)) and walk it;
  3. keep a mutant if both the host tests build and the ARM64 image build,
     recording every skipped one; stop at COUNT (default 4); write
     NAME.patch for each, plus draw.json.
--order N prints the total and the first N mutants of the shuffled order
as JSON, building nothing.
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

# the files of case set v3's draw (its default, kept for row K1)
FILES = [
    "src/core/partition.c", "src/platform/qemu-virt/memory.h", "arch/arm64/include/asm/s2_attrs.h",
    "arch/arm64/mm.c", "src/core/mm/stage2.c", "src/core/dma/smmu.c", "drivers/iommu/smmu_v3.c",
    "drivers/pci/ecam.c", "drivers/irqchip/gic_v3.c", "src/core/irq/ownership.c", "arch/arm64/irq.c",
]
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
            if subprocess.run(cmd, cwd=src, shell=True, stdout=f, stderr=subprocess.STDOUT).returncode:
                return False
    return True



def enumerate_all(repo, files, read=None):
    """Every mutant of FILES in order; READ(path) gives a file's text
    (default: the file under REPO)."""
    def from_dir(path):
        with open(os.path.join(repo, path)) as f:
            return f.read()
    allm = []
    for path in files:
        allm += mutants_of(path, (read or from_dir)(path))
    return allm


def shuffled(n, seed_text):
    """The walk order of n mutants for SEED_TEXT, and the seed's SHA-256."""
    seed = hashlib.sha256(seed_text.encode()).hexdigest()
    order = list(range(n))
    random.Random(seed).shuffle(order)
    return order, seed


def entry(k, mut):
    return {"index": k, "file": mut[0], "line": mut[1] + 1, "mutation": mut[5]}


def draw(repo, out, seed_text, count, files):
    head = subprocess.check_output(["git", "-C", repo, "rev-parse", "HEAD"], text=True).strip()
    allm = enumerate_all(repo, files)
    order, seed = shuffled(len(allm), seed_text)
    os.makedirs(out, exist_ok=True)
    result = {"haven_commit": head, "seed_text": seed_text, "seed_sha256": seed, "mutants_total": len(allm),
              "kept": [], "skipped": []}
    for k in order:
        if len(result["kept"]) == count:
            break
        mut = allm[k]
        src = tempfile.mkdtemp(prefix="mutate-")
        subprocess.run(f"git -C {repo} archive HEAD | tar -x -C {src}", shell=True, check=True)
        apply(src, mut)
        e = entry(k, mut)
        if builds(src, os.path.join(out, "draw-build.log")):
            e["name"] = f"R{len(result['kept']) + 1}"
            diff = subprocess.run(["diff", "-u", "--label", f"a/{mut[0]}", "--label", f"b/{mut[0]}",
                                   os.path.join(repo, mut[0]), os.path.join(src, mut[0])],
                                  capture_output=True, text=True).stdout
            with open(os.path.join(out, f"{e['name']}.patch"), "w") as f:
                f.write(diff)
            result["kept"].append(e)
        else:
            result["skipped"].append(e)
        shutil.rmtree(src)
    with open(os.path.join(out, "draw.json"), "w") as f:
        json.dump(result, f, indent=1)
    return result


def main(argv):
    files = FILES
    if "--files" in argv:
        i = argv.index("--files")
        argv, files = argv[:i], argv[i + 1:]
    order_n = None
    if "--order" in argv:
        i = argv.index("--order")
        order_n = int(argv[i + 1])
        argv = argv[:i] + argv[i + 2:]
    if len(argv) not in (3, 4):
        sys.exit(__doc__)
    repo, out, seed_text = os.path.abspath(argv[0]), argv[1], argv[2]
    if order_n is not None:
        allm = enumerate_all(repo, files)
        order, seed = shuffled(len(allm), seed_text)
        print(json.dumps({"seed_sha256": seed, "mutants_total": len(allm),
                          "order": [entry(k, allm[k]) for k in order[:order_n]]}, indent=1))
        return 0
    result = draw(repo, os.path.abspath(out), seed_text, int(argv[3]) if len(argv) == 4 else 4, files)
    print(json.dumps(result, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
