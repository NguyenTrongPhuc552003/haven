#!/usr/bin/env python3
"""linux2.py's verdicts from recorded logs (row S1 of the campaign's s2/README.md)."""

import os
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import linux2  # noqa: E402

FIXTURE = os.path.join(linux2.SMP, "runs-smp3", "H1", "uart.log")


def fixture():
    with open(FIXTURE, errors="replace") as f:
        return f.read()


def demand_check(text):
    """demand_check.py on TEXT's demand, as linux2.methods runs it."""
    with tempfile.TemporaryDirectory() as d:
        path = os.path.join(d, "demand.json")
        with open(path, "w") as f:
            f.write(linux2.demand(text) + "\n")
        r = subprocess.run([sys.executable, os.path.join(linux2.REPO, "tools/readback/demand_check.py"),
                            linux2.GRANT, path, "--partition", "1"], capture_output=True, text=True)
    return linux2.m5_verdict(r.returncode, r.stdout)


class M2(unittest.TestCase):
    def test_recorded_run_passes(self):
        self.assertEqual(linux2.m2_verdict(fixture())[0], "pass")

    def test_missing_hotplug_marker(self):
        v, ev = linux2.m2_verdict(fixture().replace("HOTPLUG-ON ok", "HOTPLUG-ON failed"))
        self.assertEqual(v, "finding")
        self.assertIn("missing: HOTPLUG-ON ok", ev)

    def test_forbidden_line(self):
        self.assertEqual(linux2.m2_verdict(fixture() + "HAVEN: EL2 FATAL synchronous exception\n")[0], "finding")

    def test_no_second_vcpu(self):
        text = fixture().replace("partition A vCPU 1 started", "partition A vCPU 1 not started")
        self.assertEqual(linux2.m2_verdict(text)[0], "finding")


class M2ThreeVcpus(unittest.TestCase):  # linux3, 20261128-readback-n3/
    def test_a_two_vcpu_run_is_a_finding_for_three(self):
        v, ev = linux2.m2_verdict(fixture(), 3)
        self.assertEqual(v, "finding")
        self.assertTrue(any("missing" in e for e in ev))

    def test_two_vcpu_evidence_unchanged(self):
        self.assertEqual(linux2.m2_verdict(fixture())[1],
                         ["Linux, two vCPUs, hotplug, demand, power-off, B started"])


class Tree(unittest.TestCase):
    def test_three_cpu_nodes_and_frames(self):
        smp1 = linux2._smp1()
        base = smp1.rc.dts(smp1.rc.CASES["B0"][2], linux2.IPA, linux2.IPA + 0x1000)
        t = linux2._run_n().tree(base, 3)
        self.assertEqual(t.count('device_type = "cpu"'), 3)
        self.assertIn("GICR, 3 CPUs", t)


class M5(unittest.TestCase):
    def test_recorded_demand_is_the_baseline(self):
        self.assertEqual(demand_check(fixture())[0], "pass")

    def test_a_new_demand_is_a_finding(self):
        text = fixture().replace('"interrupts": [{"id": 39}]', '"interrupts": [{"id": 39}, {"id": 40}]')
        v, ev = demand_check(text)
        self.assertEqual(v, "finding")
        self.assertTrue(any(e.startswith("new: ") and "INTID 40" in e for e in ev), ev)

    def test_a_vanished_finding_is_a_finding(self):
        v, ev = linux2.m5_verdict(1, "  FINDING " + linux2.KNOWN_DEMAND[0] + "\n")
        self.assertEqual(v, "finding")
        self.assertEqual(ev, ["no longer reported: " + linux2.KNOWN_DEMAND[1]])

    def test_kernel_lines_inside_the_demand(self):
        """R49 of the first run: the other vCPU's log inside the block."""
        text = fixture().replace('"vocabulary"', '\n[    4.049455] kdal: gpu driver registered\n"vocabulary"', 1)
        self.assertNotIn("kdal: gpu", linux2.demand(text))
        self.assertEqual(demand_check(text)[0], "pass")

    def test_no_demand(self):
        self.assertIsNone(linux2.demand("Linux booted, no KDAL\n"))

    def test_tool_error(self):
        self.assertEqual(linux2.m5_verdict(2, "")[0], "error")


if __name__ == "__main__":
    unittest.main()
