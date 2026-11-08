# HAVEN's own console in QEMU: the second PL011

**Status:** owner-approved 2026-11-08 (console policy; the board uses
UART3, G2). Design and expectations by Claude, committed before the code.

## Problem (`../20261108-printk-lock/README.md`)

In QEMU, HAVEN prints on PL011 UART0, which partition A owns (its stage-2
maps it, `A UART` in the grants). The line lock keeps HAVEN's lines whole
against each other, not against Linux, which writes to the same UART. QEMU
`virt` gives Normal world a second PL011 (0x09040000, SPI 8) when a second
`-serial` is given and TrustZone emulation is off (QEMU documentation;
observed with QEMU 11.1.3).

## Design

- CMake option `HAVEN_CONSOLE_UART` (0 by default, 1 = the second PL011),
  qemu-virt platform only. `QEMU_UART_BASE` stays partition A's UART0; a
  new `QEMU_CONSOLE_BASE` is HAVEN's console, 0x09000000 or 0x09040000.
  Nothing maps the second PL011 into a partition.
- Default 0: CI (Ubuntu's older QEMU, which may lack the second UART), the
  frozen records and every existing runner keep the old console. With 1,
  QEMU must get two `-serial` options (UART0 first); without the second
  one HAVEN's first write would hit an unmapped address.
- Read-back gets `--haven-uart` (v13): a second `-serial` into
  `haven-uart.log`. Refused together with `--tfa`, where the second PL011
  is Secure world's.

## Expectations

Configuration: the two-vCPU Linux setup of SMP3 (`run_smp3.py` build
options plus `-DHAVEN_CONSOLE_UART=1`, the hotplug init), QEMU 11.1.3.

| Run | Expected |
| - | - |
| C1 to C3: free boot to power-off, three times, two serial logs | the HAVEN log holds only HAVEN's output (every line starts with `HAVEN` or is the boot banner `Haven hypervisor starting`); the Linux log holds no `HAVEN` text; `check_lines.py` finds no broken HAVEN line in any of the three; Linux reaches `KDAL-DEMAND-END`, CPU 1 goes off and on again, both stop lines in the HAVEN log |
| C4: read-back v13 `--haven-uart`, `grant-smp-v12.json`, at `KDAL-DEMAND-END` | matches, as V2 (UART1 is mapped in no partition, so no new finding) |
| Default build (`HAVEN_CONSOLE_UART` unset): CI QEMU checks, host tests, read-back tests | unchanged, pass |
| Trusted code | under +10 code lines (estimate) |

## Results (2026-11-09, QEMU 11.1.3)

HAVEN `30a6577` (option, read-back v13 and runner; expectations in
`b55dd3c`). Command: `run_console.py <workspace>/haven <workspace>/kdal
<Linux v6.18 Image> kdal.ko runs`, one call, each run the first of its
kind. Raw logs in `runs/`.

| Run | Expected | Observed | Verdict |
| - | - | - | - |
| C1 to C3 | HAVEN log only HAVEN's (lines start with `HAVEN` or are the banner `Haven hypervisor starting`); no `HAVEN` in the Linux log; no broken HAVEN line; marker, hotplug, both stops | each run: 38 HAVEN lines, **0 broken**, 0 `HAVEN` in the Linux log, `KDAL-DEMAND-END`, `HOTPLUG-ON ok, online: 0-1`, 2 stop lines. 3 lines per run start with neither: `  Version: 0.6.2`, `  Build: ...`, `  CPU: PARange=4 GIC=v3 - features OK` | intent held; **rule wording wrong** (prediction error) |
| C4 | read-back matches | `RESULT enforced state matches the grant`; CPU table 1/1, 1/1, 2/2, 0/0; `haven-uart.log` captured | as expected |
| Default build | CI checks, host and read-back tests unchanged | `qemu-direct-check` and `partition-b-check` PASS (cortex-a72, max); 26 host tests; read-back tests OK | as expected |
| Trusted code | under +10 | not measured separately (6 code lines in `memory.h` and two changed calls in `platform.c`; the CMake lines are not in the count) | counted by hand |

Prediction error (Claude's): the three lines are HAVEN's own boot banner
(`src/core/init.c` lines 174, 175 and 208, printed with leading spaces),
so the HAVEN log does hold only HAVEN's output, as intended; the rule
named only the banner's first line. The separation itself is complete in
all three runs: no HAVEN text on Linux's UART and no Linux text on
HAVEN's.

Use from now on (proposal): new Linux runs build with
`-DHAVEN_CONSOLE_UART=1` and capture both logs; read-back runs add
`--haven-uart`; CI stays on the default until its QEMU has the second
PL011.
