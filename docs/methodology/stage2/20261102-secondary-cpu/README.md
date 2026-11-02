# Secondary CPUs: CPU map, entry checks, ready handshake (FINDINGS B3)

**Status:** EXPERIMENT, observed 2026-11-02. Work approved by the owner
(2026-11-02). Controls and expectations by Claude, committed in `f41b0a7`
before the first run; not blind.

## Problem

`secondary_entry` took its stack and mailbox index from MPIDR Aff0 without
checking the exception level or bounds, `hv_arch_cpu_id` used
`Aff1 * 4 + Aff0`, PSCI CPU_ON and GIC routing used the logical index as
the MPIDR, and the primary logged "CPU 2 woken" as soon as CPU_ON returned.
On CIX P1 every core has Aff0 = 0 and its number in Aff1, with core 4
booting (vendor MADT, see `docs/porting/cix-p1-ledger.md`): all secondaries
would have shared stack 0.

## Changes

| Commit | Change |
| - | - |
| `91daeb8` | Per-platform CPU map (`hv_cpu_mpidr[]`, boot CPU first) in `asm/cpu_map.h`; `secondary_entry` checks EL2, finds its index in the map, parks if absent or if it claims entry 0; PSCI CPU_ON and SPI routing take the MPIDR from the map; ready handshake in `arch/arm64/smp.c` (secondary reports ready after its EL2 setup and waits; primary waits at most 100 ms, then says go or abort; a late CPU parks); the boot CPU must be entry 0 |
| `be4eb32` | Found by L5: GCC folded the weak const default `hv_cpu_count` into `cpu.c`, so C used 4 while `boot.S` used the platform's value. No weak default any more; each platform defines its map (legacy i.MX keeps the old assumption, unverified). No CPU_ON for a CPU missing from the map |

## Results (`run_b3.py`, `runs/results.md` at `be4eb32`; 10 of 10 as expected, both CPUs identical)

| Check | Change | Observed |
| - | - | - |
| L1 | none | "CPU 2 ready, partition B started"; B reaches stage 5 |
| L2 | CPU 2's MPIDR also given to entry 1 | CPU 2 resolves to index 1, never reports ready for index 2; "CPU 2 not ready within 100 ms - partition B not started"; B never runs; partition A unaffected |
| L3 | CPU 2 reports ready 300 ms late | same "not ready" line; CPU 2 later sees the abort and parks; B never runs |
| L4 | boot CPU not entry 0 of the map | Haven panics "boot CPU is not entry 0 of the CPU map" |
| L5 | no CPU 2 in the map | "no CPU 2 in the CPU map - partition B not started"; no CPU_ON issued |

## How the record got here (kept for honesty)

- `runs-first/` (at `f41b0a7`): the runner matched lines anywhere in the
  check script's output, which repeats some lines as check labels, so L2 to
  L5 were wrongly scored as failures. L5 also showed a real defect: the
  ready deadline fired instead of a PSCI failure.
- `runs-second/` (at local commit `e364526`, replaced by `be4eb32` before
  anything was pushed; runner fixed): L1 to L4 as expected; L5
  still wrong. An objdump of `hv_arch_logical_mpidr` showed the count
  folded to 4 (the cause above). The first commit message for the L5 fix
  wrongly blamed QEMU's PSCI; it was corrected before anything was pushed.

## Limits

One secondary CPU, started once; no CPU off or restart; QEMU's built-in
PSCI, not vendor firmware. The handshake words are Device memory today
(EL2 stage-1 off); the EL2 memory decision (`../20261102-el2-memory/`)
keeps the protocol valid under a cacheable mapping.
