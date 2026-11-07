# Interference pipeline with Linux in partition A: results

**Run:** 2026-11-07, macOS, QEMU 11.1 TCG, cortex-a72, haven `eb11bcd`
(expectations in `README.md`; see its note on order). Raw output:
`runs/<cell>/` (UART log, B's status record, tree), `runs/results.md`.

| Cell | Linux load | B budget | Pipeline checks | B probe: avg / max (us) | < 1 ms | 1 to 10 ms | Linux `/init` at |
| - | - | - | - | - | - | - | - |
| idle-Boff | idle | off | 11 of 11 | 541 / 1510 | 494 | 6 | 0.55 s |
| cpu-Boff | cpu | off | 11 of 11 | 507 / 2066 | 498 | 2 | 0.56 s |
| mem-Boff | mem | off | 11 of 11 | 273 / 1261 | 499 | 1 | 0.64 s |
| idle-B2ms | idle | 2 ms / 10 ms | 11 of 11 | 3974 / 8929 | 249 | 251 | 0.56 s |

**Expectations:** both held. All 11 checks passed in all 4 cells; the
control cell averages about 4 ms (as on 2026-11-03), the budget-off cells
under 1 ms.

**Observations.**
- The pipeline works with Linux as the aggressor: Linux loads run from
  about 0.6 s, partition B measures from 4 s with its caches on, the
  record is read back over QMP, and isolation holds (B's read of A's
  memory denied on CPU 2 in every cell).
- The numbers move between runs: idle-Boff averaged 309 us in the
  stopped first start and 541 us here, and the memory load cell is the
  lowest. Both show that QEMU timing on the Mac is noise at this scale;
  no load effect can be read from them, as `README.md` said.
- With B's budget on, about half the samples wait for the next period
  (251 of 500 between 1 and 10 ms): the probe measures its own budget,
  confirming the 2026-11-03 advice to run the victim with its budget off.

**For the board** (unchanged from `../20261102-interference/PLAN.md`): the
same probe and loads, victim on A720 and A520 cores, at least 10 runs per
cell and 10^5 samples each; Linux as the aggressor is now ready to use.
