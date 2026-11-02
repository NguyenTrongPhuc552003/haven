#!/usr/bin/env python3
"""List the ACPI IORT tables inside a vendor UEFI firmware image.

Usage: iort_scan.py IMAGE [IMAGE ...]

Searches the image as is and inside every LZMA stream ("alone" format, as
EDK2 compresses its firmware volumes) for IORT tables with OEM ID CIXTEK,
and prints each distinct table (by SHA-256) with its nodes: SMMUv3 base and
flags, root-complex and SMMU ID mappings, and named-component paths.
Read-only: the image is never modified. Used for the CIX P1 board ledger
(docs/porting/cix-p1-ledger.md).
"""
import hashlib
import lzma
import re
import struct
import sys

NODE = {0: "ITS", 1: "named component", 2: "root complex", 3: "SMMUv1/2", 4: "SMMUv3", 5: "PMCG", 6: "RMR"}


def describe(d, o):
    """Lines describing the IORT table at offset o (ACPI 6.x IORT layout)."""
    nn, noff = struct.unpack_from("<II", d, o + 36)
    nodes, offs, p = [], {}, o + noff
    for i in range(nn):
        t, ln = d[p], struct.unpack_from("<H", d, p + 1)[0]
        offs[p - o] = i
        nodes.append((p, t, ln))
        if ln == 0:
            break
        p += ln
    out = []
    for i, (p, t, ln) in enumerate(nodes):
        nmap, moff = struct.unpack_from("<II", d, p + 8)
        extra = ""
        if t == 4:
            extra = f"base={struct.unpack_from('<Q', d, p + 16)[0]:#x} flags={struct.unpack_from('<I', d, p + 24)[0]:#x}"
        elif t == 1:
            m = re.search(rb"\\_SB_\.[A-Z0-9_.]+", d[p:p + ln])
            extra = m.group(0).decode() if m else ""
        maps = []
        if t in (2, 4):
            for k in range(nmap):
                ib, n, ob, oref, _ = struct.unpack_from("<IIIII", d, p + moff + 20 * k)
                maps.append(f"ids {ib:#x}+{n:#x} -> {ob:#x} at node {offs.get(oref, '?')}")
        out.append(f"  node {i}: {NODE.get(t, t)} {extra} {'; '.join(maps)}".rstrip())
    return out


def tables(buf, where, seen):
    for m in re.finditer(b"IORT", buf):
        o = m.start()
        length = struct.unpack_from("<I", buf, o + 4)[0]
        if not (48 < length < 0x10000) or buf[o + 10:o + 16] != b"CIXTEK":
            continue
        digest = hashlib.sha256(buf[o:o + length]).hexdigest()
        if digest in seen:
            seen[digest].append(where)
            continue
        seen[digest] = [where]
        print(f"IORT sha256 {digest} ({where}): length {length}, revision {buf[o + 8]}, "
              f"OEM table {buf[o + 16:o + 24].decode(errors='replace')}")
        print("\n".join(describe(buf, o)))


def scan(path):
    d = open(path, "rb").read()
    print(f"== {path} ({len(d)} bytes, sha256 {hashlib.sha256(d).hexdigest()})")
    seen = {}
    tables(d, "uncompressed", seen)
    for m in re.finditer(rb"\x5d\x00\x00[\x00-\xff][\x00-\x04]", d):
        o = m.start()
        size = struct.unpack_from("<Q", d, o + 5)[0]
        if not 0x1000 < size < 0x8000000:
            continue
        try:
            dec = lzma.LZMADecompressor(format=lzma.FORMAT_ALONE).decompress(
                d[o:o + min(len(d) - o, size + 0x400000)], max_length=size)
        except lzma.LZMAError:
            continue
        if b"IORT" in dec:
            tables(dec, f"LZMA stream at {o:#x}", seen)
    print(f"distinct IORT tables: {len(seen)}; copies: {sum(len(v) for v in seen.values())}")


if __name__ == "__main__":
    for image in sys.argv[1:]:
        scan(image)
