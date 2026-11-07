# Read-back on the board: method and fallbacks (G2 input)

**Status:** proposal for gate G2 (owner-approved as a G2 agenda item,
2026-11-07). Nothing here is built or observed on hardware. Labels:
**[F]** primary source or our own record, **[S]** community or single
source, **[I]** inference, **[P]** proposal.

## 1. Problem

Read-back compares what HAVEN enforces (stage-2 tables, SMMU stream
tables, GIC routing, and now its per-CPU partition table) with the grant.
In QEMU it reads HAVEN's memory and the device registers through the QEMU
monitor, from outside HAVEN [F, `../methodology/stage1/20261101-a1-readback/DESIGN.md`
section 2]. The same design plans board read-back "through a JTAG/SWD
debugger" [F, same]. On the Orange Pi 6 Plus that path is uncertain
[F, `../porting/cix-p1-ledger.md` row 24]:

- J21 is wired to the SoC's JTAG pins in the standard Arm 10-pin order, at
  1.8 V, but the header is not fitted in the vendor's photo, and three
  pull-ups are not fitted;
- whether the CD8180 lets an external debugger in on retail parts is
  unknown; the CIX firmware layout has a 4 KiB `SECURE_DEBUG` item;
- the vendor has been asked (owner, 2026-11-07).

## 2. What read-back needs from the board

| Need | Used for |
| - | - |
| Read HAVEN's memory (tables, partition table) | stage-2, SMMU, CPU assignment |
| Read device registers (GIC distributor and redistributors, SMMU) | GIC and SMMU drift |
| Stop the system at a chosen instruction | the entry read point (v9) |
| Not depend on HAVEN reporting itself | the thesis claim: read-back outside HAVEN's trusted code |

[I] A debugger can read memory and registers through a memory access
port without halting any core; only the entry read point needs a halt.
So a part that allows bus access but not core halt would still support
most of read-back.

## 3. Options

| Option | How | Outside HAVEN? | Needs | Risk |
| - | - | - | - | - |
| **A. External debugger** (planned) | JTAG probe on J21 reads memory and registers; halts a core for the entry read point | yes, fully | soldered header and pull-ups; debug not locked; probe support for Cortex-A720/A520 | the lock; unknown until the vendor answers or the board is tested |
| **B. EL3 read-back service** | a small handler added to the board's BL31 (TF-A): on an SMC from a read-back client, it copies HAVEN's tables and the GIC/SMMU registers to a buffer or a UART | yes; inside the firmware's trusted code instead | rebuilt and flashed BL31 (ledger row 8: rebuildable with the OEM key [S]); the owner's approval to flash | bricking the firmware; trusting EL3 code we wrote; no halt at a chosen instruction |
| **C. HAVEN reports itself** | HAVEN walks its own tables and prints them | **no**: adds trusted code | nothing on the board | breaks the adopted reading of the research question ("without growing HAVEN's trusted code"); a faulty HAVEN may also report wrongly |
| **D. Read-back partition** | a partition granted read-only access to HAVEN's memory and the GIC/SMMU registers | partly: HAVEN sets up its access | an extra core and grant | weakens isolation (a partition sees HAVEN); HAVEN controls the observer |
| **E. Behaviour probes only** | guests try allowed and forbidden accesses; the hardware's faults are the evidence | yes | nothing | tests behaviour by sampling, not the configuration; misses drift the probes do not touch |

E is already part of the design and runs alongside any option.

## 4. Proposal [P]

1. **A first**, decided by the vendor's answer and, on the board, the
   staged test in ledger row 24 (debug port ID, access ports, then halt).
   If bus access works but halting does not, keep A for memory and
   registers and drop only the entry read point on the board.
2. **B as the fallback** if external debug is locked: it keeps read-back
   outside HAVEN, at the cost of trusting our own EL3 code. Before the
   board, B can be prototyped in QEMU: HAVEN already has a `qemu-tfa`
   build profile [F, `CMakeLists.txt`], although no record has yet booted
   HAVEN through TF-A in QEMU.
3. **C only as the last resort**, with its added code measured and the
   research question's reading revisited at a gate, not silently.
4. **D is not proposed**: it gives a partition a view of HAVEN.
5. **E always**, as the behaviour check next to whichever of A to C works.

## 5. Decisions for G2

- Accept the order A, B, E (C last, D no).
- Approve prototyping B in QEMU through TF-A before the board, or wait
  for the vendor's answer first.
- Board purchase: if the vendor says debug is locked and cannot be
  unlocked, whether the Orange Pi 6 Plus is still the right board (the
  other boards in `data/` share the SoC, so a lock in the silicon or in
  CIX's firmware would likely apply to them too [I]).
