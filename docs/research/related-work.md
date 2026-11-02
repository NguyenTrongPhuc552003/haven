# Related work for A1 (enforcement read-back): literature matrix

**Status:** living record, started 2026-11-03 by Claude from an external
survey and a targeted search. Each row says what the work checks, from
where, when, and how it differs from HAVEN's read-back. Rows marked
"(not read in full)" are taken from abstracts or summaries and must be
read before the thesis cites them for a specific claim.

HAVEN's read-back, for comparison: a tool outside HAVEN's trusted code
stops the machine and reads the enforced state of three Arm structures
(stage-2 translation tables, SMMUv3 stream table entries and their
stage-2 tables, GICv3 routing, enable and priority state, plus HAVEN's
own EL2 map), then compares it semantically with the declared grant.
Evaluated with seeded fault case sets and randomly drawn held-out faults
(`docs/methodology/stage2/20261102-a1-cases-v3/`).

## Matrix

| Work | What is checked | Vantage point and time | Arm? | Difference from HAVEN read-back |
| - | - | - | - | - |
| HyperSentry, Azab et al., ACM CCS 2010 | integrity of hypervisor code and data | SMM, triggered out of band (IPMI), periodic | no (x86) | measures code integrity, not enforced configuration against a policy |
| HyperCheck, Wang, Stavrou, Ghosh, RAID 2010; IEEE TDSC 2014 | static hypervisor code and CPU registers | SMM, analysed on a remote machine | no (x86) | same family as HyperSentry |
| **IOCheck**, F. Zhang, 2013 (preliminary; not read in full) | firmware of I/O devices; PCI configuration space; **IOMMU configuration: root-entry table register, domain mapping and page tables, DMAR table, checked to stay unchanged** | SMM, periodic | no (x86 VT-d) | **closest prior idea for the SMMU part.** Differences: checks that static state does not change (hash-style), not that it matches a declared grant; IOMMU only, no CPU stage-2 or interrupt controller; no evaluation beyond SMM switch time |
| SeKVM / VRM, Tao et al., SOSP 2021 | proofs that KCore's stage-2 and SMMU page table code preserves isolation on Arm relaxed memory | design time, formal | yes | proves the code; read-back observes the state the hardware enforces at run time, including faults outside the proven code (configuration, platform data, firmware) |
| Nemati, "Secure System Virtualization: End-to-End Verification of Memory Isolation", KTH thesis, arXiv 2005.02605 (not read in full) | formal verification of memory isolation of an ARMv7 hypervisor and its MMU configuration | design time, formal | yes (ARMv7) | as SeKVM: proof versus observation |
| ACAI, Sridhara et al., USENIX Security 2024 | the monitor checks every update of the SMMU stream table and stage-2 tables for aliasing across streams | inline mediation of updates, Arm CCA | yes | prevents bad updates in the update path (inside the trusted code); read-back is an independent check after the fact and outside it |
| virtCCA, arXiv 2306.11011 (not read in full) | attestation of confidential VM creation steps; stage-2 management | creation time, TrustZone S-EL2 | yes | attests creation-time configuration, not drift after start |
| Hafnium and TF-A FF-A manifests (documentation) | the SPMC configures stage-2 and SMMUv3 from a partition's declared memory, device regions, SMMU and stream IDs | load time, S-EL2 | yes | defines a declared-grant model (adopted as HAVEN's grant vocabulary); does not read back what was enforced |
| Cesarano, Cotroneo, De Simone, "Towards Assessing Isolation Properties in Partitioning Hypervisors", arXiv 2209.00405, 2022 | framework: test partitions stress the hypervisor (fuzzing, fault injection) while a run-time monitor checks whether regular partitions are affected; mapped to ISO 15408 | run time, behavioural | generic | observes effects on partitions; does not read the enforcement structures. Its premise matches A1's finding that behavioural checks miss faults that read-back finds |
| Lengyel et al., VMI with Xen on Arm (2015); Proskurin et al., ACSAC 2018 (not read in full) | virtual machine introspection of guests on Arm | hypervisor looks into guests | yes | the opposite direction: the hypervisor inspects guests, not an outside tool inspecting the hypervisor's enforcement |
| RealmEye, arXiv 2608.12822, 2026 (not read in full) | introspection of Arm CCA realm VMs | run time | yes | guest introspection under CCA; same direction remark |
| Commercial debuggers with hypervisor awareness (for example Lauterbach TRACE32) | display guest and hypervisor MMU tables through JTAG | stopped target | yes | industrial practice of an external view; no grant comparison or drift classes. Relevant to the board method (read-back over the Orange Pi 6 Plus JTAG header) |
| pKVM SMMUv3 (Mostafa, LKML v8, September 2026; LPC 2026) | hypervisor-owned SMMUv3 shadowing the CPU stage-2 | design trend | yes | makes "SMMU stage-2 consistent with CPU stage-2" a live invariant, exactly the kind of property read-back can check |
| Rajapaksha et al., IOMMU deferred invalidation (Boston University; URL indicates DATE 2024) | stale IOTLB entries allow DMA after unmap under batched invalidation | run time | generic | **threat to validity for read-back:** it reads tables in memory, not TLB or IOTLB contents; a stale cached translation is invisible to it (record as a blind spot) |

