# Linux chain case set v1: results

**Run:** 2026-11-06, macOS, QEMU cortex-a72, haven `7904642` (the freeze
commit; inputs re-checked against `freeze/SHA256SUMS`), 16 cases with
`run_cases.py ... --held-out heldout`. Raw output: `runs/<case>/`
(`uart.log`, `demand.json`, `demand-check.txt`, `readback.txt`,
`readback.json`, `summary.json`) and `runs/results.md`. T4's log is cut
to its first 300 and last 50 lines; the full log's hash is in
`runs/FULL_LOG_SHA256SUMS`.

## Score

Each of the 15 scored cases has four predicted checks (60 predictions):
12 owner-approved rows from `CASES.md`, 3 held-out rows from
`EXPECT-random.md` written by Claude after the random draw.

| Case | Demand check | Read-back | Boot | HAVEN log | All four |
| - | - | - | - | - | - |
| T1 | held | held | held | held | yes |
| T2 | held | held | held | held | yes |
| T3 | held | held | held | held | yes |
| T4 | **missed** | **missed** | held | held | no |
| T5 | **missed** | **missed** | held | held | no |
| G1 | held | held | held | held | yes |
| G2 | held | held | held | held | yes |
| H1 | held | held | held | held | yes |
| H2 | held | held | held | held | yes |
| H3 | held | held | held | held | yes |
| C1 | held | held | held | held | yes |
| C2 | held | held | held | held | yes |
| R1 | held | held | held | held | yes |
| R2 | held | held | held | held | yes |
| R3 | held | held | held | held | yes |

**56 of 60 predictions held; 13 of 15 cases right on all four checks.**
B0 (L4 unchanged, unscored) reproduced L4 exactly.

## The four misses (observations)

- **T4 (memory node 1 GiB) and T5 (timer as PPI 12):** Linux did not reach
  `KDAL-DEMAND-END`, as predicted, so KDAL never printed a demand. The
  predictions for the demand check ("no finding", "as L4") contradicted the
  predicted boot failure; this is an error in the predictions (Claude's),
  not in the tools.
- **Read-back in T4 and T5** did not match the grant, against the
  prediction: `SPI 33: missing interrupt binding (granted but disabled)`
  and `CPU 0 PPI 27: missing interrupt binding (granted but disabled)`.
  Read-back read at its 60 s time limit, before Linux had enabled either
  interrupt. Since L3, the emulated GIC keeps a partition's owned
  interrupts physically disabled until the guest enables them, so the
  enforced state depends on how far the guest got.

## Other observations

- **T4:** 101,905 denied stage-2 accesses in 45 s, all at the top of the
  claimed but ungranted memory (first: IPA 0x7ffff000, ESR 0x92000046: a
  write, translation fault, no instruction syndrome). HAVEN skipped each
  faulting instruction; Linux kept running on wrong memory and never
  finished booting. Isolation held: read-back shows no extra mapping.
- **T5:** Linux registered the architected timer, then stalled once the
  console moved to ttyAMA0 (last line at 0.19 s); no timer interrupt was
  ever delivered (PPI 28 is not owned, so the model ignored its enable;
  PPI 27 stayed disabled).
- **G1, G2, H2:** read-back reported exactly the planted difference
  (missing GPIO binding and SPI 39; PPI 27 enabled without a grant;
  GPIO page mapped without a grant). The demand check saw G1 (no finding:
  the grant now covers the demand) and, by design, not G2 or H2 (they do
  not change what KDAL needs).
- **H1, H3, R1, R2, R3:** no check saw any difference. Each is a real
  defect in the Linux-guest code (the GIC model, the trap logger) that this
  guest never exercises.

## Interpretation (inference)

1. **The two checks see different things, as designed.** The demand check
   finds driver-side mismatches before any access (T2, T3, as in L4);
   read-back finds enforcement-side ones (G1, G2, H2). No single check
   covered every planted mismatch, which supports keeping both.
2. **End-state checks miss latent defects in the emulation.** Five of
   fifteen faults (three of them drawn at random) changed nothing any
   check could see. For the thesis this bounds the claim: read-back
   verifies the isolation that is enforced, not the correctness of code
   paths the guest did not take. The host unit tests are the instrument
   for those paths (inference: `test_vgic` asserts that only owned
   interrupts reach the hardware, which H1 violates; not run in this set).
3. **Read-back's verdict depends on when it reads**, once the hypervisor
   enables interrupts lazily (T4, T5). Reading at a fixed guest milestone
   (the marker) is part of the method, not a convenience; a read at the
   time limit reports "granted but disabled" for a guest that stalled.
   Whether the grant should say "enabled when the guest asks" is an open
   design question for the grant format.
4. **A guest can flood HAVEN's log** (T4: about 2,300 denied accesses a
   second, each logged). Availability is out of scope in the charter, but
   the log is evidence; rate-limiting it is a candidate change.
5. **No checker compares the device tree with the grant** (T against G).
   T4 and T5 would have been caught before boot by such a check; here only
   their symptoms showed.

## Limits

- One guest workload (boot, load KDAL, power off), one CPU, QEMU only.
- The 12 approved predictions and the 3 held-out expectations are all
  Claude's; the owner approved the 12 and the held-out faults were drawn
  at random, but no expectation came from an independent person.
- Host unit tests and the QEMU boot check of the other partitions were
  not part of this set.
