"""Read-back v18 (docs/methodology/stage2/20261124-lr-overflow/, row R1):
the grant's maintenance_ppi may be enabled on any CPU at HAVEN's priority."""
import unittest

import grant
import readback

H, P = 0x40, 0xA0


def gic(enabled, prio_of_25=H):
    cpus = []
    for c in range(2):
        pr = [0] * 8
        pr[25 // 4] = prio_of_25 << (8 * (25 % 4))
        cpus.append({"typer": c << 32, "isenabler0": enabled[c], "ipriorityr": pr})
    return {"ctlr": (1 << 4) | (1 << 1), "nirq": 64, "isenabler": [0, 0],
            "igroupr": [0xFFFFFFFF, 0xFFFFFFFF], "irouter": [0] * 32, "ipriorityr": [0] * 16,
            "cpus": cpus}


BASE = {"cpus": {"0": 1, "1": 0}, "spis": {}, "ppis": {}, "priority": {"haven": H, "partition": P}}
WITH = dict(BASE, maintenance_ppi=25)


class MaintenancePpi(unittest.TestCase):
    def test_enabled_at_havens_priority_is_no_finding(self):
        f, rep = readback.compare_gic(gic([1 << 25, 1 << 25]), WITH)
        self.assertEqual(f, [])
        self.assertEqual(rep["maintenance_off"], [])

    def test_wrong_priority_is_drift(self):
        f, _ = readback.compare_gic(gic([1 << 25, 0], prio_of_25=P), WITH)
        self.assertEqual([(x[0], x[1]) for x in f], [("CPU 0 PPI 25", "interrupt priority drift")])

    def test_off_on_a_running_cpu_is_an_observation(self):
        f, rep = readback.compare_gic(gic([0, 0]), WITH)
        self.assertEqual(f, [])
        self.assertEqual(rep["maintenance_off"], [0])  # CPU 1 runs nothing

    def test_without_the_field_as_before(self):
        f, _ = readback.compare_gic(gic([1 << 25, 0]), BASE)
        self.assertEqual([(x[0], x[1]) for x in f], [("CPU 0 PPIs", "interrupt enable drift")])

    def test_round_trip_through_the_ffa_format(self):
        v1 = {"partitions": {"1": []}, "gic": dict(WITH)}
        v2 = grant.to_v2(v1)
        self.assertEqual(v2["haven"]["maintenance-ppi"], 25)
        self.assertEqual(grant.to_v1(v2)["gic"]["maintenance_ppi"], 25)


if __name__ == "__main__":
    unittest.main()
