"""Read-back v12: CPUs started on the guest's request (multi-vCPU SMP2).

A CPU in gic.cpus_on_request that runs nothing is "waiting for the guest"
while the partition still has a vCPU switched off in HAVEN's vCPU table;
then its granted PPIs and HAVEN's own may be off too."""
import unittest

import readback

ON, OFF, PENDING = 0, 1, 2
GRANT = {"cpus": {"0": 1, "1": 1, "2": 2, "3": 0}, "cpus_on_request": [1]}


def table(*states):
    return {"partition": 1, "count": len(states), "state": list(states)}


def waiting(rep):
    return sorted(x["cpu"] for x in rep if x.get("waiting"))


class OnRequest(unittest.TestCase):
    def test_waiting_while_a_vcpu_is_off(self):  # V1, entry breakpoint
        f, rep = readback.compare_cpus([1, 0, 2, 0], GRANT, table(ON, OFF))
        self.assertEqual(f, [])
        self.assertEqual(waiting(rep), [1])

    def test_started(self):  # V2, after KDAL
        f, rep = readback.compare_cpus([1, 1, 2, 0], GRANT, table(ON, ON))
        self.assertEqual((f, waiting(rep)), ([], []))

    def test_idle_with_every_vcpu_on_is_drift(self):
        f, _ = readback.compare_cpus([1, 0, 2, 0], GRANT, table(ON, ON))
        self.assertEqual(f, [("CPU 1", "CPU assignment drift", "granted to partition 1, HAVEN runs no partition"
                                                                " (on request; no vCPU of partition 1 is off)")])

    def test_pending_vcpu_does_not_count_as_off(self):
        f, _ = readback.compare_cpus([1, 0, 2, 0], GRANT, table(ON, PENDING))
        self.assertEqual(len(f), 1)

    def test_without_the_table_v10_applies(self):  # V3-like: no evidence either way
        f, rep = readback.compare_cpus([1, 0, 2, 0], GRANT, None)
        self.assertEqual(f, [("CPU 1", "CPU assignment drift", "granted to partition 1, HAVEN runs no partition")])
        self.assertEqual(waiting(rep), [])

    def test_cpu_not_on_request_v10_applies(self):
        g = dict(GRANT, cpus_on_request=[])
        f, _ = readback.compare_cpus([1, 0, 2, 0], g, table(ON, OFF))
        self.assertEqual(len(f), 1)

    def test_vcpu_pinned_to_cpu_3(self):  # FA: two CPU findings
        f, _ = readback.compare_cpus([1, 0, 2, 1], GRANT, table(ON, ON))
        self.assertEqual([x[0] for x in f], ["CPU 1", "CPU 3"])

    def test_off_vcpus_cover_only_as_many_idle_cpus(self):
        g = {"cpus": {"0": 1, "1": 1, "2": 1}, "cpus_on_request": [1, 2]}
        f, rep = readback.compare_cpus([1, 0, 0], g, table(ON, OFF, ON))
        self.assertEqual(waiting(rep), [1])
        self.assertEqual([x[0] for x in f], ["CPU 2"])

    def test_v10_tests_unchanged_without_on_request(self):
        f, rep = readback.compare_cpus([1, 0, 2, 0], {"cpus": {"0": 1, "1": 0, "2": 2, "3": 0}})
        self.assertEqual((f, waiting(rep)), ([], []))


def gic_ppis(cpu1_enabled):
    return {"ctlr": (1 << 4) | (1 << 1), "nirq": 64, "isenabler": [0, 0],
            "igroupr": [0xFFFFFFFF, 0xFFFFFFFF], "irouter": [0] * 32,
            "cpus": [{"typer": c << 32, "isenabler0": cpu1_enabled if c == 1 else 0} for c in range(4)]}


PPI_GRANT = {"cpus": {"0": 1, "1": 1, "2": 2, "3": 0}, "ppis": {"1": [27]}, "haven_ppis": {"1": [26]}}


class WaitingPpis(unittest.TestCase):
    def test_off_ppis_on_a_waiting_cpu_are_information(self):
        f, rep = readback.compare_gic(gic_ppis(0), PPI_GRANT, waiting_cpus={1})
        self.assertEqual(f, [])
        self.assertEqual(sorted((x["cpu"], x["irq"]) for x in rep["waiting_ppis"]), [(1, 26), (1, 27)])

    def test_off_ppis_on_a_cpu_not_waiting_are_findings(self):
        f, _ = readback.compare_gic(gic_ppis(0), PPI_GRANT)
        self.assertEqual(sorted(x[0] for x in f), ["CPU 1 PPI 26", "CPU 1 PPI 27"])

    def test_ungranted_ppi_on_a_waiting_cpu_is_still_drift(self):
        f, _ = readback.compare_gic(gic_ppis(1 << 25), PPI_GRANT, waiting_cpus={1})
        self.assertEqual([x[1] for x in f], ["interrupt enable drift"])


class GrantFormat(unittest.TestCase):
    def test_round_trip_through_the_ffa_format(self):
        import grant
        v1 = {"partitions": {"1": []}, "gic": {"cpus": {"0": 1, "1": 1}, "spis": {}, "ppis": {},
                                               "cpus_on_request": [1]}}
        v2 = grant.to_v2(v1)
        self.assertEqual(v2["haven"]["cpus-on-request"], [1])
        self.assertEqual(grant.to_v1(v2)["gic"]["cpus_on_request"], [1])


if __name__ == "__main__":
    unittest.main()
