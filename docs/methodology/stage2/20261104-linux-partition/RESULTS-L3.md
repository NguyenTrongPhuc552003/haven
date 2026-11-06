# L3 results: GICv3 emulation, timer and self-IPIs

**Run:** 2026-11-06, macOS, QEMU virt (cortex-a72, qemu-direct profile),
`run_l0.sh IMAGE runs/l3 40` with the same kernel Image and device tree
as L0 to L2 (SHA-256 in `runs/l3/SHA256SUMS`). HAVEN: this record's
commit. Raw output: `runs/l3/uart.log` (269 lines). Design and
expectations: `PLAN.md` section 7, committed before the run (`68d9723`).

## Observations

- HAVEN set up the emulated distributor and redistributor before entering
  partition A (uart.log line 13).
- Linux's GICv3 driver probed (lines 84 to 89): `32 SPIs implemented`,
  `0 Extended SPIs implemented`, `16 PPIs`, `GICD_CTLR.DS=1`,
  `CPU0: found redistributor 0 region 0:0x00000000080a0000`.
- The architected timer registered (line 91): `cp15 timer running at
  62.50MHz (virt)`; `sched_clock: 57 bits at 63MHz` (line 93). Timestamps
  advance from line 93 on and reach 0.384694 s at the panic. Linux
  switched to the `arch_sys_counter` clocksource (line 161).
- The PL011 driver bound (`ttyAMA0 at MMIO 0x9000000 (irq = 13 ...)`,
  line 134) and the console moved from `earlycon` to `ttyAMA0`.
- Linux panicked at the root mount (no root file system, no initramfs:
  `check access for rdinit=/init failed`), then sent SYSTEM_RESET. HAVEN
  reported (line 268):

  ```
  HAVEN: virtual interrupts delivered on CPU 0: 5 x109 27 x68
  ```

  Interrupt 27 is the virtual timer, injected hardware-linked. SGI 5 is a
  self-targeted SGI that Linux wrote to ICC_SGI1R_EL1 (on arm64, SGI 5 is
  Linux's `IPI_IRQ_WORK`; inference from `arch/arm64/kernel/smp.c`), each
  trapped and injected as a virtual SGI. Interrupt 33 (UART) was not
  delivered.
- No access was denied, no other system register access trapped and no
  interrupt was left undelivered (0 matching lines).
- Host tests: 24 of 24 on macOS, including `test_vgic` (probe sequence,
  distributor init writes, enable follows ownership, self-targeted SGIs).
  The Windows host build and the default ARM64 build are clean.

## Against the expectations (PLAN.md section 7)

| # | Expectation | Result |
| - | - | - |
| 1 | GIC probes: 32 SPIs, redistributor for CPU 0 at 0x080a0000 | held |
| 2 | timer registers (virtual), timestamps advance | held |
| 3 | interrupt 27 delivered more than zero times; 33 may stay at zero | held (27: 68; 33: 0) |
| 4 | root-mount panic, SYSTEM_RESET stops A, no denied access | held |
| 5 | host tests pass on three CI systems, qemu-psci unchanged | held locally; CI result in the push's run |

Not predicted: the 109 self-IPIs. The plan said a trapped SGI write would
otherwise loop forever, but did not predict how many Linux sends at boot.

## Interpretation (inference)

Linux v6.18 runs as a HAVEN partition with working interrupts on one
vCPU: the distributor and redistributor model (`src/core/irq/vgic.c`,
pure, host-tested) and the existing hardware-linked injection are enough
for the GIC driver, the timer and Linux's self-IPIs. Enable writes reach
the physical GIC only for interrupts the partition owns on its CPU, so
the model cannot be used to switch on another partition's interrupt;
this is a property read-back can check (the GIC's enable and routing
state against the grant) once L4 adds the read-back run.

The remaining blocker is the root file system: L4 adds an initramfs with
a shell and KDAL.

## Limits

- Priority, configuration, group and routing writes are stored, not
  applied to the hardware; injection keeps priority 0xa0.
- Pending and active state are not emulated (read as zero); a driver that
  sets an interrupt pending through the distributor will not see it.
- One vCPU; SGIs to other CPUs are dropped. 64 interrupt IDs.
- The emulation adds to HAVEN's trusted code: `vgic.c` (about 250 lines),
  `mmio.c` (about 140 lines) and `psci.c` (about 130 lines). This needs
  measuring against the "without growing HAVEN's trusted code" part of
  the research question.
