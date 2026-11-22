#!/usr/bin/env python3
"""CIX P1's fixed memory layout against the loading study (row P4 of
docs/methodology/stage2/20261123-cix-p1-partition-b): HAVEN's image and
both partitions' RAM are 2 MiB aligned, do not overlap one another, and
overlap no fixed firmware reservation of docs/porting/cix-p1-loading.md,
section 5.2 (read from that table, so the two cannot drift apart)."""

import os
import re
import unittest

REPO = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
MEMORY_H = os.path.join(REPO, "src", "platform", "cix-p1", "memory.h")
STUDY = os.path.join(REPO, "docs", "porting", "cix-p1-loading.md")


def defines(path):
    """#define NAME value, values that are numbers or other names."""
    raw = {}
    for m in re.finditer(r"(?m)^#define (\w+) +([\w]+)\b", open(path).read()):
        raw[m.group(1)] = m.group(2)

    def value(name, depth=0):
        v = raw[name]
        if v in raw and depth < 8:
            return value(v, depth + 1)
        return int(v.rstrip("UL"), 0)
    return value


def reservations(path):
    """(base, size, what) of section 5.2's rows that firmware keeps from the OS."""
    text = open(path).read()
    sec = text[text.index("### 5.2"):]
    sec = sec[:sec.index("\n### ", 4)] if "\n### " in sec[4:] else sec
    out = []
    for m in re.finditer(r"(?m)^\| (0x[0-9A-F_]+) \+ (0x[0-9A-F_]+) \| ([^|]+)\| ([^|]+)\|", sec):
        if "not reserved" in m.group(4):
            continue
        out.append((int(m.group(1).replace("_", ""), 16), int(m.group(2).replace("_", ""), 16), m.group(3).strip()))
    return out


class CixLayout(unittest.TestCase):
    def setUp(self):
        v = defines(MEMORY_H)
        self.ranges = [("HAVEN", v("HAVEN_LOAD_PA"), v("HAVEN_SIZE")),
                       ("partition A", v("PART_A_PA_BASE"), v("PART_A_SIZE")),
                       ("partition B", v("PART_B_PA_BASE"), v("PART_B_SIZE"))]
        self.reserved = reservations(STUDY)

    def test_study_table_read(self):
        self.assertGreaterEqual(len(self.reserved), 8)  # the eight kept rows of 5.2

    def test_aligned(self):
        for name, base, size in self.ranges:
            self.assertEqual((base | size) & 0x1FFFFF, 0, name)

    def test_disjoint(self):
        for i, (n1, b1, s1) in enumerate(self.ranges):
            for n2, b2, s2 in self.ranges[i + 1:]:
                self.assertFalse(b1 < b2 + s2 and b2 < b1 + s1, f"{n1} and {n2}")

    def test_clear_of_reservations(self):
        for name, base, size in self.ranges:
            for rb, rs, what in self.reserved:
                self.assertFalse(base < rb + rs and rb < base + size, f"{name} overlaps {what}")


if __name__ == "__main__":
    unittest.main()
