"""Read-back v12a: routing and group are checked for granted SPIs that are
disabled or waiting for the guest (EL3 record, case I1)."""
import unittest

import readback


def gic(spi33_on, route_cpu=0, group1=True):
    return {
        "ctlr": (1 << 4) | (1 << 1),
        "nirq": 64,
        "isenabler": [0, (1 << 1) if spi33_on else 0],
        "igroupr": [0xFFFFFFFF, 0xFFFFFFFF if group1 else 0xFFFFFFFD],
        "irouter": [route_cpu if i == 1 else 0 for i in range(32)],
        "cpus": [{"typer": c << 32, "isenabler0": 0} for c in range(3)],
    }


GRANT = {"cpus": {"0": 1, "1": 0, "2": 2}, "spis": {"33": {"partition": 1, "cpu": 0}}}


def kinds(findings):
    return sorted((name, kind) for name, kind, _ in findings)


class DisabledSpi(unittest.TestCase):
    def test_disabled_and_routed_right(self):
        f, _ = readback.compare_gic(gic(False, 0), GRANT)
        self.assertEqual(kinds(f), [("SPI 33", "missing interrupt binding")])

    def test_disabled_and_routed_to_another_partition(self):  # case I1
        f, _ = readback.compare_gic(gic(False, 2), GRANT)
        self.assertEqual(kinds(f), [("SPI 33", "interrupt routing drift"),
                                    ("SPI 33", "missing interrupt binding")])

    def test_waiting_for_the_guest_but_routed_elsewhere(self):
        lazy = dict(GRANT, guest_enabled={"0": [33]})
        f, _ = readback.compare_gic(gic(False, 2), lazy, {0: 0})
        self.assertEqual(kinds(f), [("SPI 33", "interrupt routing drift")])

    def test_disabled_and_group_0(self):
        f, _ = readback.compare_gic(gic(False, 0, group1=False), GRANT)
        self.assertEqual(kinds(f), [("SPI 33", "interrupt group drift"),
                                    ("SPI 33", "missing interrupt binding")])

    def test_enabled_and_right_is_clean(self):
        f, _ = readback.compare_gic(gic(True, 0), GRANT)
        self.assertEqual(f, [])


if __name__ == "__main__":
    unittest.main()
