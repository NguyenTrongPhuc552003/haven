#!/usr/bin/env python3
"""Read-back v15 (docs/methodology/stage2/20261121-readback-vttbr, row V1):
each CPU's VTTBR_EL2 and HCR_EL2 against the grant and the stage-2 roots."""

import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import readback  # noqa: E402

ROOTS = {1: 0xA0000000, 2: 0xA0010000}
GRANT = {"cpus": {"0": 1, "1": 0, "2": 2, "3": 0}}


def regs(vttbr, hcr=1):
    return {"VTTBR_EL2": vttbr, "HCR_EL2": hcr}


class CpuStage2(unittest.TestCase):
    def kinds(self, cpu_regs, waiting=()):
        f, _ = readback.compare_cpu_stage2(cpu_regs, ROOTS, GRANT, waiting)
        return [(name, detail) for name, _, detail in f]

    def test_as_granted(self):
        vmid1, vmid2 = 1 << 48, 2 << 48
        self.assertEqual(self.kinds({0: regs(vmid1 | ROOTS[1]), 1: regs(0, 0),
                                     2: regs(vmid2 | ROOTS[2]), 3: regs(0, 0)}), [])

    def test_other_partitions_tables(self):  # case W09
        f = self.kinds({0: regs(ROOTS[1]), 2: regs((1 << 48) | ROOTS[1])})
        self.assertEqual(f, [("CPU 2", "granted to partition 2, runs partition 1's stage-2 tables")])

    def test_no_partitions_root(self):
        f = self.kinds({2: regs(0xBEEF0000)})
        self.assertEqual(f, [("CPU 2", "granted to partition 2, VTTBR_EL2 base 0xbeef0000 is no partition's root")])

    def test_stage2_off(self):
        f = self.kinds({2: regs(ROOTS[2], hcr=0)})
        self.assertEqual(f, [("CPU 2", "granted to partition 2, stage 2 is off (HCR_EL2.VM clear)")])

    def test_ungranted_and_waiting(self):
        self.assertEqual(self.kinds({1: regs(0, 0), 3: regs(0, 0)}), [])
        self.assertEqual(self.kinds({0: regs(ROOTS[1]), 2: regs(0, 0)}, waiting={2}), [])
        _, rep = readback.compare_cpu_stage2({3: regs(ROOTS[1])}, ROOTS, GRANT)
        self.assertEqual(rep[0]["note"], "stage 2 on, no partition granted")

    def test_vmid_width_and_cnp(self):
        # 8- or 16-bit VMID in bits 63:48 and CnP in bit 0 leave BADDR alone
        for top in (0xFF << 48, 0xFFFF << 48):
            self.assertEqual(self.kinds({2: regs(top | ROOTS[2] | 1)}), [])


class GdbReply(unittest.TestCase):
    def test_little_endian(self):
        self.assertEqual(readback.le_hex("0000010a00000200"), 0x000200000A010000)
        self.assertEqual(readback.le_hex("0100000000000000"), 1)

    def test_errors(self):
        for r in ("", "E01", "abc"):
            with self.assertRaises(ValueError):
                readback.le_hex(r)


if __name__ == "__main__":
    unittest.main()
