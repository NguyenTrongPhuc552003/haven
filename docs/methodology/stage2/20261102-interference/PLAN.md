# Interference measurements: plan (the thesis's HOW WELL chapter)

**Status:** PLAN, 2026-11-02, approved in principle by the owner
("interference measurements"). Written by Claude after the budget work.
Nothing here is measured yet. The QEMU pipeline check of prerequisites 1 to 3
ran on 2026-11-03: see `README.md` (all cells ok, numbers host-bound).

## Question

With Haven's isolation in place (stage-2, SMMU, interrupt ownership,
budgets), how much does a busy partition disturb a real-time partition's
timing on CIX P1, and which of the known countermeasures reduce it? This
is the "not a novelty claim" performance chapter of the charter (section
8): interference is measured, not claimed solved.

## Why QEMU cannot answer it (observed 2026-11-02)

- QEMU's TCG does not model caches, memory bandwidth or the interconnect,
  so a memory aggressor slows nothing it shares.
- Its timing is the host's: the EL2 timer interrupt arrived 0.5 to 2 ms
  late on the Mac (`../20261102-budgets/`), far above the effects to be
  measured.
- So QEMU validates the measurement pipeline (probes, data export,
  analysis), and only the board produces numbers.

## Prerequisites (in order)

1. **Cacheable guests.** Both demo guests run with their EL1 stage-1 MMU
   off, so every guest access is Device memory: no guest touches a cache,
   and no cache interference can occur, even on the board. The guests need
   an identity map with Normal write-back memory first (small, guest-side
   code; not trusted code).
2. **Victim probe.** Partition B measures, per virtual-timer interrupt, the
   delay from the programmed deadline (CNTV_CVAL) to its handler, plus the
   run-to-run variation of a fixed workload; histogram in its status
   record.
3. **Aggressors in partition A.** (a) CPU-only loop, (b) memory bandwidth
   (streaming over a buffer larger than the last-level cache), (c) later,
   the GPU through a Linux partition (needs the Linux guest; experiment G4
   in `docs/architecture/gpu-partition.md`).
4. **Board.** Orange Pi 6 Plus, with Haven entering at EL2 (stage 3).

## Experiment matrix (board)

| Factor | Levels |
| - | - |
| Victim placement | A720 core, A520 core (CPU numbering from the vendor MADT, `docs/porting/cix-p1-ledger.md`) |
| Aggressor | none, CPU loop, memory stream, GPU load (later) |
| Countermeasure | none; Haven budgets on the aggressor's CPUs; CPU bandwidth regulation by PMU counters (MemGuard-style, a later addition); GPU frequency cap through SCMI (Linux) |
| Repetitions | at least 10 runs per cell, 10^5 samples each |

Metrics: victim deadline-to-handler latency (p50, p99, p99.9, max), victim
workload time variation, aggressor throughput (to show the cost of each
countermeasure). Threats to validity: firmware frequency scaling (pin
frequencies through SCMI or record them), EL3 interference (ledger rows 2
and 14), thermal state, measurement overhead.

## Next concrete step

Prerequisite 1 and 2 in QEMU (pipeline only, numbers meaningless), then
the board.
