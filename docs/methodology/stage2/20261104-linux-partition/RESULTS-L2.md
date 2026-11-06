# L2 results: MMIO decode and the full register frame

**Run:** 2026-11-06, macOS, QEMU virt (cortex-a72, qemu-direct profile),
`run_l0.sh IMAGE runs/l2` with the same kernel Image and device tree as
L0 and L1 (SHA-256 in `runs/l2/SHA256SUMS`). HAVEN: this record's commit.
Raw output: `runs/l2/uart.log` (262 lines). Design and expectations:
`PLAN.md` section 6, committed before the run (`090923c`).

## Observations

- Apart from the build timestamp, the only difference from the L1 log is
  one new line after the denied access (uart.log lines 83 and 84):

  ```
  HAVEN: Denied stage-2 violation IPA=0x800f000 FAR=0xffff800082f5ffe8 ESR=0x93880006 cpu=0
  HAVEN: decoded: read 4 bytes, reg w8, IPA=0x800ffe8; reads as zero
  ```

- Linux then printed `GICv3: /interrupt-controller@8000000: no distributor
  detected, giving up` and `arch_timer: No interrupt available, giving up`,
  panicked at the root mount and sent SYSTEM_RESET, which stopped
  partition A, as in L1.
- Host tests: 23 of 23 pass on macOS, including `test_mmio` (decoder,
  frame slots for x0 to x30 and XZR, load extension, region dispatch).
  The Windows (mingw-w64) host build and the ARM64 build are clean.

## Against the expectations (PLAN.md section 6)

| # | Expectation | Result |
| - | - | - |
| 1 | GICD_PIDR2 decoded as a 4-byte read into w8 at IPA 0x800ffe8 | held |
| 2 | Linux still finds no distributor; rest of the boot as in L1 | held |
| 3 | all host tests pass; non-faulting partitions unchanged | held locally (23 of 23); the CI QEMU boot check is in the push's run |

## Interpretation (inference)

The emulation path is in place but nothing is emulated yet, so the guest
sees the same failure; what changed is that HAVEN now knows exactly which
register Linux reads and where the value must go. L3 registers a GIC
device model on this path. The frame now holds every general register,
so a model can serve any load or store with a valid syndrome.

## Limits

- Accesses without a valid syndrome (ISV = 0: load and store pair,
  writeback addressing) cannot be emulated; Linux's MMIO accessors use
  single loads and stores, which have one.
- Only translation faults are emulated (HPFAR is valid for them); a
  permission fault on a mapped page is still denied.
- Regions are keyed by the CPU the partition runs on (one vCPU per
  partition).
