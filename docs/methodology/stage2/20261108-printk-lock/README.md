# HAVEN's log lines from several CPUs: a line lock in hv_printk

**Status:** owner-approved 2026-11-08 ("Approve", the first step of the
console-policy open item). Design and expectations by Claude, committed
before the code.

## Problem (`../20261108-multi-vcpu/README.md`, SMP3 results)

`hv_printk` writes each character to the UART as it formats it, with no
lock. In SMP3's run H1, CPU 1's CPU_OFF line and CPU 0's AFFINITY_INFO
line, printed at the same moment, came out mixed character by character.
Several of HAVEN's messages are also built from more than one
`hv_printk` call (the "virtual interrupts delivered" report prints its
pairs one call at a time), so a lock per call would not keep those lines
whole.

## Design

A **line lock**: the calling CPU takes the lock (owner word = CPU index
+ 1) before its first character and keeps it until it has printed a
newline, so a line built from several calls stays whole. A CPU that
already holds it (a line in progress, or a fault while printing) does
not wait. A CPU that cannot get it within 50 ms prints anyway: a CPU
that parked in the middle of a line, or a very slow UART, then costs
interleaving instead of a hang. The lock is used only once HAVEN's EL2
stage-1 translation is on (`hv_el2_ttbr` non-zero): before that HAVEN's
memory is Device memory, where exclusive loads and stores need not work,
and only the boot CPU runs.

Not covered: partition A's Linux writes to the same PL011 directly (the
UART is passed through), so HAVEN lines and Linux lines can still mix.
Only HAVEN-with-HAVEN mixing is addressed.

## Expectations

| Run | Expected |
| - | - |
| P1 to P3: SMP3's H1 (Linux on two vCPUs, CPU 1 off and on again, power-off), three times, HAVEN at the lock commit | every line holding `HAVEN:` holds it once; every "virtual interrupts delivered on CPU n:" line is whole (only `INTID xCOUNT` pairs after it); the PSCI lines are whole; Linux still reaches `KDAL-DEMAND-END` and both vCPUs stop |
| Mixing of a HAVEN line with a Linux line | counted, not judged (not addressed by this change) |
| CI checks (`qemu-direct-check.sh`, `partition-b-check.py`), host tests | pass (host tests do not build `printk.c`) |
| Trusted code | about +25 code lines (estimate) |

Checker: `check_lines.py` (this directory) applies the first row's rules
to a UART log. Applied before the change to SMP3's H1 log, it should
report the broken line seen there (a check of the checker, not a
result).
