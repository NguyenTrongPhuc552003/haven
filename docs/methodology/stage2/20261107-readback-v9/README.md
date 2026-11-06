# Read-back v9: reading at a breakpoint

**Status:** owner-approved 2026-11-07 ("Approve", item 1: close the
enable-drift timing gap found in read-back v8 run V6). Design and
expectations by Claude, committed before the runs below.

## Problem (`../20261107-readback-v8/RESULTS.md`, section 1)

v8 reads HAVEN's state after a UART line appears. In V6 the line was
`Booting Linux`, but the guest ran about 90 ms further before QEMU
stopped, enabled PPI 27 itself, and hid the H3 fault on that interrupt.
The read point depends on how fast read-back polls the UART file.

## Options considered

| | Breakpoint at the guest's entry (chosen) | HAVEN records each first enable |
| - | - | - |
| Read point | exact: HAVEN has set up the partition, the guest has run no instruction | at the guest's first enable of each interrupt |
| Trusted code | none added; read-back stays outside HAVEN | adds a hook and a record to the vGIC (HAVEN reports on itself) |
| Hardware | needs a debugger (QEMU's GDB stub here; on the board an external debugger, not yet checked for the Orange Pi 6 Plus) | works without a debugger |
| What it sees | anything set before the guest starts (H3); not faults that act on later guest requests (H1) | H3 when the guest asks; also not H1 |

The breakpoint keeps the thesis property that read-back is independent
of HAVEN's own reports. The HAVEN record stays the fallback if the board
has no usable debugger.

## Design

`readback.py --break ADDR`: QEMU starts paused (`-S`) with its GDB stub on
a local socket; read-back speaks the GDB remote protocol itself (no gdb
needed), sets a breakpoint at ADDR, resumes, and reads the usual state
when a CPU stops there. The stop reply and the protocol exchange are kept
in `gdb.json`. Here ADDR is partition A's entry, IPA 0x40000000 (HAVEN
runs at PA 0x80000000, partition B starts at IPA 0x60000000, so no other
code runs at that address).

Mechanism check before this record, on the default demo build (not a
case of this record): the stub accepted the breakpoint, the stop came on
thread 1 (CPU 0), and the UART log ended at HAVEN's `ERET -> partition A
entry` line.

## Runs and expectations (Claude's, before the runs)

`rerun_v9.py` reuses the frozen case set's builder, device trees and
patches and v8's grant (`grant-l4-guest.json`).

| Run | Case | Read at | Expected |
| - | - | - | - |
| W1 | B0 | entry breakpoint | matches; 27 and 33 waiting for the guest |
| W2 | H3 | entry breakpoint | two findings: enable drift on SPI 33 and on CPU 0 PPI 27 (V6 found only SPI 33) |
| W3, W4 | H3 | entry breakpoint | identical to W2 (same findings, same stop) |
| W5 | H1 | entry breakpoint | matches; both waiting (H1 acts only when the guest enables an interrupt, which it has not done yet) |
| W6 | B0 | `KDAL-DEMAND-END` | as V1: matches, 27 and 33 enabled by guest and GIC (marker reads unchanged in v9) |

For every breakpoint run: the stop is on thread 1 (CPU 0), and the UART
log contains no Linux line (`Booting Linux` absent).

What this does not show: faults that act after the guest starts (H1,
missing bindings) still need a later read; W5 checks that the breakpoint
read does not invent findings, not that it catches H1.
