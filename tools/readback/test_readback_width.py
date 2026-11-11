"""Read-back v14: widths taken from the image (D5 step 3).

A fake image (bytes from readback.IMG_PA) with a prebuilt symbol list
(haven.nm next to haven.elf, as the try-it bundle has), so no QEMU and no
cross tools: the emulated GIC's width (hv_vgic_nr_irqs) and the vCPU
table's size (hv_psci_max_vcpus), present, absent and impossible.
"""
import atexit
import os
import shutil
import struct
import tempfile
import unittest

import readback

BASE = readback.IMG_PA
BY_CPU, MODEL, WIDTH, TABLE, MAX = 0x1000, 0x2000, 0x3000, 0x4000, 0x5000


def image(width=None, size=None, bits=(), count=0, states=()):
    """The image and the elf path of a fake HAVEN with one vGIC model on
    CPU 0 (enable bits `bits`) and a vCPU table of `size` entries."""
    mem = bytearray(0x6000)
    struct.pack_into("<Q", mem, BY_CPU, BASE + MODEL)
    for b in bits:
        off = MODEL + readback.VGIC_ENABLED_OFFSET + 4 * (b // 32)
        struct.pack_into("<I", mem, off, struct.unpack_from("<I", mem, off)[0] | 1 << (b % 32))
    names = {"by_cpu": BY_CPU, "part_a_vcpus": TABLE}
    if width is not None:
        struct.pack_into("<I", mem, WIDTH, width)
        names["hv_vgic_nr_irqs"] = WIDTH
    if size is not None:
        struct.pack_into("<I", mem, MAX, size)
        names["hv_psci_max_vcpus"] = MAX
    struct.pack_into("<I", mem, TABLE, count)
    for i, s in enumerate(states):
        struct.pack_into("<I", mem, TABLE + 8 + 8 * (size or 4) + 4 * i, s)
    d = tempfile.mkdtemp()
    atexit.register(shutil.rmtree, d, True)
    with open(os.path.join(d, "haven.nm"), "w") as f:
        for n, a in names.items():
            f.write(f"{BASE + a:016x} D {n}\n")
    return bytes(mem), os.path.join(d, "haven.elf")


class VgicWidth(unittest.TestCase):
    def test_absent_symbol_reads_64(self):
        mem, elf = image(bits=(27, 33, 70))
        self.assertEqual(readback.vgic_width(mem, None, elf), 64)
        # INTID 70 lies beyond a 64-ID model: not read, as before v14
        self.assertEqual(readback.read_vgic_enables(mem, None, elf), {0: 1 << 27 | 1 << 33})

    def test_cix_width_shows_intid_330(self):
        mem, elf = image(width=352, bits=(27, 330))
        self.assertEqual(readback.vgic_width(mem, None, elf), 352)
        self.assertEqual(readback.read_vgic_enables(mem, None, elf), {0: 1 << 27 | 1 << 330})

    def test_impossible_width_stops(self):
        for bad in (100, 32, 2048):
            mem, elf = image(width=bad)
            with self.assertRaises(SystemExit):
                readback.read_vgic_enables(mem, None, elf)


class VcpuTable(unittest.TestCase):
    def test_absent_symbol_reads_4_entries_at_40(self):
        mem, elf = image(count=6, states=(0, 1, 1, 2))
        v = readback.read_vcpus(mem, None, elf)
        self.assertEqual(v["count"], 4)  # capped at the table size
        self.assertEqual(v["state"], [0, 1, 1, 2])

    def test_eight_entries_state_at_72(self):
        mem, elf = image(size=8, count=8, states=(0, 1, 1, 1, 1, 1, 1, 2))
        self.assertEqual(readback.vcpu_table_size(mem, None, elf), 8)
        self.assertEqual(readback.read_vcpus(mem, None, elf)["state"], [0, 1, 1, 1, 1, 1, 1, 2])

    def test_impossible_size_stops(self):
        for bad in (0, 17):
            mem, elf = image(size=bad)
            with self.assertRaises(SystemExit):
                readback.read_vcpus(mem, None, elf)


if __name__ == "__main__":
    unittest.main()
