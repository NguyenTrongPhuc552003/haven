# Primary-source screen for the Stage 1 candidates

This is a focused screen, not an exhaustive novelty review. No “first”,
unprecedented isolation, verified hardware binding or new real-time guarantee
is established. Published systems are prior work, not additional thesis
implementation repositories to clone or depend on.

| ID / source                                                                                                                                                                                                                        | Verified statement and implication                                                                                                                                                                                                                                                                                                 |
| ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| P1 - Martins et al., [Bao, NG-RES2020](https://drops.dagstuhl.de/entities/document/10.4230/OASIcs.NG-RES.2020.3), DOI10.4230/OASIcs.NG-RES.2020.3                                                                                  | Static partitioning and a small standalone hypervisor architecture are established work. Merely adding static resource assignment or running Linux under HAVEN is not a research distinction. This publication does not establish current Bao behavior or a deficiency in its latest release.                                      |
| P2 - [Jailhouse v0.12 cell-config.h](https://raw.githubusercontent.com/siemens/jailhouse/v0.12/include/jailhouse/cell-config.h), `jailhouse_memory`, `jailhouse_cell_desc`, `jailhouse_irqchip` and stream-ID/configuration layout | Memory regions/permissions, CPU sets and interrupt/device configuration already have concrete static representations. A new HAVEN manifest, range check or device ownership field is not enough by itself. The tag pins the cited source; no performance comparison or copied implementation is proposed.                          |
| P3 - Boyton et al., [Formally Verified System Initialisation, ICFEM2013](https://www.trustworthy.systems/publications/nictaabstracts/Boyton_ABFGGKLS_13.abstract), DOI10.1007/978-3-642-41202-8_6                                  | Describes an automatic verified initializer from declarative desired state to components, channels and access rights, connected to seL4 correctness. Configuration-to-authority assurance is already established, with stronger formal claims than HAVEN currently supports.                                                       |
| P4 - Weibel et al., [Verifying the seL4 Microkit](https://trustworthy.systems/publications/papers/Weibel_KPSSH_24.abstract), technical report v1.0, June2024                                                                       | Reports Microkit library verification and verified SDF-to-capDL access-right mapping. Candidate A must investigate a narrower backend/binding/fault-coverage question. Do not claim verified configuration generation is new or that this report proves every hardware binding.                                                    |
| P5 - [Official seL4 API, Revoke](https://docs.sel4.systems/projects/sel4/api-doc.html#revoke)                                                                                                                                      | `seL4_CNode_Revoke` removes descendant capabilities. Generic capability revocation is existing work. The documented API operation does not by itself show physical-device quiescence, draining DMA or correct device reset.                                                                                                        |
| P6 - [Official Linux VFIO documentation](https://docs.kernel.org/driver-api/vfio.html), isolation/group semantics, device regions/IRQ interfaces and reset example                                                                 | Device assignment already treats isolation granularity and DMA ownership as explicit conditions and exposes reset/interrupt interfaces. A KDAL name/handle or resource lease must not be equated with hardware containment. This is an interface reference, not a benchmark baseline or proof that a selected board supports VFIO. |

All links were inspected through primary publisher/author/project sources in
this task. P5/P6 are live documentation read at this checkpoint; implementations
must pin the applicable kernel/API version before using them as an experimental
input. P1/P3 have stable publication identifiers; P4 is identified as a report,
not silently described as a peer-reviewed hardware proof. P2 is release-tagged.

The plausible distinctions below are hypotheses: measured coverage of
inconsistencies spanning backend demand and actual enforcement, or a bounded
resource lifecycle protocol evaluated under specified failures. The sources
above show why broad static-configuration/revocation claims would be weak;
they do not demonstrate absence of all prior work on either narrower topic.
The early rejection study must establish a worthwhile distinction before a
large runtime implementation is justified.
