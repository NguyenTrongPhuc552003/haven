# GPU partition on CIX P1: design note

**Status:** proposed design (2026-11-01). Nothing here is implemented or
observed on hardware. Facts are labelled: **[F]** primary or upstream source,
**[S]** community or single source, **[I]** inference. Platform evidence and
sources: [`../porting/cix-p1-ledger.md`](../porting/cix-p1-ledger.md).

## 1. Why a GPU belongs in this thesis

The visible demo is a Linux partition rendering on the GPU while a real-time
partition keeps its timing. The research value is elsewhere: on CIX P1 the
GPU appears not to sit behind any SMMU stream under the shipped ACPI
firmware [S, two independent community sources, vendor silent]. If that
holds, GPU DMA bypasses every stage-2 and SMMU grant HAVEN can make: what is
granted is not what is enforced. That is the A1 question on real hardware.

## 2. Platform facts that shape the design

| Fact | Evidence | Consequence |
| - | - | - |
| GPU is Arm Immortalis-G720 MC10 (arch v12, CSF) | [F] | Panthor (Linux 6.18+) or vendor kbase r54p1 |
| GPU (ACPI `CIXH5000`) has no IORT entry and no IOMMU group | [S] | GPU DMA is untranslated; GPU partition must be trusted for DMA |
| GPU power, clocks and DVFS go through SCMI; DVFS churn can wedge the shared mailbox | [S] | The partition that owns the GPU should also own SCMI |
| GPU access is gated by an EL3 interconnect grant (IDM) | [S] | GPU security state is invisible at EL2 |
| Upstream Linux needs the CIX or Sky1-Linux patch set for GPU and display | [F] | Pin kernel 7.0 or 7.1 with CIX patches |

## 3. Proposed architecture

| Partition | CPUs | Owns | Memory |
| - | - | - | - |
| Linux (general purpose) | A720 cluster 1 and A520s | GPU, display, SCMI, PCIe, network | Identity-mapped (IPA = PA), because untranslated GPU DMA uses physical addresses [I] |
| Real-time (bare metal) | 2 to 4 cores of A720 cluster 2 | Timer, one UART | Cache-coloured where possible; no shared devices |
| HAVEN | all, at EL2 | stage-2, SMMU, GIC configuration | Its own image |

HAVEN's read-back module (A1) reads the enforced state of every partition:
stage-2 tables, SMMU stream table entries and GIC routing. It compares that
state with the grant and also lists **unenforced DMA masters**: devices
assigned to a partition that can do DMA but have no SMMU stream table entry,
or have a bypass entry.

## 4. QEMU analogue (before any board)

In QEMU `virt` with `iommu=smmuv3`, the SMMU sits in front of the PCIe root
complex only. Observed 2026-11-01 (QEMU 11.1.3, `dumpdtb`): the PCIe node has
`iommu-map` to `smmuv3@9050000`, and none of the 32 `virtio_mmio` nodes has an
`iommus` property. A virtio-mmio device assigned to a partition is therefore a
faithful stand-in for the CIX GPU: a DMA master the SMMU cannot see.

## 5. Experiments

| ID | Where | Question | Pass condition |
| - | - | - | - |
| G1 | QEMU | Does SMMU read-back list a PCIe device as enforced and a virtio-mmio DMA master as unenforced? | Both classified correctly; no false "enforced" |
| G2 | Board | Does the shipped IORT omit `CIXH5000` and `CIXH502F`? Do the SMMUs hold any stream for them? | `acpidump`/`iasl` output and SMMU stream table dump recorded per firmware version |
| G3 | Board | Can HAVEN install a stream for the GPU, and does the GPU still work? | Recorded either way; a negative result is a finding |
| G4 | Board | Real-time partition jitter under glmark2 or vkcube plus a CPU memory hog, with and without mitigation | Jitter distribution per configuration, repeated runs |

Mitigation levers for G4: real-time partition cache colouring, CPU-side
bandwidth regulation of the Linux cores (PMU budgets), and a GPU frequency cap
through SCMI. No GPU memory-bandwidth throttle is documented for CIX P1, so
published work (Bao, BWLOCK++) suggests regulating CPUs, not the GPU [F].

**Fallback:** if display or Panthor regress, keep G1 to G3 and run G4 with a
headless Vulkan compute load as the aggressor.

## 6. Open questions only hardware can answer

1. IORT contents per firmware version; whether the MMU-700 has a TBU path for
   the GPU that firmware leaves unconfigured.
2. `HCR_EL2` (E2H) and ID registers at HAVEN entry.
3. Whether a non-root partition can reset and re-enable the GPU without EL3
   RAS storms.
4. Which A720 and A520 PMU events count reliably for bandwidth regulation.
5. L3 and system-cache set geometry, and whether GPU traffic shares it.
