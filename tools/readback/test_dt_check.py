"""dt_check.py: a guest's device tree against its grant (needs dtc)."""
import os
import shutil
import tempfile
import unittest

import dt_check

GRANT = {
    "partitions": {"1": [
        {"name": "RAM", "ipa": "0x40000000", "pa": "0x80800000", "size": "0x20000000", "attrs": "RWX"},
        {"name": "UART", "ipa": "0x09000000", "pa": "0x09000000", "size": "0x1000", "attrs": "DEVICE_RW"},
    ]},
    "gic": {"cpus": {"0": 1, "1": 0}, "spis": {"33": {"partition": 1, "cpu": 0}}, "ppis": {"0": [27]}},
}
EMULATED = [(0x08000000, 0x10000), (0x080A0000, 0x20000)]

BASE = """/dts-v1/;
/ {
	#address-cells = <2>; #size-cells = <2>; interrupt-parent = <&gic>;
	chosen { linux,initrd-start = <0x0 0x48000000>; linux,initrd-end = <0x0 0x48100000>; };
	memory@40000000 { device_type = "memory"; reg = <0x0 0x40000000 0x0 MEMSIZE>; };
	cpus { #address-cells = <1>; #size-cells = <0>; CPUS };
	gic: interrupt-controller@8000000 { compatible = "arm,gic-v3"; #interrupt-cells = <3>;
		interrupt-controller; reg = <0x0 0x08000000 0x0 0x10000>, <0x0 0x080a0000 0x0 0x20000>; };
	timer { compatible = "arm,armv8-timer"; interrupts = <1 13 4>, <1 14 4>, <1 VTIMER 4>, <1 10 4>; };
	pl011@9000000 { compatible = "arm,pl011"; reg = <0x0 0x09000000 0x0 0x1000>; interrupts = <0 1 4>; };
	EXTRA
};
"""


def dts(memsize="0x20000000", vtimer="11", extra="", cpus='cpu@0 { device_type = "cpu"; reg = <0>; };'):
    return (BASE.replace("MEMSIZE", memsize).replace("VTIMER", vtimer)
            .replace("EXTRA", extra).replace("CPUS", cpus))


@unittest.skipUnless(shutil.which("dtc"), "dtc not installed")
class DtCheck(unittest.TestCase):
    def run_check(self, text):
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, "t.dts")
            with open(p, "w") as f:
                f.write(text)
            return dt_check.check(GRANT, dt_check.load_dt(p), 1, EMULATED)

    def test_matching_tree(self):
        f, info = self.run_check(dts())
        self.assertEqual(f, [])
        self.assertEqual(len(info), 3)  # the three timer interrupts an EL1 guest does not use

    def test_memory_larger_than_granted(self):  # case T4
        f, _ = self.run_check(dts(memsize="0x40000000"))
        self.assertEqual(f, ["/memory@40000000: memory 0x40000000+0x40000000 not granted to partition 1"])

    def test_virtual_timer_not_granted(self):  # case T5
        f, _ = self.run_check(dts(vtimer="12"))
        self.assertEqual(f, ["/timer: PPI 28 not granted to partition 1"])

    def test_device_not_granted(self):  # L4's KDAL node
        f, _ = self.run_check(dts(extra='gpio@9030000 { compatible = "kdal,device"; '
                                        'reg = <0x0 0x09030000 0x0 0x1000>; interrupts = <0 7 4>; };'))
        self.assertEqual(f, ["/gpio@9030000: registers 0x9030000+0x1000 not granted to partition 1",
                             "/gpio@9030000: SPI 39 not granted to partition 1"])

    def test_disabled_device_is_ignored(self):
        f, _ = self.run_check(dts(extra='gpio@9030000 { compatible = "x"; status = "disabled"; '
                                        'reg = <0x0 0x09030000 0x0 0x1000>; };'))
        self.assertEqual(f, [])

    def test_more_cpus_than_granted(self):
        f, _ = self.run_check(dts(cpus='cpu@0 { device_type = "cpu"; reg = <0>; }; '
                                       'cpu@1 { device_type = "cpu"; reg = <1>; };'))
        self.assertEqual(f, ["/cpus: 2 CPU nodes, partition 1 has 1 CPU(s)"])


class Spans(unittest.TestCase):
    def test_inside_union(self):
        self.assertTrue(dt_check.inside(0x1000, 0x2000, [(0x0, 0x2000), (0x2000, 0x2000)]))
        self.assertFalse(dt_check.inside(0x1000, 0x2000, [(0x0, 0x2000)]))
        self.assertFalse(dt_check.inside(0x1000, 0x10, [(0x2000, 0x1000)]))


if __name__ == "__main__":
    unittest.main()
