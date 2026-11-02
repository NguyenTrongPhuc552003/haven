# Partition B on CPU 2: boot profile, per-CPU setup, own timer interrupt

**Status:** EXPERIMENT, observed 2026-11-02. Work approved by the owner
("finish partition B", 2026-11-02). Checks and expected outcomes by Claude,
fixed in `run_partb.py` (commit `4c740a5`) before the first run; not blind.
The grant `grant-partb.json` is a proposal pending owner approval.

## Problem

Partition B is configured (stage-2 tables, budget) but had never been shown
running. The `qemu-direct` profile has no PSCI provider, so CPU 2 never
starts; under the `qemu-tfa` build, HAVEN logged "CPU 2 woken for partition
B", yet no stage-2 denial from B ever appeared.

## Cause (experimental observation)

A `qemu-tfa`-conduit build run in the direct QEMU setup, with `-d int`
(2026-11-02, before the fix):

```
Taking exception 13 [Secure Monitor Call] on CPU 0 ... handled as PSCI call
Illegal exception return at EL2: resuming execution at 0x60000000
Taking exception 1 [Undefined Instruction] on CPU 2
...from EL2 to EL2 ... with ELR 0x60000000 ... to EL2 PC 0x200
Taking exception 1 [Undefined Instruction] on CPU 2   (repeats at ELR 0x200)
```

CPU 2 entered `partition_b_secondary_cpu_init` with HCR_EL2 at its reset
value. HCR_EL2.RW = 0 makes an ERET to AArch64 EL1 an illegal exception
return (PSTATE.IL set, so the first instruction is undefined); VBAR_EL2 = 0
then sent every exception to address 0x200, which is not code. Partition B
never ran one instruction. This confirms FINDINGS B2 (secondary entry omits
per-CPU initialization) by observation rather than by source alone.

## Changes (haven `9102099`)

| Area | Change |
| - | - |
| Boot profile | `qemu-psci`: QEMU loads HAVEN at EL2 and answers PSCI over SMC itself (virt machine, virtualization on, secure off, no firmware), so no TF-A is needed to start CPU 2. `qemu-direct` is unchanged |
| B2 | `partition_b_secondary_cpu_init` runs `hv_arch_cpu_init()` (HCR_EL2, VTCR_EL2, SCTLR_EL2, VBAR_EL2, ICC_SRE_EL2, ICH_HCR/VMCR) and `hv_arch_phys_irq_cpu_init()` (EOImode 1, PMR, Group 1) before stage-2 activation |
| T1 | PPIs are owned per (INTID, CPU) and enabled or disabled in that CPU's redistributor (`gic_v3_enable_ppi(cpu, irq)`); `gic_v3_enable_irq` takes SPIs only and no longer reaches CPU 0 for a PPI. SGIs cannot be assigned. An undeliverable PPI is disabled on its CPU |
| Partition B | Owns PPI 27 (EL1 virtual timer) on CPU 2. Guest B programs a 1 ms timer, takes 8 interrupts through the virtual CPU interface and records its progress in `guest_b_status` (its own RAM). The old 0xDEADBEEF sentinel store at IPA 0x60000100 was removed: it landed inside B's own code once the image passed 256 bytes |
| Evidence | `scripts/qemu/partition-b-check.py` boots the image and reads B's status record over QMP; stage-2 denial lines now name the CPU |

## Results (`run_partb.py` at `4c740a5`, `runs/results.md`; both CPUs identical, 8 of 8 as expected)

| Check | Change | Partition B check | Read-back, proposed grant | Read-back, v3 grant |
| - | - | - | - | - |
| K1 | none | PASS: B's read of A's IPA denied on CPU 2, stage 5, 8 timer interrupts, none other | matches | **CPU 2 PPIs: enable drift** (PPI 27 is the new grant) |
| K2 | no per-CPU setup on CPU 2 (B2 back) | FAIL: B status all zero, no denial from CPU 2 | matches | CPU 2 PPI drift only |
| K3 | PPI 27 on CPU 2 owned by A | FAIL: 0 timer interrupts; HAVEN "PPI 27 on CPU 2 not delivered, disabled" | matches | matches |
| K4 | PPI 27 granted on CPU 0 | FAIL: 0 timer interrupts | **CPU 0 PPIs: enable drift** | **CPU 0 PPIs: enable drift** |

Also observed with the baseline image: `qemu-direct-check.sh` passes on the
`qemu-psci` build, the `qemu-direct` build still passes it (CPU 2 stays off,
as before), host tests 21 of 21 (new `test_irq_ppi_contract`).

## Observations

- **Read-back sees the grant change.** Against the v3 grant, the new PPI
  enable on CPU 2 is reported as drift: an earlier grant cannot silently
  absorb a new interrupt. Every rerun of case set v3 must therefore use its
  frozen commit (`ce220bb`), not later HEADs.
- **Two read-back blind spots, both expected.** K2: read-back reads
  configuration, not whether a CPU actually entered its partition; the
  partition's own report is the only evidence that B runs. K3: read-back
  flags enabled PPIs without a grant, but not a granted PPI that is disabled
  (SPIs are checked both ways). Candidate tool change RB2, for a future tool
  version.
- **The owner check works for PPIs** (K3): HAVEN's table says A owns the
  timer on CPU 2, B is running there, so HAVEN refuses to inject and
  disables the PPI rather than leaving a level interrupt to storm.
- **Console sharing.** HAVEN's denial line from CPU 2 is printed on the
  UART that partition A owns, concurrently with A's own output. No
  interleaving was seen in these runs, but nothing prevents it.

## Limits

- QEMU only, single cluster, MPIDR Aff0 = logical CPU (FINDINGS B3 still
  open: no ready acknowledgment, entry EL and bounds unchecked).
- HAVEN scans 16 list registers although `cortex-a72` in QEMU implements 4
  (FINDINGS I1, open). B never had more than one pending interrupt here.
- EL2 runs with its MMU off on both CPUs; the mailbox handoff relies on
  QEMU's lack of caches (FINDINGS A1, open). On a board this needs explicit
  cache maintenance or a cacheable shared mapping.
- B's timer is the EL1 virtual timer with CNTVOFF_EL2 left at its reset
  value. HAVEN's own EL2 timer (CNTHP) is initialised masked and never
  armed, so neither partition's budget (A 8 ms, B 2 ms per 10 ms) is
  enforced yet; with one partition per CPU this does not affect isolation
  here, only the temporal claims.
