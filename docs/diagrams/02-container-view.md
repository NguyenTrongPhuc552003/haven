# What are the three repositories, and who depends on whom?

Build time and run time. Sources: `.github/workflows/ci.yml`,
`tools/readback/`, `tools/campaign/`, `docs/methodology/`, ELMOS's
`docs/developer/sdk-ci.md`, the [ecosystem page](https://docs-haven.vercel.app/start/ecosystem/).

```mermaid
flowchart LR
  OWNER(["Owner<br/>approves grants and expectations,<br/>decides gates"])
  subgraph BUILD["Build: CI runners or a Mac"]
    ELMOS["ELMOS<br/>SDK kernel and root file system,<br/>demos"]
    HB["HAVEN build<br/>CMake, aarch64 GCC<br/>haven.bin, guest images"]
    KB["KDAL build<br/>kdal.ko against the SDK kernel"]
  end
  subgraph TARGET["Target: QEMU virt today"]
    FW["TF-A BL31<br/>(qemu-tfa profile)"]
    HV["HAVEN at EL2"]
    LX["Linux in partition A"]
    KM["KDAL module"]
  end
  BOARD["CIX P1 board<br/>(proposed: not in hand)"]
  subgraph CHECK["Checks, outside the TCB"]
    RB["read-back<br/>readback.py"]
    DC["demand check<br/>demand_check.py"]
    DT["guest device tree check<br/>dt_check.py"]
    GR["grant structure<br/>grant.py"]
  end
  CAMP["Campaign<br/>mutate, run_case, score, seal"]
  EV[("Evidence<br/>docs/methodology: dated records,<br/>logs, hashes")]

  OWNER --> CAMP
  ELMOS -->|SDK kernel| KB
  ELMOS -->|SDK kernel and root fs| LX
  HB --> HV
  KB --> KM
  KM --> LX
  HV --> LX
  HV -->|SMC PSCI| FW
  HB -.-> BOARD
  HV -.->|enforced state| RB
  KM -.->|demand| DC
  CAMP --> RB
  CAMP --> DC
  CAMP --> DT
  CAMP --> GR
  CAMP --> EV

  classDef observed stroke:#0ca30c,stroke-width:2px
  classDef proposed stroke:#8e8e93,stroke-width:2px,stroke-dasharray:6 4
  class ELMOS,HB,KB,FW,HV,LX,KM,RB,DC,DT,GR,CAMP,EV observed
  class BOARD proposed
```

- Dependencies point one way: ELMOS and KDAL never need HAVEN to build;
  HAVEN's CI uses a pinned KDAL and ELMOS's SDK kernel.
- The checks read the grant (a JSON file the owner approves) and what the
  target shows; none of them runs inside HAVEN.
- The CIX P1 build compiles in CI (`cix-p1` draft), but no board has run it.
