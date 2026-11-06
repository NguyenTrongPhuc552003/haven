"""Read-back v8: interrupts HAVEN enables on the guest's request.

compare_gic() on a synthetic GIC state, one CPU, SPI 33 and PPI 27 granted
to partition 1 and marked guest-enabled; the emulated GIC's enable bits
(model) and the hardware's are varied. Also: the field survives the
format 1 -> 2 -> 1 conversion.
"""
import unittest

import grant
import readback


def gic(spi33_on, ppi27_on):
    return {
        "ctlr": (1 << 4) | (1 << 1),
        "nirq": 64,
        "isenabler": [0, (1 << 1) if spi33_on else 0],
        "igroupr": [0xFFFFFFFF, 0xFFFFFFFF],
        "irouter": [0] * 32,
        "cpus": [{"typer": 0, "isenabler0": (1 << 27) if ppi27_on else 0}],
    }


EXPECTED = {
    "cpus": {"0": 1},
    "spis": {"33": {"partition": 1, "cpu": 0}},
    "ppis": {"0": [27]},
    "guest_enabled": {"0": [27, 33]},
}


def kinds(findings):
    return sorted((name, kind) for name, kind, _ in findings)


class GuestEnabled(unittest.TestCase):
    def test_waiting_for_the_guest_is_not_a_finding(self):
        f, rep = readback.compare_gic(gic(False, False), EXPECTED, {0: 0})
        self.assertEqual(f, [])
        self.assertEqual(len(rep["guest_enabled"]), 2)

    def test_enabled_by_guest_and_gic_matches(self):
        f, _ = readback.compare_gic(gic(True, True), EXPECTED, {0: (1 << 33) | (1 << 27)})
        self.assertEqual(f, [])

    def test_guest_asked_gic_off_is_missing_binding(self):
        f, _ = readback.compare_gic(gic(False, True), EXPECTED, {0: (1 << 33) | (1 << 27)})
        self.assertEqual(kinds(f), [("SPI 33", "missing interrupt binding")])

    def test_gic_on_guest_never_asked_is_drift(self):
        f, _ = readback.compare_gic(gic(True, True), EXPECTED, {0: 0})
        self.assertEqual(kinds(f), [("CPU 0 PPI 27", "interrupt enable drift"),
                                    ("SPI 33", "interrupt enable drift")])

    def test_without_model_the_old_rule_applies(self):
        f, _ = readback.compare_gic(gic(False, False), EXPECTED, {})
        self.assertEqual(kinds(f), [("CPU 0 PPI 27", "missing interrupt binding"),
                                    ("SPI 33", "missing interrupt binding")])

    def test_field_survives_format_conversion(self):
        v1 = {"partitions": {"1": [{"name": "RAM", "ipa": "0x40000000", "pa": "0x80800000",
                                    "size": "0x20000000", "attrs": "RWX"}]},
              "gic": dict(EXPECTED)}
        back = grant.to_v1(grant.to_v2(v1))
        self.assertEqual(back["gic"]["guest_enabled"], {"0": [27, 33]})


if __name__ == "__main__":
    unittest.main()