## Interference and partitioning on Arm (for the HOW WELL chapter)

| Work | Use |
| - | - |
| Martins, Pinto, "Shedding Light on Static Partitioning Hypervisors for Arm-based Mixed-Criticality Systems", IEEE RTAS 2023 | baseline comparison of Bao, Jailhouse, Xen and others; notes the lack of support for DMA traffic management in static partitioning hypervisors |
| Martins et al., "Bao", NG-RES 2020 | closest design to HAVEN |
| Costa et al., "SP-IMPact", arXiv 2501.16245 (not read in full) | interference evaluation for static partitioning |
| Oliveira et al., "H-MBR", arXiv 2502.02437 (not read in full) | hypervisor-level memory bandwidth reservation (a countermeasure in the PLAN matrix) |
| "Beyond the Bermuda Triangle of Contention: IOMMU Interference in Mixed Criticality Systems", arXiv 2508.19670 (not read in full) | SMMU as a source of interference; relevant to CIX P1's PCIe SMMU |
| Omnivisor, ECRTS 2024 | static partitioning over heterogeneous cores (A720 and A520 on CIX P1) |

## Novelty statement (inference, to keep under review)

No work found so far reads back, at run time and from outside the
hypervisor's trusted code, all three Arm enforcement structures (CPU
stage-2, SMMUv3, GICv3) and compares them with a declared grant to find
drift. The closest idea is IOCheck (x86, IOMMU only, unchanged-state
check, preliminary). The thesis should therefore claim the combination
and the evaluation (drift classes that unit tests and boot checks miss,
scored against expectations fixed beforehand), not the idea of checking
IOMMU tables from outside. A negative search is not proof: repeat the
search before submission (terms: configuration attestation, hypervisor
introspection Arm, SMMU verification, partition configuration checking).

## Sources

- Survey report, 2026-11-03 (external research task, summarised in `stages/ECOSYSTEM.md`).
- [Cesarano et al., arXiv 2209.00405](https://arxiv.org/pdf/2209.00405)
- [IOCheck (Zhang)](https://cse.sustech.edu.cn/faculty/~zhangfw/paper/iocheck-dsn13.pdf)
- [HyperCheck, IEEE TDSC 2014](https://cse.sustech.edu.cn/faculty/~zhangfw/paper/hypercheck-tdsc14.pdf)
- [SeKVM, SOSP 2021](https://www.cs.columbia.edu/~rgu/publications/sosp21-tao.pdf)
- [ACAI, USENIX Security 2024](https://www.usenix.org/conference/usenixsecurity24/presentation/sridhara)
- [virtCCA, arXiv 2306.11011](https://arxiv.org/pdf/2306.11011)
- [Nemati, arXiv 2005.02605](https://arxiv.org/pdf/2005.02605)
- [RealmEye, arXiv 2608.12822](https://arxiv.org/pdf/2608.12822)
- [IOMMU deferred invalidation](https://bu-icsg.github.io/publications/2024/iommu_date_2024.pdf)
- [Hafnium secure partition manager](https://hafnium.readthedocs.io/en/v2.12-rc0/secure-partition-manager/secure-partition-manager.html)
- [pKVM SMMUv3 v8 series](https://patchew.org/linux/20260922131259.2975334-1-smostafa@google.com/20260922131259.2975334-13-smostafa@google.com/)
- [Lauterbach hypervisor debugging flyer](https://www2.lauterbach.com/download/pdf/flyer/lauterbach-hypervisor-flyer-v2.00.pdf)
- [Omnivisor, ECRTS 2024](https://drops.dagstuhl.de/entities/document/10.4230/LIPIcs.ECRTS.2024.7)
- [Martins and Pinto, RTAS 2023](https://ui.adsabs.harvard.edu/abs/2023arXiv230311186M/abstract)
- [IOMMU interference, arXiv 2508.19670](https://arxiv.org/pdf/2508.19670)
