"""Read-back v19 (docs/methodology/stage2/20261128-readback-v19/, row V1):
HAVEN's interrupt ownership table against the grant."""
import unittest

import readback

GRANT = {"cpus": {"0": 1, "1": 1, "2": 2, "3": 1}, "ppis": {"0": [27], "1": [27], "2": [27]},
         "guest_enabled": {"0": [27, 33], "3": [27]}, "haven_ppis": {"0": [26]},
         "spis": {"33": {"partition": 1, "cpu": 0}, "106": {"partition": 0, "cpu": 0}}}


def table(**changes):
    own = {"ppis": {0: {27: 1}, 1: {27: 1}, 2: {27: 2}, 3: {27: 1}}, "spis": {33: (1, 0)}}
    for k, v in changes.items():
        own[k] = v
    return own


def found(own):
    f, _ = readback.compare_ownership(own, GRANT)
    return [(x[0], x[1]) for x in f]


class Ownership(unittest.TestCase):
    def test_a_matching_table_has_no_finding(self):
        self.assertEqual(found(table()), [])

    def test_n3_w1_a_missing_ppi_route(self):  # vCPU 2's timer on CPU 3
        own = table(ppis={0: {27: 1}, 1: {27: 1}, 2: {27: 2}})
        self.assertEqual(found(own), [("CPU 3 PPI 27", "missing interrupt binding")])

    def test_a_ppi_held_for_the_wrong_partition(self):
        own = table(ppis={0: {27: 1}, 1: {27: 2}, 2: {27: 2}, 3: {27: 1}})
        self.assertEqual(found(own), [("CPU 1 PPI 27", "missing interrupt binding")])

    def test_a_ppi_the_grant_does_not_give(self):
        own = table(ppis={0: {27: 1, 30: 1}, 1: {27: 1}, 2: {27: 2}, 3: {27: 1}})
        self.assertEqual(found(own), [("CPU 0 PPI 30", "interrupt ownership drift")])

    def test_spi_wrong_owner_wrong_target_and_extra(self):
        self.assertEqual(found(table(spis={33: (2, 0)})), [("SPI 33", "missing interrupt binding")])
        self.assertEqual(found(table(spis={33: (1, 3)})), [("SPI 33", "interrupt routing drift")])
        self.assertEqual(found(table(spis={33: (1, 0), 40: (1, 0)})), [("SPI 40", "interrupt ownership drift")])
        self.assertEqual(found(table(spis={})), [("SPI 33", "missing interrupt binding")])

    def test_havens_own_interrupts_given_to_a_partition_are_drift(self):
        # SPI 106 is HAVEN's (partition 0): not demanded from the table, but a
        # table entry giving it to a partition is drift (review of 2026-11-28)
        self.assertEqual(found(table(spis={33: (1, 0), 106: (1, 0)})), [("SPI 106", "interrupt ownership drift")])
        # PPI 26 (haven_ppis) held for a partition: drift as well
        own = table(ppis={0: {26: 1, 27: 1}, 1: {27: 1}, 2: {27: 2}, 3: {27: 1}})
        self.assertEqual(found(own), [("CPU 0 PPI 26", "interrupt ownership drift")])

    def test_special_intids_are_not_read(self):
        import struct
        mem = bytearray(readback.IMG_SIZE)
        base = readback.IMG_PA + 0x1000
        syms = {"ppi_owner_map": base, "irq_owner_map": base + 0x1000, "irq_target_cpu_map": base + 0x2000}
        for i in (1019, 1020, 1023):
            struct.pack_into("<I", mem, syms["irq_owner_map"] - readback.IMG_PA + 4 * i, 1)
        orig = readback.symbol
        readback.symbol = lambda nm, elf, name: syms[name]
        try:
            own = readback.read_ownership(bytes(mem), None, "haven.elf")
        finally:
            readback.symbol = orig
        self.assertEqual(sorted(own["spis"]), [1019])

    def test_read_from_an_image(self):
        import struct
        mem = bytearray(readback.IMG_SIZE)
        base = readback.IMG_PA + 0x1000
        syms = {"ppi_owner_map": base, "irq_owner_map": base + 0x1000, "irq_target_cpu_map": base + 0x2000}
        struct.pack_into("<I", mem, syms["ppi_owner_map"] - readback.IMG_PA + 4 * (16 * 3 + 11), 1)  # CPU 3 PPI 27
        struct.pack_into("<I", mem, syms["irq_owner_map"] - readback.IMG_PA + 4 * 33, 1)
        struct.pack_into("<I", mem, syms["irq_target_cpu_map"] - readback.IMG_PA + 4 * 33, 0)
        orig = readback.symbol
        readback.symbol = lambda nm, elf, name: syms[name]
        try:
            own = readback.read_ownership(bytes(mem), None, "haven.elf")
        finally:
            readback.symbol = orig
        self.assertEqual(own, {"ppis": {3: {27: 1}}, "spis": {33: (1, 0)}})

    def test_an_image_without_the_table(self):
        def missing(nm, elf, name):
            raise SystemExit("not found")
        orig = readback.symbol
        readback.symbol = missing
        try:
            self.assertIsNone(readback.read_ownership(b"", None, "haven.elf"))
        finally:
            readback.symbol = orig


if __name__ == "__main__":
    unittest.main()
