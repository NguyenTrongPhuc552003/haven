#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check the UART logs of run_n.py (Linux in partition A with N vCPUs).

Usage: linux-partition-check.py OUT
OUT is run_n.py's output directory (OUT/N2/uart.log, OUT/N3/uart.log).
For each run: Linux brought up N CPUs, KDAL printed its demand
(KDAL-DEMAND-END), HAVEN started every vCPU on its CPU, the partition
stopped by PSCI OFF and partition B started; and no interrupt went
undelivered, no EL2 fatal error, no CPU failed to come online.
Exits 1 on the first run that misses a marker
(docs/methodology/stage2/20261112-e4-ci-linux).
"""
import os
import sys

# qemu-virt pinning (src/platform/qemu-virt/memory.h): vCPU 1 on CPU 1,
# vCPU 2 on CPU 3 (CPU 2 is partition B's)
VCPU_CPU = {1: 1, 2: 3}
FORBIDDEN = ("not delivered", "EL2 FATAL", "failed to come online")


def markers(n):
    m = [f"smp: Brought up 1 node, {n} CPUs", "KDAL-DEMAND-END"]
    m += [f"CPU {VCPU_CPU[i]} ready, partition A vCPU {i} started" for i in range(1, n)]
    return m + ["partition stopped by PSCI OFF", "partition B started"]


def check(out, n):
    """Missing markers and forbidden lines of run N, or None if no log."""
    path = os.path.join(out, f"N{n}", "uart.log")
    if not os.path.exists(path):
        return None
    text = open(path, errors="replace").read()
    bad = [f"missing: {m}" for m in markers(n) if m not in text]
    bad += [f"found: {f}" for f in FORBIDDEN if f in text]
    banner = next((line.strip() for line in text.splitlines() if "Linux version" in line), "no banner")
    print(f"N = {n}: {banner[:100]}")
    return bad


def main():
    out = sys.argv[1]
    for n in (2, 3):
        bad = check(out, n)
        if bad is None:
            bad = [f"no log {os.path.join(out, f'N{n}', 'uart.log')}"]
        if bad:
            print(f"N = {n}: FAIL")
            for b in bad:
                print(f"  {b}")
            sys.exit(1)
        print(f"N = {n}: OK ({len(markers(n))} markers)")


if __name__ == "__main__":
    main()
