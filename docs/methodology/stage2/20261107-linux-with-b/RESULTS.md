# Linux with partition B: results

**Run:** 2026-11-07, macOS, QEMU virt. Y1 to Y3 at haven `584ae1f`
(expectations committed there), Y4 and Y5 at `a6bf810` (expectations
added there after Y1, before Y4 ran). Raw output: `runs/Y1..Y5/`,
`runs/results.md` (Y1 to Y3) and `runs/results-Y4-Y5.md`.

| Run | Profile | Grant | Read at | Observed | Expectation |
| - | - | - | - | - | - |
| Y1 | qemu-psci | `grant-l4-cpus.json` | `KDAL-DEMAND-END` | B started; CPU table 1/1, 0/0, 2/2, 0/0; Linux reached `KDAL-DEMAND-END`; **1 finding: CPU 2 PPI 26 enabled without a grant** | **missed** (expected "matches") |
| Y2 | qemu-psci | `grant-l4-cpus.json` | entry breakpoint | as Y1, no Linux output yet; same finding | **missed** |
| Y3 | qemu-direct | `grant-l4-cpus.json` | `KDAL-DEMAND-END` | B inactive; 1 finding, CPU 2 granted to B but running nothing | held |
| Y4 | qemu-psci | `grant-l4-withb.json` | `KDAL-DEMAND-END` | **matches**; CPU table 1/1, 0/0, 2/2, 0/0; B started; Linux reached `KDAL-DEMAND-END` | held |
| Y5 | qemu-psci | `grant-l4-withb.json` | entry breakpoint | **matches**, same table, no Linux output | held |

3 of 5 expectations held. The two misses were a prediction error, not a
HAVEN fault: the grant written for `qemu-direct` left out HAVEN's own EL2
timer PPI 26 on CPU 2, which HAVEN enables for partition B's budget once
B runs (its source note said so). Read-back found it; the corrected grant
`grant-l4-withb.json` matches at both read points.

**Result.** With the `qemu-psci` profile, the QEMU Linux configuration runs
both partitions, and its enforced state matches the grant at the guest's
entry and after Linux has loaded KDAL. New Linux runs use this profile
and grant.

**Observations (not predicted).**
- Partition B prints nothing on the UART Linux owns: its demo guest has
  no UART by design (`tests/demos/guest_b_entry.S`). The one denied access
  in Y4, `IPA=0x40000000 ... cpu=2`, is that guest's deliberate read of
  partition A's base, which HAVEN blocks (same file).
- Linux reached `Run /init` at 0.679 s (kernel time) with B running,
  against 0.437 s in L4 without B. This is QEMU on a Mac: both partitions'
  CPUs are host threads, and kernel time follows host time, so the
  difference is not evidence of hardware interference. Interference is
  measured on the board (STATUS, open items).
