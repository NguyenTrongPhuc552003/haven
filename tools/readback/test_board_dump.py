#!/usr/bin/env python3
"""Read-back's board path, row B5 (docs/methodology/stage2/
20261123-readback-board-path): a synthetic CIX P1 EL3 dump (twelve 256 KiB
redistributor frames, core 4 first) read with --platform cix-p1 matches the
board grant v1, and a PPI disabled in core 8's frame is reported on
logical CPU 8."""

import json
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import platforms  # noqa: E402
import readback  # noqa: E402

GRANT = os.path.join(HERE, "..", "..", "docs", "methodology", "stage2", "20261123-cix-p1-partition-b",
                     "grant-cixp1-v1.json")
NIRQ = 352  # GICD_TYPER.ITLinesNumber 10 (INTIDs to 351: UART2 330, UART3 331)


def secure_prio(ns):
    """The Secure view of a Non-secure priority (the dump reads Secure)."""
    return (ns >> 1) | 0x80


def dump(gic, drop_ppi_on_core=None):
    """RB lines of a dump in which the GIC holds what the grant asks."""
    p = platforms.PLATFORMS["cix-p1"]
    gicd, gicr, smmu = p["gicd"], p["gicr"], p["smmu"]
    w = {gicd: 0x37, gicd + 0x4: NIRQ // 32 - 1}
    spis = {int(k): v for k, v in gic["spis"].items()}
    hp, pp = int(gic["priority"]["haven"], 0), int(gic["priority"]["partition"], 0)
    for i in range(NIRQ // 32):
        w[gicd + 0x80 + 4 * i] = 0xFFFFFFFF  # every interrupt Group 1 Non-secure
        w[gicd + 0xD00 + 4 * i] = 0
        w[gicd + 0x100 + 4 * i] = sum(1 << (irq % 32) for irq in spis if irq // 32 == i)
    for i in range(NIRQ // 4):
        v = 0
        for b in range(4):
            irq = 4 * i + b
            if irq in spis:
                v |= secure_prio(hp if spis[irq]["partition"] == 0 else pp) << (8 * b)
        w[gicd + 0x400 + 4 * i] = v
    mpidr = p["mpidr"]
    for irq in range(32, NIRQ):
        cpu = spis[irq]["cpu"] if irq in spis else 0
        w[gicd + 0x6000 + 8 * irq] = mpidr[cpu] & 0xFFFFFF
    for core in range(12):  # frame k is core k (the GIC-700 lists frames by core)
        rd = gicr + core * 0x40000
        w[rd + 0x8] = (core << 8) << 32 | 1 << 1 | (1 << 4 if core == 11 else 0)
        logical = mpidr.index(core << 8)
        sgi = rd + 0x10000
        on = set(gic["ppis"].get(str(logical), [])) | set(gic["haven_ppis"].get(str(logical), []))
        if core == drop_ppi_on_core:
            on.discard(27)
        w[sgi + 0x80], w[sgi + 0xD00] = 0xFFFFFFFF, 0
        w[sgi + 0x100] = sum(1 << b for b in on)
        for i in range(8):
            v = 0
            for b in range(4):
                irq = 4 * i + b
                if irq in on:
                    v |= secure_prio(hp if irq in gic["haven_ppis"].get(str(logical), []) else pp) << (8 * b)
            w[sgi + 0x400 + 4 * i] = v
    for off in (0x0, 0x20, 0x24, 0x44, 0x60, 0x88, 0x98, 0x9C, 0x100A8, 0x100AC, 0x80, 0x90, 0xA0):
        w[smmu + off] = 0  # SMMU off: no DMA grant in the board grant
    lines = ["RB BEGIN 1 0000000000000001 0000000080000400"]
    lines += [f"RB W {a:010x} {v:x}" for a, v in sorted(w.items())]
    return "\n".join(lines + [f"RB END {len(w):x}"])


class BoardDump(unittest.TestCase):
    def setUp(self):
        readback.use_platform("cix-p1")
        with open(GRANT) as f:
            self.gic = json.load(f)["gic"]

    def tearDown(self):
        readback.use_platform("qemu-virt")

    def findings(self, text):
        snap = readback.el3_snapshot(text)
        f, _ = readback.compare_gic(snap["gic"], self.gic, {}, True, set())
        return [(n, k) for n, k, _ in f]

    def test_matches_grant(self):
        self.assertEqual(self.findings(dump(self.gic)), [])

    def test_ppi_off_in_core_8_is_logical_8(self):
        self.assertEqual(self.findings(dump(self.gic, drop_ppi_on_core=8)),
                         [("CPU 8 PPI 27", "missing interrupt binding")])

    def test_core_without_frame_is_a_probe_failure(self):
        text = "\n".join(x for x in dump(self.gic).splitlines()
                         if not x.startswith(f"RB W {platforms.PLATFORMS['cix-p1']['gicr'] + 11 * 0x40000 + 8:010x}"))
        with self.assertRaises(readback.ProbeFailure):
            readback.el3_snapshot(text)


if __name__ == "__main__":
    unittest.main()
