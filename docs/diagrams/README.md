# Architecture diagrams

These diagrams show how HAVEN, ELMOS and KDAL fit together, with HAVEN as
the core, and how the working research question (can independent read-back
of enforced hardware state find enforcement drift that tests and
configuration checks miss?) is evaluated. Each diagram answers one review
question and names the files it is drawn from.

They are Mermaid text: GitHub draws them here, and the
[documentation site](https://docs-haven.vercel.app/haven/diagrams/) draws
the same text in its own colours. They replace the PlantUML set of
2026-11-01, which was kept outside the repository and had fallen behind
the code (it still showed Linux, KDAL and read-back as proposed).

**State as of 2026-11-28, `main` at `c825dd1`.**

## Evidence key

A diagram never shows a part that does not exist as if it did. Each element
carries one mark as its border; implemented and proposed elements also say
so in their label, so nothing depends on colour alone.

| Border | Mark | Meaning |
| - | - | - |
| solid green | observed | built and run; evidence recorded |
| solid amber | implemented | in the code; not yet run |
| dashed grey | proposed | design only; nothing built |

Elements without a border mark are context: hardware, firmware HAVEN does
not own, people. A diagram whose elements are all observed may say so once
in its text instead (class diagrams draw no borders per element on every
renderer). In sequence diagrams, which have no borders, a part that is
not observed says so in its name or in the block around it. Update a mark in
the same change that records the evidence. The same key is used by ELMOS and
KDAL ([Reading the diagrams](https://docs-haven.vercel.app/start/diagrams/)).

## Index

| File | Kind | Question |
| - | - | - |
| [`01-layered-el-stack.md`](01-layered-el-stack.md) | layers (EL0 to EL3) | What runs where, and what is trusted? |
| [`02-container-view.md`](02-container-view.md) | containers | What are the three repositories, and who depends on whom? |
| [`03-evaluation-pipeline.md`](03-evaluation-pipeline.md) | activity | How is the research question evaluated, and where is the oracle not independent? |
| [`04-qemu-virt.md`](04-qemu-virt.md) | deployment | What exactly runs in QEMU, with which addresses? |
| [`05-cix-boards.md`](05-cix-boards.md) | deployment | What is the physical target, and what is unverified? |
| [`06-boot-and-admission.md`](06-boot-and-admission.md) | sequence | How does HAVEN reach running, isolated partitions? |
| [`07-stage2-fault-handling.md`](07-stage2-fault-handling.md) | sequence | What happens on a denied guest access? |
| [`08-psci-cpu-on.md`](08-psci-cpu-on.md) | sequence | How are CPUs started, for HAVEN and for a guest? |
| [`09-kdal-demand.md`](09-kdal-demand.md) | sequence | How does KDAL's demand reach the check without growing the TCB? |
| [`10-partition-lifecycle.md`](10-partition-lifecycle.md) | state machine | What states can a partition be in? |
| [`11-resource-lifecycle.md`](11-resource-lifecycle.md) | state machine | How does a resource move from demand to checked enforcement? |
| [`12-resource-model.md`](12-resource-model.md) | class | What data describes a resource, and what is compared? |

## Writing one

Copy the three class definitions into the diagram and give every element a
class:

```text
classDef observed stroke:#0ca30c,stroke-width:2px
classDef implemented stroke:#fab219,stroke-width:2px
classDef proposed stroke:#8e8e93,stroke-width:2px,stroke-dasharray:6 4
```

Keep logical views (layers, containers, classes) apart from physical ones
(deployments). English only, no em-dashes. GitHub's preview of the file shows
whether the diagram draws.
