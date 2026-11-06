#!/usr/bin/env python3
"""Draw held-out faults for the Linux chain case set at random.

Usage: heldout_random_linux.py <haven-repo> <out-dir> <seed-text> [count]

The owner asked Claude to write the held-out cases (2026-11-06). As for
A1 case set v3, they are not chosen: this reuses v3's sampler
(`../20261102-a1-cases-v3/heldout_random.py`: five single-point mutation
operators, shuffled with a seed fixed before the list exists, first
`count` mutants that build are kept) on the code the Linux partition
depends on, and checks that a mutant builds in the Linux profile.
"""
import importlib.util
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location(
    "v3", os.path.join(HERE, "..", "20261102-a1-cases-v3", "heldout_random.py"))
v3 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(v3)

v3.FILES = [
    "src/core/irq/vgic.c", "src/core/exc/mmio.c", "src/core/exc/psci.c",
    "src/core/exc/el2_exceptions.c", "src/core/partition.c", "src/core/irq/ownership.c",
    "arch/arm64/irq.c",
]


def builds(src, log):
    """Host tests and the Linux-profile image must both build."""
    with open(log, "a") as f:
        for cmd in ("cmake --preset host-tests && cmake --build build-host",
                    "cmake --preset arm64-qemu -DHAVEN_BOOT_PROFILE=qemu-direct "
                    "-DHAVEN_PART_A_DTB_IPA=0x50000000 && cmake --build build"):
            if subprocess.run(cmd, cwd=src, shell=True, stdout=f, stderr=subprocess.STDOUT, env=v3.ENV).returncode:
                return False
    return True


v3.builds = builds

if __name__ == "__main__":
    v3.main()
