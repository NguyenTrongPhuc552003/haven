# Unit tests for R2 and R3

**Status:** owner-approved 2026-11-07 ("Approve", item 3: unit tests for
the two faults nothing caught, see
`../20261106-linux-chain-cases/UNIT-TESTS.md`). Design and expectations
by Claude, committed before the patched runs. Results are appended below.

## What was added

- **R2 (affinity mask, `vgic.c`).** `test_vgic` gets `test_affinity`: a
  second vCPU with MPIDR Aff3 4, Aff2 1, Aff1 2, Aff0 3. It checks
  `GICR_TYPER` (64-bit and upper half) and that an SGI is accepted only
  when all four affinity fields and the target bit match. The MPIDR is
  synthetic: no CIX P1 value is assumed (MPIDR per core is a queued board
  experiment).
- **R3 (trap log cap).** The trapped system register handler was only
  compiled for ARM64, so no host test could reach it. It was **moved
  unchanged** from `el2_exceptions.c` to `src/core/exc/sysreg_trap.c`;
  the only differences are that the frame pointer and CPU number are
  parameters instead of being taken inside, and `HV_VIRT_IRQ_PRIORITY`
  moved to `el2_exceptions.h`. New `tests/unit/test_sysreg_trap.c`
  checks SGI injection, read-as-zero, ignored writes, ELR advance and the
  log cap of four.
- **R3 retargeted.** The original patch names the old file. The same
  mutation (`&&` to `||` on the identical line) is in
  `R3-sysreg_trap.patch`. This is a threat to validity: the test is
  checked against the moved code, not the code R3 was drawn from.

Unpatched, before this record: 25 of 25 host tests pass on macOS.

## Method

`run_unit.sh COMMIT OUT`: `git archive` of the commit that adds the tests,
then per run (unpatched, H1, H3, R1, R2, R3) apply the patch and run all
host tests with CTest (`bench_temporal_isolation` excluded as in CI).
H1, H3 and R1 are rerun to check the new tests did not change what was
already caught. Then the ARM64 image is rebuilt and the L4 boot repeated
(`../20261104-linux-partition/run_l4.sh`), because the move touches the
trap path Linux uses.

## Expectations (Claude, from reading the tests and patches)

| Run | Expected | Why |
| - | - | - |
| base | 25 of 25 pass | |
| H1 | 24 of 25; `test_vgic` fails (enable test) | as before; H1 only drops the ownership check for IDs 16 and up, the new tests enable only SGI 1 |
| H3 | 24 of 25; `test_vgic` fails at the start (two disable calls) | as before; the new tests' vCPUs own nothing |
| R1 | 24 of 25; `test_vgic` fails (enable test) | as before; the new tests touch only register word 0 |
| R2 | 24 of 25; `test_vgic` fails in `test_affinity` at the first `GICR_TYPER` check | the mask keeps MPIDR bit 24 instead of Aff2..Aff0, so the affinity reads 0x04000000 |
| R3 | 24 of 25; `test_sysreg_trap` fails in `test_log_cap` | with `||` every access is logged: 10 calls, not 4 |
| L4 boot | completes as in `RESULTS-L4.md`: `KDAL-DEMAND-END`, demand check 2 findings, read-back matches | the move does not change behaviour |

If these hold, every one of the 15 faults of the case set is caught by at
least one instrument.
