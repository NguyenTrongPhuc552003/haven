"""Read-back v10: CPU assignment drift between gic.cpus and HAVEN's table."""
import unittest

import readback

GRANT = {"cpus": {"0": 1, "1": 0, "2": 2, "3": 0}}


class CpuAssignment(unittest.TestCase):
    def test_matches(self):
        f, rep = readback.compare_cpus([1, 0, 2, 0], GRANT)
        self.assertEqual(f, [])
        self.assertEqual([x["served"] for x in rep], [1, 0, 2, 0])

    def test_granted_cpu_not_run(self):  # dt_check blind test, M01
        f, _ = readback.compare_cpus([1, 0, 0, 0], GRANT)
        self.assertEqual(f, [("CPU 2", "CPU assignment drift", "granted to partition 2, HAVEN runs no partition")])

    def test_cpu_run_for_another_partition(self):
        f, _ = readback.compare_cpus([2, 0, 2, 0], GRANT)
        self.assertEqual(f, [("CPU 0", "CPU assignment drift", "granted to partition 1, HAVEN runs partition 2")])

    def test_cpus_missing_from_grant_mean_none(self):
        f, _ = readback.compare_cpus([0, 0, 0, 1], {"cpus": {"0": 0}})
        self.assertEqual(f, [("CPU 3", "CPU assignment drift", "granted to no partition, HAVEN runs partition 1")])


if __name__ == "__main__":
    unittest.main()
