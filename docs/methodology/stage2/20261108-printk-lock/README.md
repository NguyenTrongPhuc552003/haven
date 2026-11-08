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

## Results (2026-11-08, QEMU 4 CPUs cortex-a72, TCG)

HAVEN `261f52d` (the lock; expectations and checker in `cbad86e`).
Runs: `ONLY=H1 ../20261108-multi-vcpu/run_smp3.py ...` three times (P1 to
P3), UART logs in `runs/`, then `check_lines.py runs/P*/uart.log`. Each
run is the first of its kind; no code changed between them.

| Row | Expected | Observed | Verdict |
| - | - | - | - |
| P1 to P3 | HAVEN lines whole in all three; marker; both stop | P2 and P3: 38 HAVEN lines each, none broken. P1: 37 HAVEN lines, **one broken**: the "virtual interrupts delivered on CPU 1" line at CPU 1's CPU_OFF has Linux's `[    0.62...] psci: CPU1 killed` text inside it; and one Linux line has HAVEN's "vCPU 1 stopped by PSCI CPU_OFF" inside it (found by eye; the checker only reads lines that hold `HAVEN`). All three reach `KDAL-DEMAND-END` with both stop lines | **prediction error** (P1) |
| HAVEN with HAVEN | (part of the row above) | none in 3 runs: the CPU_OFF line (CPU 1) and the AFFINITY_INFO line (CPU 0), mixed in SMP3, are whole in all three | as intended |
| Linux with HAVEN | counted, not judged | 2 lines in P1 (both above), 0 in P2 and P3 | counted |
| CI checks, host tests | pass | `qemu-direct-check` and `partition-b-check` PASS (cortex-a72 and max), 25 of 25 host tests | as expected |
| Trusted code | about +25 | 7,342 at `4d61d32`, 7,381 at `261f52d`: +39 | measured |

Prediction error (Claude's): the expectation said every report line would
be whole, while the design section itself says that Linux writes to the
same UART and can still mix with HAVEN's lines. The judged rule did not
exclude that case, and the checker cannot tell a Linux insertion from a
HAVEN one inside a line. The inserted text in P1 is Linux's own (its
timestamp and `psci:` prefix), printed by CPU 0 at the moment CPU 1's
HAVEN printed its report (observation); the lock cannot stop a guest
writing to a UART it owns.

Consequence for the console policy: whole lines from both HAVEN and
Linux need separate UARTs, which is the board plan already (UART3 for
HAVEN, G2). This paragraph first said that QEMU's `virt` machine gives
Normal world only one PL011; that was wrong (corrected the same day,
before any further run). QEMU's documentation says `virt` has "Either one
or two PL011 UARTs for the NonSecure World", the second only when a
second `-serial` is given and TrustZone emulation is off
(https://www.qemu.org/docs/master/system/arm/virt). Observed with the local
QEMU 11.1.3 (`dumpdtb` with two `-serial` options): the second PL011 is at
0x09040000 with SPI 8 (INTID 40). So HAVEN's own console could move to
that UART in QEMU too, as on the board (proposal, not done: it changes
the setup every run script and CI check reads, and CI's older Ubuntu
QEMU may not have the second UART). Until then, run scripts must keep
matching HAVEN lines loosely when both CPUs print.
