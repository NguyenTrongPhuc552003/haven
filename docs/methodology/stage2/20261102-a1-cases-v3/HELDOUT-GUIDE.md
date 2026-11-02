# Writing held-out cases for A1 case set v3

A held-out case is a fault that you choose and Claude does not see until it
has been run. It is the one part of the evaluation that the tool's author
cannot tune for, so Claude does not propose concrete faults here: anything
Claude suggested would no longer be held out. This guide gives you the map
and the mechanics instead.

## Where enforcement lives (pick any)

| Enforcement point | Files | What they decide |
| - | - | - |
| Memory (stage 2) | `src/core/partition.c` (regions), `src/platform/qemu-virt/memory.h` (addresses, sizes), `arch/arm64/include/asm/s2_attrs.h` (permission and memory-type bits), `arch/arm64/mm.c` (page tables) | which guest addresses reach which physical memory, with which rights |
| DMA (SMMU) | `src/core/dma/smmu.c` (policy), `drivers/iommu/smmu_v3.c` (stream table, queues), `arch/arm64/mm.c` (DMA tables), `src/core/partition.c` (`dma_grants`) | which device may DMA where |
| Device setup | `drivers/pci/ecam.c`, `src/core/partition.c` (BAR, config page) | where the device's registers are, who can change them |
| Interrupts (GIC) | `drivers/irqchip/gic_v3.c`, `src/core/irq/ownership.c`, `arch/arm64/irq.c`, `src/core/partition.c` (`part_a_irqs`) | which interrupt goes to which CPU and partition |

## Shapes a fault can take

Combine a place from the table with a shape: a wrong constant or bit, a
size off by a page or a block, a check that is skipped, a call that is
missing, two steps in the wrong order, the right value sent to the wrong
target. Think of mistakes a tired developer, a bad merge or a copied
configuration could make. A fault you expect read-back to miss is as useful
as one you expect it to catch: it tests the limits, not only the strengths.

Keep each fault small (a few lines) and make sure HAVEN still builds; a
fault that breaks the build tests the compiler, not the method.

## Mechanics (about 5 minutes per case)

```sh
mkdir -p ~/thesis-heldout
cd ~/Documents/workspace/haven
# 1. make the change in your editor, then save it as a patch:
git diff > ~/thesis-heldout/H1.patch
# 2. undo it in the repository:
git checkout -- .
# 3. write down what you expect, before running anything:
cat > ~/thesis-heldout/H1.expect.txt <<'TXT'
What I changed and why it is a realistic mistake:
Read-back: finding (which class, on what) / no finding
Unit tests: pass / fail
Boot check: pass / fail
TXT
```

Repeat for H2 to H4. Then run them yourself (about 3 minutes per case):

```sh
python3 docs/methodology/stage2/20261102-a1-cases-v3/run_cases_v3.py \
    . docs/methodology/stage2/20261102-a1-cases-v3/runs --held-out ~/thesis-heldout
```

Results land in `runs/held-out/`. Then copy `~/thesis-heldout/*` into
`runs/held-out/` (the runner already copies each patch and expectation next
to its results), commit, and tell Claude; Claude scores them against your
expectations in the report.

Rules that keep the result honest: write the `.expect.txt` before the run
and do not change it afterwards; do not show the patches to Claude before
the run; run on the frozen commit (the runner uses HEAD, so do not commit
anything else to haven in between).
