"""Unit tests for acpi_scan.py on small tables built here (no vendor image)."""
import lzma
import os
import struct
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import acpi_scan  # noqa: E402


def header(sig, length, rev):
    """36-byte ACPI header with OEM ID CIXTEK (checksum not checked)."""
    return sig + struct.pack("<IBB", length, rev, 0) + b"CIXTEK" + b"SKY1EDK2" + bytes(12)


def gas(addr):
    return struct.pack("<BBBBQ", 0, 32, 0, 3, addr)


def spcr(base, gsiv):
    body = bytes([3, 0, 0, 0]) + gas(base) + bytes([8, 0]) + struct.pack("<I", gsiv) + bytes([7])
    body += bytes(80 - 36 - len(body))
    return header(b"SPCR", 80, 2) + body


def dbg2(base):
    dev = bytearray(22)
    struct.pack_into("<BHBHHHHHHHHH", dev, 0, 0, 22 + 12 + 4, 1, 0, 0, 0, 0,
                     0x8000, 0x3, 0, 22, 34)
    dev += gas(base) + struct.pack("<I", 0x1000)
    return header(b"DBG2", 44 + len(dev), 0) + struct.pack("<II", 44, 1) + bytes(dev)


def gtdt(timers):
    body = bytearray(132 - 36)
    struct.pack_into("<Q", body, 0, 0)
    for i, g in enumerate(timers[:4]):
        struct.pack_into("<I", body, 12 + 8 * i, g)
    struct.pack_into("<I", body, 60, timers[4])
    return header(b"GTDT", 132, 3) + bytes(body)


def fdt():
    blob = bytearray(0x200)
    struct.pack_into(">II", blob, 0, 0xD00DFEED, len(blob))
    struct.pack_into(">I", blob, 20, 17)
    return bytes(blob)


class AcpiScanTest(unittest.TestCase):
    def test_raw_tables(self):
        img = bytes(64) + spcr(0x040E0000, 331) + bytes(16) + dbg2(0x040D0000) + gtdt([29, 30, 27, 26, 28])
        lines = acpi_scan.scan_bytes(img)
        self.assertEqual(len(lines), 3)
        self.assertIn("interface 3 base [space 0 addr 0x40e0000] inttype 8 gsiv 331 baud-code 7", lines[0])
        self.assertIn("type 0x8000/0x3 base [space 0 addr 0x40d0000]", lines[1])
        self.assertIn("secEL1 29 nsEL1 30 virt 27 nsEL2 26 virtEL2 28", lines[2])

    def test_lzma_stream_and_dedup(self):
        payload = spcr(0x040E0000, 331) + bytes(0x2000) + fdt()
        # Python writes the size as unknown and ends with an end marker. Patching
        # the size in (as EDK2 records it) gives a stream that the macOS and Linux
        # jobs accept but the Windows job rejects ("Corrupt input
        # data", 2026-11-13), so the test keeps the unknown-size form.
        packed = lzma.compress(payload, format=lzma.FORMAT_ALONE)
        img = bytes(32) + packed + bytes(32) + packed
        lines = acpi_scan.scan_bytes(img)
        self.assertEqual(len(lines), 2, lines)
        self.assertTrue(lines[0].startswith("SPCR ") and "(lzma@0x20)" in lines[0])
        self.assertTrue(lines[1].startswith("FDT ") and "size 512" in lines[1])

    def test_other_oem_ignored(self):
        t = bytearray(spcr(0x040E0000, 331))
        t[10:16] = b"OTHER!"
        self.assertEqual(acpi_scan.scan_bytes(bytes(t)), [])


if __name__ == "__main__":
    unittest.main()
