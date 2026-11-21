# Campaign step C2: results of the written cases (first run)

**Run:** `campaign.yml` on `tools/campaign/cases/c2-written.json`, ten
shards, at commit `54e3bd9` (the merge of #162), 2026-11-21, 5 min 10 s
from dispatch to summary. `freeze/SHA256SUMS-C2` holds at that commit
(`sha256sum -c`, all 55 files) [F]. **[F]** fact from the run's logs,
**[I]** inference, **[P]** proposal. Scored with `tools/campaign/score.py`
from the job logs' verdict lines.

## Four cases did not build (reported apart) [F]

| Case | Why | Effect |
| - | - | - |
| W03, W07, W12 | the edits use `HV_MEM_RW`, which `include/haven/stage2.h` does not define (it has `HV_MEM_R`, `HV_MEM_W`, `HV_MEM_RWX`, `HV_MEM_DEVICE_RW`) | every method saw a failed build, not the fault |
| W23 | replacing `gicd_irouter_from_mpidr(mpidr)` leaves `gic_v3_route_irq`'s `mpidr` unused; the build treats that as an error [I] | the same |

These are Claude's errors in writing the edits, not results. They are left
out of every table below; corrected edits with the same intent and the
same approved expectations (W03r, W07r, W12r, W23r) run separately,
frozen first. S2-EXTRA has no valid case until W12r runs.

## The 36 cases that built

| Class | M1 | M2 | M3 | M4 | M6 |
| - | - | - | - | - | - |
| CPU-ASSIGN | 0/2 | 0/2 | 0/2 | 2/2 | 1/2 |
| EL2-MAP | 0/1 | 0/1 | 0/1 | 0/1 | 1/1 |
| EL2-TIMER | 0/2 | 0/2 | 0/2 | 0/2 | 1/2 |
| GIC-ENABLE | 0/1 | 0/1 | 0/1 | 0/1 | 1/1 |
| GIC-GROUP | 0/2 | 0/2 | 0/2 | 1/2 | 2/2 |
| GIC-PPI | 1/2 | 0/2 | 0/2 | 2/2 | 2/2 |
| GIC-PRIO | 0/2 | 0/2 | 0/2 | 0/2 | 2/2 |
| GIC-ROUTE | 0/1 | 0/1 | 0/1 | 0/1 | 1/1 |
| S2-EXTENT | 2/4 | 1/4 | 0/4 | 0/4 | 3/4 |
| S2-MISSING | 2/2 | 0/2 | 0/2 | 0/2 | 2/2 |
| S2-OWNER | 1/1 | 1/1 | 0/1 | 1/1 | - |
| S2-PERM | 1/2 | 1/2 | 0/2 | 1/2 | 2/2 |
| S2-TYPE | 1/2 | 0/2 | 0/2 | 0/2 | 2/2 |
| SMMU-EXTENT | 1/2 | 0/2 | 0/2 | 0/2 | 2/2 |
| SMMU-PERM | 1/2 | 0/2 | 0/2 | 0/2 | 2/2 |
| SMMU-TAG | 1/2 | 0/2 | 0/2 | 0/2 | 0/2 |
| SMMU-UNBOUND | 0/1 | 0/1 | 0/1 | 0/1 | 1/1 |
| false findings on the 5 controls | 0/5 | 0/5 | 0/5 | 0/5 | 0/5 |

Exact 95 % intervals are in the run's summary; with one or two cases per
class they are wide (2 of 2 gives [0.158, 1]), so the table shows where
each method sees a class, not how often it would [I]. M5 and M7 did not
apply.

**Against the campaign's expectations** (these are written cases only;
E1 to E5 are judged after C3 and C4):

- *E1* (controls): the unchanged tree, W35, W36 and v3's C6 and C7 have no
  finding from any method [F].
- *E2* (a class M1 to M5 miss and M6 detects): twelve cases are detected
  by read-back alone: I7, W05, W06, W14, W17, W19, W22, W24, W25, W27,
  W29, W33 [F]. The classes where every valid case is read-back-only are
  GIC-PRIO, GIC-ROUTE, GIC-ENABLE, SMMU-UNBOUND and EL2-MAP [F]; they are
  the lower-layer cases (`mm.c`, `gic_v3.c`, `budget_hw.c`, the SMMU
  driver, the EL2 map), as the case design predicted [I].
- *E3*: I7 is detected by M6 alone; P1 is missed by every method [F].
- Predicted misses held: W21 (DMA VMID), W34 (budget), v3's S9 and P1:
  no method detects them [F].

**Prediction accuracy**: of 180 predicted verdicts (36 cases, five
methods), 178 held; the two disagreements:

1. **W08** (B's RAM mapped for A): M6 expected a finding, got an error:
   HAVEN halts before stage-2 setup (`partition 2 root 0x0 (root is
   zero)`), so there is no enforced state to read; M1, M2 and M4 detect it
   [F]. Cause [I]: HAVEN's own launch refuses a partition with no memory;
   the prediction assumed it would boot. Not a read-back miss.
2. **W09** (CPU 2 enables stage 2 with A's tables while HAVEN's table
   says B): **M6 passes; M4 detects it** (partition B faults) [F].
   Cause [F]: read-back's CPU check (v10, `readback.py`) compares the
   grant with HAVEN's own table (`cpu_partition`), never with each CPU's
   VTTBR_EL2, so a fault below the table is invisible to it. **A blind
   spot of read-back, named before anything changes (E5).**

## Next [P]

1. Run W03r, W07r, W12r and W23r (corrected edits, same expectations),
   frozen by hash first.
2. W09's blind spot: read each CPU's VTTBR_EL2 (QEMU's monitor or gdb
   stub) and compare it with the partition's stage-2 root; the change and
   its expectations as a separate record, then W09 runs again as it is.
   Recorded as a limitation until then.
3. C3: the owner's held-out cases and the K7 channel.

## The corrected run's freeze

`make_cases.py` gained the four corrected edits (`--corrected` writes
`tools/campaign/cases/c2-corrected.json` and `c2/W03r.patch` to
`W23r.patch`; `c2-written.json` comes out unchanged). `SHA256SUMS-C2`
was checked at the first run's commit, before this change.
`freeze/SHA256SUMS-C2r` freezes the corrected run: the same files as
`SHA256SUMS-C2`, with the new generator, cases file and patches.
