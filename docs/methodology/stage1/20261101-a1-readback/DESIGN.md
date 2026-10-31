# A1: enforcement read-back (design note)

**Status:** DOCUMENTATION plus a feasibility probe (EXPERIMENT, QEMU only, no
HAVEN code change), started at the workspace owner's direction on 2026-11-01
("go A1"). Not a scored experiment and not a G1 decision.
**Owner:** HAVEN (research question), ELMOS (evidence tooling, later adoption).
**Context:** P-A0 ended REJECT/REFRAME because its effective binding H was a
modelled value, so every extra detection reduced to comparing fields against
the envelope E (`../20261031-pa0/README.md`). A1 replaces modelled H with H
**observed** from the real enforcement state (`stages/RESEARCH_CHARTER.md`
section 13).

## 1. Question

Can independent read-back of the enforced stage-2 (and later GIC) state detect
enforcement drift, meaning H differs from the admitted grant G while G itself
agrees with E, that policy tests, structural checks and G-versus-E comparison
cannot see, without enlarging HAVEN's trusted code?

MM1 and MM2 are the motivating cases: in both, G and E agreed and only the
hardware descriptors were wrong.

## 2. Three independent views of H

| View | How | Trust | Where it works |
| - | - | - | - |
| **H-ext** (external read-back) | QEMU monitor dumps HAVEN's memory; a host script finds `pt_roots` by symbol and walks the stage-2 tables with the Arm descriptor format | outside the TCB; does not rely on HAVEN reporting itself | QEMU now; boards later through a JTAG/SWD debugger |
| **H-int** (internal dump) | HAVEN walks its own tables and prints them | inside the TCB; adds code | QEMU and boards |
| **H-probe** (behaviour) | guest probes perform allowed and forbidden accesses; the hardware's own faults are the ground truth | architecture behaviour | QEMU (function only) and boards |

A finding counts when the views agree. H-ext and H-probe disagreeing with G is
an enforcement drift; H-int disagreeing with H-ext means HAVEN misreports its
own state. Only H-ext is built in this note.

## 3. Facts the probe relies on (source-derived, branch `stage1/evidence-and-bugfixes`)

- HAVEN runs at EL2 with its own MMU off; `pt_pa()` returns the pointer
  unchanged, so table pointers are physical addresses (`arch/arm64/mm.c`).
- Tables come from a static 4 MiB BSS pool; roots are in `pt_roots[]`, indexed
  by partition ID (`arch/arm64/mm.c`).
- VTCR: T0SZ = 25 (39-bit IPA), SL0 = 1 (walk starts at level 1), 4 KiB granule
  (`VTCR_HAVEN_DEFAULT`, `arch/arm64/include/asm/sysregs.h`).
- Image region: PA 0x8000_0000, 8 MiB (`src/platform/qemu-virt/memory.h`).

## 4. Oracle for the expected binding

The expected descriptor fields are derived from G (the region table in
`src/core/partition.c` and `memory.h`) using the translation table approved in
the MM1 proposal (section 4), not by reading `mm.c`. For each mapped page:
output address, S2AP, MemAttr, SH, XN, AF. Coalesced ranges are compared.

## 5. Fault taxonomy for later scoring (draft, not frozen)

| Class | Example | Why G-versus-E comparison misses it |
| - | - | - |
| access drift | S2AP broader than G (MM1) | G and E agree; only descriptors differ |
| execute drift | XN = 0b11 instead of 0b10 (MM2) | same |
| memory-type drift | Device mapped as Normal, or reverse | same |
| association drift | descriptor output address differs from G's PA | same |
| extent drift | a block descriptor covers more than the region | same |
| missing binding | a granted page is not mapped | same |
| stale binding | a mapping survives unmap | G no longer lists it |
| cross-partition alias | one PA reachable from two partitions' tables | policy models only its own records |
| IRQ route drift (later) | GIC `IROUTER` target differs from the grant | same |

Seeding method: disposable mutants of the arch binding, and direct descriptor
corruption after mapping, each with an owner-reviewed expected outcome, as in
P-A0.

## 6. Prior work to recheck before any claim

seL4 capDL verified initialisation (proves initial state matches the spec,
formally); Xen P2M audit facilities; pKVM page-ownership checks; Jailhouse
configuration checks. A1's possible distinction is runtime, test-based
read-back with independent views for an unverified hypervisor, and a
measured fault taxonomy, not "first to compare tables with a configuration".

## 7. Feasibility probe (this step)

1. Boot the current qemu-direct build; wait for partition A's post-fault line.
2. Dump PA 0x8000_0000 + 8 MiB with QMP `stop` then `pmemsave`.
3. Find `pt_roots` with `nm`, walk the tables of partitions 1 and 2, coalesce
   leaves into ranges, and compare them with the expected binding from G.
4. Repeat with a disposable mutant that reverts MM2 (XN = 0b11) and check that
   the read-back reports execute drift on the UART mapping while G and E are
   unchanged.

Success means the external view is practical and sees drift that G-versus-E
comparison cannot. It does not establish a research distinction.

Outcome (2026-11-01): see `RESULTS.md`. Baseline H matches G on cortex-a72
and max; the MM2-revert mutant is reported as execute drift on the A UART
mapping while guest output, G and E are unchanged. The dump uses QMP, because
the human monitor misparsed the file path.
