"""Tests for demand_check.py (KDAL demand against a HAVEN grant)."""
import unittest

import demand_check

GRANT = {
    "partitions": {"1": [
        {"name": "RAM", "ipa": "0x40000000", "pa": "0x80000000", "size": "0x1000000", "attrs": "RWX"},
        {"name": "GPIO", "ipa": "0x09030000", "pa": "0x09030000", "size": "0x1000", "attrs": "DEVICE_RW"},
        {"name": "RTC", "ipa": "0x09010000", "pa": "0x09010000", "size": "0x1000", "attrs": "DEVICE_R"},
    ]},
    "smmu": {"enabled": True, "streams": {"8": {"partition": 1}}, "allowed_unenforced": []},
    "gic": {"cpus": {"0": 1, "1": 0}, "spis": {"39": {"partition": 1, "cpu": 0}},
            "ppis": {"0": [27]}},
}


def demand(*regions):
    return {"format": "kdal-demand/1", "device-regions": list(regions)}


class DemandCheck(unittest.TestCase):
    def test_met(self):
        d = demand({"description": "gpio", "base-address": "0x9030000", "pages-count": 1,
                    "attributes": "0x3", "interrupts": [{"id": 39}, {"id": 27}], "stream-ids": [8]})
        f, info = demand_check.check(GRANT, d, 1)
        self.assertEqual(f, [])
        self.assertEqual(info, ["not demanded: RTC 0x9010000+0x1000"])

    def test_unmet(self):
        d = demand({"description": "x", "base-address": "0x9040000", "pages-count": 1, "attributes": "0x3",
                    "interrupts": [{"id": 40}, {"id": 30}], "stream-ids": [9]},
                   {"description": "rtc", "base-address": "0x9010000", "pages-count": 1, "attributes": "0x3"})
        classes = sorted(c for _, c, _ in demand_check.check(GRANT, d, 1)[0])
        self.assertEqual(classes, ["insufficient access", "unmet DMA demand", "unmet MMIO demand",
                                   "unmet interrupt demand", "unmet interrupt demand"])

    def test_other_partition_gets_nothing(self):
        d = demand({"description": "gpio", "base-address": "0x9030000", "pages-count": 1,
                    "attributes": "0x1", "interrupts": [{"id": 39}]})
        self.assertEqual(len(demand_check.check(GRANT, d, 2)[0]), 2)

    def test_region_must_fit_inside(self):
        d = demand({"description": "wide", "base-address": "0x9030000", "pages-count": 2, "attributes": "0x1"})
        self.assertEqual([c for _, c, _ in demand_check.check(GRANT, d, 1)[0]], ["unmet MMIO demand"])


if __name__ == "__main__":
    unittest.main()
