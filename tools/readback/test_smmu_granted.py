#!/usr/bin/env python3
"""Read-back v16 (docs/methodology/stage2/20261123-readback-v16, row V1):
a granted stream that no DMA master covers is checked through its own
stream-table entry."""

import os
import struct
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import readback  # noqa: E402

ROOTS = {1: 0x8000E000}
PARTS = {1: [{"ipa": 0x40000000, "pa": 0x80800000, "size": 0x20000000}]}
GRANT = {"enabled": True, "streams": {"8": {"partition": 1, "window": {"ipa": "0x5F000000", "size": "0x100000"},
                                            "access": "RW"}}}
ABORT = (1, 0)  # V = 1, config 0
OUTSIDE = (1 | (6 << 1), 0x100008001A000)  # stage 2, a table address with a VMID bit (H2)


def snap(entry, pci):
    table = bytearray(256 * 64)
    struct.pack_into("<4Q", table, 8 * 64, entry[0], 0, 0, entry[1])
    return {"smmu": {"cr0ack": 1, "cmdq_cons": 0, "gerror": 0}, "stream_table": bytes(table),
            "image": bytes(readback.IMG_SIZE), "pci": pci}


def master(sid):
    return {"device": "pci-1af4:1044", "bus": "pci 00:01.0", "sid": sid, "behind_smmu": True}


class GrantedStreams(unittest.TestCase):
    def run_smmu(self, entry, pci, masters=()):
        s = snap(entry, pci)
        if masters:  # the master path classifies; only what v16 adds matters here
            readback.classify = lambda m, *_: ("translated:1", "stub")
        try:
            return readback.compare_smmu(s, list(masters), GRANT, ROOTS, PARTS)
        finally:
            readback.classify = CLASSIFY

    def test_el3_abort_is_a_finding(self):
        f, _ = self.run_smmu(ABORT, [])
        self.assertEqual([(n, d) for _, n, d in f],
                         [("granted stream not translated", "SID 8: entry abort, granted to partition 1")])

    def test_monitor_no_device_is_an_observation(self):
        f, rep = self.run_smmu(ABORT, [{"bus": 0, "devices": []}])
        self.assertEqual(f, [])
        self.assertEqual([(r["sid"], r["state"]) for r in rep], [(8, "withheld")])

    def test_master_covers_the_stream(self):
        f, rep = self.run_smmu(ABORT, [], masters=[master(8)])
        self.assertEqual(f, [])
        self.assertEqual([r["state"] for r in rep], ["translated:1"])

    def test_table_of_no_partition(self):
        f, _ = self.run_smmu(OUTSIDE, [])
        self.assertEqual([n for _, n, _ in f], ["DMA enforcement drift"])
        self.assertIn("SID 8: translated:unknown", f[0][2])
        f, _ = self.run_smmu(OUTSIDE, [], masters=[master(8)])
        self.assertEqual(f, [])  # the master path reports it in monitor mode


CLASSIFY = readback.classify

if __name__ == "__main__":
    unittest.main()
