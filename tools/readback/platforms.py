# SPDX-License-Identifier: Apache-2.0
"""The addresses read-back needs, per platform (docs/methodology/stage2/
20261123-readback-board-path). test_platforms.py checks every value against
the platform's own headers (src/platform/<name>/memory.h and platform.c),
so the table and HAVEN cannot disagree.

  image      HAVEN's image: physical base and size (HAVEN_LOAD_PA, HAVEN_SIZE)
  smmu       the SMMUv3 that HAVEN programs
  gicd       GIC distributor base
  gicr       base of the first GIC redistributor frame (frames found by
             GICR_TYPER affinity from there, as HAVEN finds them)
  mpidr      each logical CPU's MPIDR, in HAVEN's logical order
"""

PLATFORMS = {
    "qemu-virt": {  # QEMU virt, -smp 4 (src/platform/qemu-virt/)
        "image": (0x80000000, 0x800000),
        "smmu": 0x09050000,
        "gicd": 0x08000000,
        "gicr": 0x080A0000,
        "mpidr": [0x0, 0x1, 0x2, 0x3],
    },
    "cix-p1": {  # CIX P1 (Sky1), Orange Pi 6 Plus (src/platform/cix-p1/)
        "image": (0xF0000000, 0x800000),
        "smmu": 0x0B010000,
        "gicd": 0x0E010000,
        "gicr": 0x0E090000,
        "mpidr": [0x400, 0x000, 0x100, 0x200, 0x300, 0x500, 0x600, 0x700, 0x800, 0x900, 0xA00, 0xB00],
    },
}


def mpidr_affinity(mpidr):
    """GICR_TYPER[63:32]'s affinity value for an MPIDR: Aff3.Aff2.Aff1.Aff0."""
    return ((mpidr >> 32) & 0xFF) << 24 | (mpidr & 0xFFFFFF)


def gicr_frames(read_typer, base, mpidrs, limit=64):
    """Each logical CPU's redistributor frame (RD_base) by GICR_TYPER
    affinity: walk from base, 128 KiB per frame, 256 KiB when
    GICR_TYPER.VLPIS (bit 1) is set, until GICR_TYPER.Last (bit 4).
    read_typer(rd) returns the frame's GICR_TYPER, or None when it cannot
    be read (a dump that holds no such frame). A CPU with no frame gets
    None."""
    by_aff, rd = {}, base
    for _ in range(limit):
        typer = read_typer(rd)
        if typer is None:
            break
        by_aff.setdefault(typer >> 32, rd)
        if typer & (1 << 4):
            break
        rd += 0x40000 if typer & (1 << 1) else 0x20000
    return [by_aff.get(mpidr_affinity(m)) for m in mpidrs]
