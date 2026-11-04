# L1 results: PSCI over HVC

**Run:** 2026-11-04, macOS, QEMU virt (cortex-a72, qemu-direct profile),
`run_l0.sh IMAGE runs/l1` with the same kernel Image and device tree as
L0 (SHA-256 in `runs/l1/SHA256SUMS`). HAVEN: this record's commit, which
adds `src/core/exc/psci.c`. Raw output: `runs/l1/uart.log` (261 lines).
Design and expectations: `PLAN.md` section 5, committed before the run
(`e867084`).

## Observations

- Linux made eight PSCI calls, all logged by HAVEN (uart.log lines 40 to
  50 and 260):

  | # | Call | Argument | HAVEN answered |
  | - | - | - | - |
  | 1 | PSCI_VERSION | | 0x10000 (v1.0) |
  | 2 | MIGRATE_INFO_TYPE | | 2 |
  | 3 | PSCI_FEATURES | SMCCC_VERSION 0x80000000 | -1 NOT_SUPPORTED |
  | 4 | PSCI_FEATURES | CPU_SUSPEND 0xc4000001 | 0 |
  | 5 | PSCI_FEATURES | SYSTEM_SUSPEND 0xc400000e | -1 |
  | 6 | PSCI_FEATURES | SYSTEM_RESET2 0xc4000012 | -1 |
  | 7 | PSCI_FEATURES | SYSTEM_OFF2 0xc4000015 | -1 |
  | 8 | SYSTEM_RESET | | partition stopped |

- Linux printed `psci: PSCIv1.0 detected in firmware.`, `Using standard
  PSCI v0.2 function IDs`, `Trusted OS migration not required` and `SMC
  Calling Convention v1.0` (L0: `PSCIv33792.0`, `MIGRATE_INFO_TYPE
  returned unknown type`, `SMC Calling Convention v32768.0`).
- The GIC still fails: one stage-2 data abort at IPA 0x800f000 (GICD
  identification register), then `GICv3: ... no distributor detected,
  giving up` and `arch_timer: No interrupt available, giving up`. All
  timestamps stay at 0.000000.
- Linux panicked at the root mount as in L0, then sent SYSTEM_RESET.
  HAVEN logged `partition stopped by PSCI SYSTEM_RESET; CPU parked`.
  `Reboot failed -- System halted` (last line of L0) no longer appears.

## Against the expectations (PLAN.md section 5)

| # | Expectation | Result |
| - | - | - |
| 1 | the four `psci:` lines | held |
| 2 | call order VERSION, MIGRATE_INFO_TYPE, FEATURES x5 | held |
| 3 | GIC still fails, root-mount panic | held |
| 4 | SYSTEM_RESET stops partition A, HAVEN stays up | held (HAVEN printed its stop line; the run ended at the 30 s timeout) |

## Interpretation (inference)

The PSCI surface Linux v6.18 needs to boot one vCPU is small: eight calls,
five of them feature probes. Saying NOT_SUPPORTED to SMCCC_VERSION keeps
Linux at SMCCC v1.0, so it makes no vendor-hypervisor or firmware
workaround calls through HVC; this keeps the trusted surface at the PSCI
functions listed in `psci.c`. The next blocker is unchanged from L0: the
GIC distributor (L2 MMIO decode, L3 GIC emulation).

## Limits

- SYSTEM_RESET stops the partition instead of restarting it.
- CPU_SUSPEND returns at once; a guest with idle states would spin.
- Only QEMU; the SMC path (EC 0x17, `ELR += 4`) is unit-tested in its
  decision function only and was not exercised by this run.
