# GIC read-back: interrupt ownership as the GICv3 enforces it (QEMU)

**Status:** EXPERIMENT, feasibility, observed 2026-11-02 at haven `401f5da`
(read-back v3 with GIC). Approved by the owner as the next read-back target.
Not a scored case set: Claude wrote the cases and expected verdicts; nothing
is blind. A scored set needs owner-approved verdicts first, as for A1 v1/v2.

**Question.** Does external read-back of the GIC find interrupt-ownership
drift that the host unit tests and the boot check do not?

## Method

Read-back stops QEMU and reads, one register access at a time, GICD_CTLR,
each SPI's enable, group and IROUTER, and each redistributor's PPI enables
and affinity (GICR_TYPER). It compares them with the grant
(`grant-gic.json`: SPI 33, the PL011 UART, owned by partition A on CPU 0;
A on CPUs 0-1, B on CPU 2; no PPI granted). Drift classes: GIC distributor
drift, interrupt enable drift (SPI or PPI enabled without a grant), interrupt
routing drift (1-of-N, or another partition's or another CPU), interrupt
group drift, missing interrupt binding. Runner: `run_gic.py`; results:
`runs/results.md`; dumps replaced by `runs/DUMP_SHA256SUMS`. QEMU 11.1.3,
cortex-a72 and max.

## Results (both CPUs identical)

| Case | Seeded change | Read-back | Unit tests | Boot check | Caught only by read-back |
| - | - | - | - | - | - |
| I0 | none | matches the grant | pass | pass | control |
| I1 | SPI 33 configured to CPU 2 (partition B's CPU) | routing drift: CPU 2, partition 2 | pass | pass | yes |
| I2 | driver routes SPIs 1-of-N | routing drift: may reach any CPU | pass | pass | yes |
| I3 | enabling one SPI enables its whole register (32-63) | enable drift: 31 SPIs without a grant | pass | pass | yes |
| I4 | SPIs put in Group 0 | group drift on SPI 33 | pass | pass | yes |
| I5 | every PPI enabled on every CPU | PPI enable drift on CPUs 0-3 | pass | pass | yes |
| I6 | ownership assigned but the SPI never enabled | missing interrupt binding | pass | pass | yes |
| C1 | route value computed into a local first (same register value) | matches the grant | pass | pass | control |

All 6 seeded faults gave the expected finding; neither control gave one. In
I5 the expected finding was CPU 0; read-back also reported CPUs 1-3, which
is correct for that change.

## Observations

- **None of the six faults is visible to the existing checks.** The host
  tests exercise the ownership table, not the distributor; the boot check
  only looks at UART lines, and HAVEN's log line still says
  "SPI 33 -> CPU 0" in I1, because it prints a constant.
- **Tool pitfall:** a multi-value `xp` reads device memory in wide chunks,
  which QEMU's GIC model answers with zeros; the first baseline therefore
  showed SPI 33 disabled. Read-back now reads one register per access.
- **GI1 (finding ledger section 7):** HAVEN writes GICD_CTLR with the
  Secure-view bit layout (ARE_NS at bit 5) although it runs Non-secure,
  where ARE_NS is bit 4 (Linux `arm-gic-v3.h`). QEMU reads back `0x53`
  (DS = 1, ARE read-as-one), so it is harmless here; on a board with two
  security states it is not.
- **GI2:** an SPI granted to a partition is enabled and routed, but never
  reaches the partition: HAVEN had no physical interrupt path at EL2 and no
  virtual injection. The DMA hardening change adds the EL2 path for HAVEN's
  own interrupt; delivery to partitions is still open.

## Interpretation (inference)

Same pattern as stage-2 and SMMU: drift in the enforcing hardware state that
policy tests and boot output cannot see, found by reading that state from
outside. Together with A1 v1/v2 and G2 this covers all three enforcement
points named in the research question.

## Limits

QEMU only, single security state (DS = 1); no LPIs or ITS, no GICv4. Read-back
does not read the CPU-interface system registers (ICC_*, ICH_*), so it cannot
see whether an interrupt is delivered, only how the distributor routes it.
