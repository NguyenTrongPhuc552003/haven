#!/usr/bin/env python3
"""Read-back's board path (docs/methodology/stage2/20261123-readback-board-path,
row B1): the platform table equals the platforms' own headers, and the
redistributor frames are found by GICR_TYPER affinity."""

import os
import re
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import platforms  # noqa: E402

SRC = os.path.join(HERE, "..", "..", "src", "platform")
KEYS = {  # table entry -> the header's name for it
    "qemu-virt": {"image": ("HAVEN_LOAD_PA", "HAVEN_SIZE"), "smmu": "QEMU_SMMU_BASE",
                  "gicd": "QEMU_GIC_DIST_BASE", "gicr": "QEMU_GIC_REDIST_BASE"},
    "cix-p1": {"image": ("HAVEN_LOAD_PA", "HAVEN_SIZE"), "smmu": "CIX_SMMU0_BASE",
               "gicd": "CIX_GIC_DIST_BASE", "gicr": "CIX_GIC_REDIST_BASE"},
}


def header(name):
    text = open(os.path.join(SRC, name, "memory.h")).read()
    defs = {m.group(1): int(m.group(2).rstrip("UL"), 0)
            for m in re.finditer(r"(?m)^#define (\w+) +(0x[0-9A-Fa-f]+U?L*|\d+U?L*)\b", text)}
    cpu = open(os.path.join(SRC, name, "platform.c")).read()
    body = re.search(r"hv_cpu_mpidr\[\] = \{([^}]*)\}", cpu).group(1)
    return defs, [int(x, 0) for x in re.findall(r"0x[0-9a-fA-F]+", body)]


def typers(frames):
    """read_typer over {rd: (affinity, vlpis, last)}."""
    def read(rd):
        if rd not in frames:
            return None
        aff, vlpis, last = frames[rd]
        return aff << 32 | vlpis << 1 | last << 4
    return read


class Table(unittest.TestCase):
    def test_matches_headers(self):
        for name, keys in KEYS.items():
            defs, mpidr = header(name)
            p = platforms.PLATFORMS[name]
            self.assertEqual(p["image"], (defs[keys["image"][0]], defs[keys["image"][1]]), name)
            for k in ("smmu", "gicd", "gicr"):
                self.assertEqual(p[k], defs[keys[k]], f"{name} {k}")
            self.assertEqual(p["mpidr"], mpidr, name)


class Frames(unittest.TestCase):
    def test_qemu_stride(self):
        base = 0x080A0000
        f = {base + i * 0x20000: (i, 0, int(i == 3)) for i in range(4)}
        self.assertEqual(platforms.gicr_frames(typers(f), base, [0, 1, 2, 3]),
                         [base + i * 0x20000 for i in range(4)])

    def test_cix_256k_frames_boot_core_4(self):
        base, mp = 0x0E090000, platforms.PLATFORMS["cix-p1"]["mpidr"]
        f = {base + c * 0x40000: (c << 8, 1, int(c == 11)) for c in range(12)}  # frame c is core c
        got = platforms.gicr_frames(typers(f), base, mp)
        self.assertEqual(got[0], base + 4 * 0x40000)  # logical 0 is core 4
        self.assertEqual(got[8], base + 8 * 0x40000)
        self.assertEqual(got, [base + (m >> 8) * 0x40000 for m in mp])

    def test_missing_frame(self):
        base = 0x080A0000
        f = {base + i * 0x20000: (i, 0, int(i == 2)) for i in range(3)}  # three frames, CPU 3 has none
        self.assertEqual(platforms.gicr_frames(typers(f), base, [0, 1, 2, 3])[3], None)


if __name__ == "__main__":
    unittest.main()
