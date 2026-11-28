# How is the research question evaluated?

The core evaluation campaign in QEMU. Sources:
`docs/methodology/stage2/20261119-evaluation-campaign/README.md` (design)
and `RESULTS.md` (results of 2026-11-22), `tools/campaign/run_case.py`,
`tools/campaign/taxonomy.json`.

```mermaid
flowchart TB
  TAX["Frozen fault taxonomy<br/>19 classes: S2, SMMU, GIC, CPU, EL2, control"]
  subgraph CASES["Three sources of cases"]
    W["Written cases<br/>expectations approved before the run"]
    R["Random mutants<br/>mutate.py, seed from a merge commit"]
    H["Historical faults<br/>HAVEN's own fixes, reverted by a rule"]
    HO["Held-out cases<br/>written by the owner, sealed<br/>(proposed: channel ready, no cases yet)"]
  end
  RUN["run_case.py: one case per job<br/>patch applied to a clean copy"]
  subgraph METHODS["Every method on every case"]
    M1["M1 host unit tests"]
    M2["M2 boot checks"]
    M3["M3 grant structure,<br/>guest device tree"]
    M4["M4 partition B's own check"]
    M5["M5 KDAL's demand"]
    M6["M6 read-back through<br/>the QEMU monitor"]
    M7["M7 read-back of the<br/>EL3 dump under TF-A"]
  end
  SCORE["score.py: per-class detection,<br/>false findings on controls,<br/>disagreements between methods"]
  REC[("Results record<br/>RESULTS.md, threats to validity")]

  TAX --> W
  TAX --> R
  TAX --> H
  TAX -.-> HO
  W --> RUN
  R --> RUN
  H --> RUN
  HO -.-> RUN
  RUN --> M1
  RUN --> M2
  RUN --> M3
  RUN --> M4
  RUN --> M5
  RUN --> M6
  RUN --> M7
  M1 --> SCORE
  M2 --> SCORE
  M3 --> SCORE
  M4 --> SCORE
  M5 --> SCORE
  M6 --> SCORE
  M7 --> SCORE
  SCORE --> REC

  classDef observed stroke:#0ca30c,stroke-width:2px
  classDef proposed stroke:#8e8e93,stroke-width:2px,stroke-dasharray:6 4
  class TAX,W,R,H,RUN,M1,M2,M3,M4,M5,M6,M7,SCORE,REC observed
  class HO proposed
```

- M1 to M4 and M6 are the baselines and read-back on the same inputs:
  scenario 1 boots the bare test guests; scenario 2 (Linux in partition A)
  replaces M2 and M4 with the Linux runs and adds M5. M7 ran after the main
  run, on the scenario 1 cases.
- **Where the oracle is not independent** (`RESULTS.md`, threats to
  validity): the same author wrote read-back, the written cases and their
  expectations, and classed the random and historical cases. Only held-out
  cases written by the owner would close that gap.
