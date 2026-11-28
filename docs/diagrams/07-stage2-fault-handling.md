# What happens on a denied guest access?

A guest's load or store, or instruction fetch, outside its stage-2 map.
Sources: `src/core/exc/el2_exceptions.c` (`hv_s2_fault_action`,
`hv_handle_sync`), `src/core/exc/mmio.c`, `include/haven/el2_exceptions.h`.

```mermaid
sequenceDiagram
  participant G as Guest (EL1)
  participant S2 as Stage-2 MMU
  participant H as hv_handle_sync (EL2)
  participant ACT as hv_s2_fault_action(esr)
  participant MMIO as MMIO emulation<br/>(registered device models)
  participant LOG as UART log
  G->>S2: access to an IPA it was not granted
  S2->>H: synchronous exception to EL2<br/>ESR_EL2, FAR_EL2, HPFAR_EL2
  H->>ACT: classify
  alt data abort from EL1 (EC 0x24), FSC 0x04 to 0x0F, S1PTW = 0
    ACT-->>H: SKIP
    H->>MMIO: translation fault at a registered address?
    alt a device model owns it (partition A's emulated GIC)
      MMIO-->>H: emulated, ELR += 4
      H-->>G: ERET
    else no device model
      H->>LOG: "Denied stage-2 violation IPA FAR ESR cpu n"<br/>(rate limited per CPU)
      H->>H: decodable syndrome: a load reads as zero,<br/>a store is ignored, ELR += 4
      H-->>G: ERET, the guest continues
    end
  else instruction abort from EL1 (EC 0x20)
    ACT-->>H: STOP
    H->>LOG: "partition stopped after stage-2 instruction abort"
    H->>H: park this CPU at EL2 (wfe loop), other CPUs run on
  else anything else
    ACT-->>H: NOT_S2: the normal handlers
  end
```

- Both outcomes are observed: the skipped data abort in every partition B
  check, the stopped partition in case M3 of case set v3
  (`docs/methodology/stage2/20261102-a1-cases-v3/runs/M3/`).
- For a direct stage-2 permission fault the IPA in HPFAR_EL2 may be
  UNKNOWN (Arm ARM), so HAVEN judges permission faults by ESR and FAR.
