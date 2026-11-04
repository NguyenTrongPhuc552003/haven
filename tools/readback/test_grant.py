"""Tests for grant.py: every format 1 grant in the repository survives
format 1 -> format 2 -> format 1 unchanged (numbers compared by value).

Run: python3 -m unittest discover -s tools/readback -p 'test_*.py'
"""
import glob
import json
import os
import unittest

import grant

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")


def norm(x):
    """Hex strings to ints, so 0x40000000 and 0x4000_0000 forms compare equal."""
    if isinstance(x, dict):
        if "window" in x and "access" not in x:
            x = dict(x, access="RW")  # read-back's default (readback.py: grant.get("access", "RW"))
        return {k: norm(v) for k, v in x.items() if k != "source"}
    if isinstance(x, list):
        out = [norm(v) for v in x]
        # Region order only changes the order of findings, not the verdict
        # (compare_stage2 checks each region independently).
        if out and all(isinstance(v, dict) and "ipa" in v for v in out):
            out.sort(key=lambda v: v["ipa"])
        return out
    if isinstance(x, str) and x.lower().startswith("0x"):
        return int(x, 16)
    return x


def v1_grants():
    for path in sorted(glob.glob(os.path.join(ROOT, "docs", "**", "grant*.json"), recursive=True)):
        g = json.load(open(path))
        if "partitions" in g and "format" not in g:
            yield path, g


class RoundTrip(unittest.TestCase):
    def test_every_repository_grant(self):
        n = 0
        for path, g in v1_grants():
            with self.subTest(path=os.path.relpath(path, ROOT)):
                back = grant.to_v1(grant.to_v2(g, level_spis=[33]))
                self.assertEqual(norm(back), norm(g))
                n += 1
        self.assertGreater(n, 0, "no format 1 grants found")

    def test_ffa_attribute_encoding(self):
        g = {"partitions": {"1": [{"name": "uart", "ipa": "0x9000000", "pa": "0x9000000",
                                   "size": "0x1000", "attrs": "DEVICE_RW"}]},
             "gic": {"cpus": {"0": 1}, "spis": {"33": {"partition": 1, "cpu": 0}},
                     "priority": {"haven": "0x40", "partition": "0xa0"}}}
        v2 = grant.to_v2(g, level_spis=[33])
        dev = v2["partitions"][0]["device-regions"]
        self.assertEqual(dev[0]["attributes"], "0x3")          # read | write
        irq = dev[1]["interrupts"][0]
        self.assertEqual(int(irq["attributes"], 16), 0xA0 | (1 << 9) | (0b10 << 10))  # prio, level, SPI

    def test_dma_window_must_be_cpu_mapped(self):
        v2 = {"format": "haven-grant/2", "partitions": [{"id": 1, "memory-regions": [
            {"base-address": "0x5F000000", "pages-count": 256, "attributes": "0x3",
             "smmu-id": 0, "stream-ids": [8]}]}], "haven": {}}
        with self.assertRaises(ValueError):
            grant.to_v1(v2)


if __name__ == "__main__":
    unittest.main()
