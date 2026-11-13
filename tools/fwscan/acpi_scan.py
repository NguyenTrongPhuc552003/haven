#!/usr/bin/env python3
"""List console and timer ACPI tables (SPCR, DBG2, GTDT) and device-tree blobs
inside a vendor UEFI firmware image.

Usage: acpi_scan.py IMAGE [IMAGE ...]

Searches the image as is and inside every LZMA stream ("alone" format, as
EDK2 compresses its firmware volumes), like iort_scan.py, for tables with OEM
ID CIXTEK and for flattened device trees, and prints each distinct one once
(by SHA-256). Read-only: the image is never modified. Used for
docs/porting/cix-p1-loading.md (section 6).
"""
import hashlib
import lzma
import re
import struct
import sys

OEM = b"CIXTEK"


def streams(d):
    """The raw bytes, then every LZMA stream that decompresses."""
    yield "raw", d
    for m in re.finditer(rb"\x5d\x00\x00[\x00-\xff][\x00-\x04]", d):
        o = m.start()
        size = struct.unpack_from("<Q", d, o + 5)[0]
        # EDK2 records the size (no end marker); size ~0 means "unknown,
        # ends with an end marker", which other encoders write
        if size == 0xFFFFFFFFFFFFFFFF:
            size = 0x8000000
        elif not 0x1000 < size < 0x8000000:
            continue
        try:
            dec = lzma.LZMADecompressor(format=lzma.FORMAT_ALONE).decompress(
                d[o:o + min(len(d) - o, size + 0x400000)], max_length=size)
        except lzma.LZMAError:
            continue
        if len(dec) > 0x1000:
            yield f"lzma@{o:#x}", dec


def gas(b, o):
    """Generic Address Structure: address space and address."""
    sid, _, _, _, addr = struct.unpack_from("<BBBBQ", b, o)
    return f"space {sid} addr {addr:#x}"


def spcr(b, o):
    """ACPI SPCR: interface type, base, interrupt, baud-rate code."""
    gsiv = struct.unpack_from("<I", b, o + 54)[0]
    return (f"interface {b[o + 36]} base [{gas(b, o + 40)}] inttype {b[o + 52]} "
            f"gsiv {gsiv} baud-code {b[o + 58]}")


def dbg2(b, o):
    """ACPI DBG2: port type/subtype and first base address of each device."""
    off, n = struct.unpack_from("<II", b, o + 36)
    out, p = [], o + off
    for _ in range(n):
        ln = struct.unpack_from("<H", b, p + 1)[0]
        ptype, psub = struct.unpack_from("<HH", b, p + 12)
        boff = struct.unpack_from("<H", b, p + 18)[0]
        out.append(f"type {ptype:#x}/{psub:#x} base [{gas(b, p + boff)}]")
        if ln == 0:
            break
        p += ln
    return "; ".join(out)


def gtdt(b, o):
    """ACPI GTDT: GSIVs of the secure EL1, EL1, virtual, EL2 (and virtual EL2) timers."""
    ln = struct.unpack_from("<I", b, o + 4)[0]
    s_el1, _, ns_el1, _, virt, _, ns_el2, _ = struct.unpack_from("<8I", b, o + 48)
    txt = f"secEL1 {s_el1} nsEL1 {ns_el1} virt {virt} nsEL2 {ns_el2}"
    if ln >= 104:
        txt += f" virtEL2 {struct.unpack_from('<I', b, o + 96)[0]}"
    return txt


DESC = {b"SPCR": spcr, b"DBG2": dbg2, b"GTDT": gtdt}


def scan_bytes(d):
    """Lines describing every distinct table and tree found in d."""
    seen, out = set(), []
    for where, b in streams(d):
        for sig, fn in DESC.items():
            for m in re.finditer(sig, b):
                o = m.start()
                if o + 36 > len(b):
                    continue
                ln = struct.unpack_from("<I", b, o + 4)[0]
                if not 36 < ln < 0x4000 or b[o + 10:o + 16] != OEM or o + ln > len(b):
                    continue
                h = hashlib.sha256(b[o:o + ln]).hexdigest()[:12]
                if (sig, h) in seen:
                    continue
                seen.add((sig, h))
                try:
                    txt = fn(b, o)
                except (struct.error, IndexError) as e:
                    txt = f"unparsed ({e})"
                out.append(f"{sig.decode()} {h} rev {b[o + 8]} len {ln} ({where}): {txt}")
        for m in re.finditer(rb"\xd0\x0d\xfe\xed", b):
            o = m.start()
            if o + 24 > len(b):
                continue
            tot, ver = struct.unpack_from(">I", b, o + 4)[0], struct.unpack_from(">I", b, o + 20)[0]
            if 0x100 < tot < 0x200000 and ver in (16, 17) and o + tot <= len(b):
                h = hashlib.sha256(b[o:o + tot]).hexdigest()[:12]
                if ("FDT", h) not in seen:
                    seen.add(("FDT", h))
                    out.append(f"FDT {h} size {tot} ({where} +{o:#x})")
    return out


def main(paths):
    for p in paths:
        d = open(p, "rb").read()
        print(f"== {p} ({len(d)} bytes, sha256 {hashlib.sha256(d).hexdigest()[:16]})")
        for line in scan_bytes(d):
            print("  " + line)


if __name__ == "__main__":
    main(sys.argv[1:])
