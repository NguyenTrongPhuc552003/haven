"""dt_check.py: a guest's device tree against its grant (DtCheck needs dtc;
BoardTrees reads D3's committed blobs and runs without it)."""
import os
import shutil
import tempfile
import unittest

import dt_check
import grant as grant_file

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

    # D4 (docs/methodology/stage2/20261109-cix-p1-platform): D3 findings 1 to 4
    def test_four_cell_specifiers(self):  # D3 finding 3
        text = (dts().replace("#interrupt-cells = <3>", "#interrupt-cells = <4>")
                .replace("<1 13 4>, <1 14 4>, <1 11 4>, <1 10 4>", "<1 13 4 0>, <1 14 4 0>, <1 11 4 0>, <1 10 4 0>")
                .replace("<0 1 4>", "<0 1 4 0>"))
        f, info = self.run_check(text)
        self.assertEqual(f, [])
        self.assertEqual(len(info), 3)

    def test_unreadable_width(self):
        f, info = self.run_check(dts().replace("#interrupt-cells = <3>", "#interrupt-cells = <2>")
                                 .replace("<0 1 4>", "<0 1>"))
        why = "interrupt specifiers not readable: the parent's #interrupt-cells is 2 (3 or 4 expected)"
        self.assertEqual(f, [f"/timer: {why}", f"/pl011@9000000: {why}"])
        self.assertEqual(info, [])

    def test_length_not_a_multiple_of_width(self):
        f, _ = self.run_check(dts().replace("<0 1 4>", "<0 1 4 0>"))
        self.assertEqual(f, ["/pl011@9000000: interrupt specifiers not readable: "
                             "4 cells is not a multiple of #interrupt-cells 3"])

    def test_psci_method(self):  # D3 finding 4: information only
        f, info = self.run_check(dts(extra='psci { compatible = "arm,psci-1.0"; method = "smc"; };'))
        self.assertEqual(f, [])
        self.assertIn('/psci: method "smc"; HAVEN serves PSCI to partitions over HVC', info)
        f, info = self.run_check(dts(extra='psci { compatible = "arm,psci-1.0"; method = "hvc"; };'))
        self.assertEqual(len(info), 3)
        f, info = self.run_check(dts(cpus='cpu@0 { device_type = "cpu"; reg = <0>; enable-method = "psci"; };'))
        self.assertIn('/psci: none, but cpu nodes use enable-method "psci"', info)

    def test_cpu_identity(self):  # D3 finding 2
        g = dict(GRANT, gic=dict(GRANT["gic"], mpidr={"0": "0x0", "1": "0x1"}))
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, "t.dts")
            with open(p, "w") as fh:
                fh.write(dts(cpus='cpu@1 { device_type = "cpu"; reg = <1>; };'))
            f, _ = dt_check.check(g, dt_check.load_dt(p), 1, EMULATED)
        self.assertEqual(f, ["/cpus/cpu@1: MPIDR 0x1 is not a CPU of partition 1"])

    def test_beyond_emulated_gic(self):  # D3 finding 1
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, "t.dts")
            with open(p, "w") as fh:
                fh.write(dts())
            root = dt_check.load_dt(p)
        self.assertEqual(dt_check.check(GRANT, root, 1, EMULATED, 64)[0], [])
        self.assertEqual(dt_check.check(GRANT, root, 1, EMULATED, 32)[0],
                         ["/pl011@9000000: SPI 33 beyond the emulated GIC (32 interrupt IDs)"])


# D3's committed board trees (blobs, so no dtc needed: these run in CI)
D3 = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "docs", "methodology", "stage2",
                  "20261109-board-partition-tree")
BOARD_EMULATED = [(0x0E010000, 0x10000), (0x0E090000, 0x100000)]
CORES = [4, 0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 11]  # logical CPU -> core, core 4 first


class BoardTrees(unittest.TestCase):
    def run_d3(self, tree, mpidr=False, vgic=None):
        g = grant_file.load(os.path.join(D3, "grant-cixp1-v0.json"))
        if mpidr:
            g["gic"]["mpidr"] = {str(i): hex(c << 8) for i, c in enumerate(CORES)}
        root = dt_check.load_dt(os.path.join(D3, tree))
        return dt_check.check(g, root, 1, BOARD_EMULATED, vgic)

    def test_clean_tree(self):
        f, info = self.run_d3("partition-a.dtb", mpidr=True, vgic=352)
        self.assertEqual((f, len(info)), ([], 3))

    def test_clean_tree_beyond_64_ids(self):  # D3 finding 1
        f, _ = self.run_d3("partition-a.dtb", mpidr=True, vgic=64)
        self.assertEqual(f, ["/soc@0/serial@40d0000: SPI 330 beyond the emulated GIC (64 interrupt IDs)"])

    def test_s2b_core_of_partition_b(self):  # D3 finding 2
        self.assertEqual(self.run_d3("seeds/partition-a-S2b.dtb")[0], [])  # no MPIDRs in the grant: as D3
        f, _ = self.run_d3("seeds/partition-a-S2b.dtb", mpidr=True)
        self.assertEqual(f, ["/cpus/cpu@800: MPIDR 0x800 is not a CPU of partition 1"])

    def test_s5_four_cells(self):  # D3 finding 3: no more false PPI 16
        f, info = self.run_d3("seeds/partition-a-S5.dtb")
        self.assertEqual((f, len(info)), ([], 3))

    def test_s6_psci_smc(self):  # D3 finding 4
        f, info = self.run_d3("seeds/partition-a-S6.dtb")
        self.assertEqual(f, [])
        self.assertEqual(info[-1], '/psci: method "smc"; HAVEN serves PSCI to partitions over HVC')


class Spans(unittest.TestCase):
    def test_inside_union(self):
        self.assertTrue(dt_check.inside(0x1000, 0x2000, [(0x0, 0x2000), (0x2000, 0x2000)]))
        self.assertFalse(dt_check.inside(0x1000, 0x2000, [(0x0, 0x2000)]))
        self.assertFalse(dt_check.inside(0x1000, 0x10, [(0x2000, 0x1000)]))


if __name__ == "__main__":
    unittest.main()
