# Trusted code added by the Linux partition (L1 to L3)

**Measured:** 2026-11-06, macOS, cloc 2.10. Asked for by the research
charter, section 14 ("new risk"), approved by the owner the same day.

## Method

`tools/scripts/tcb-image-loc.py BUILD` counts the files the hypervisor
image `haven.elf` is built from: the compiler's dependency records for
the `haven` target (Ninja), restricted to the repository's `.c`, `.h` and
`.S` files, counted by cloc as code lines (no blanks, no comments). Demo
guests, host tests and tools are not part of the image and are not
counted. Code under an `#ifdef` that is off still counts, so the default
and Linux builds give the same totals.

Both builds: `arm64-qemu` preset, qemu-direct, with and without
`-DHAVEN_PART_A_DTB_IPA=0x50000000`.

| Commit | What | Files | Code lines |
| - | - | - | - |
| `667e6c1` | before L1 (L0: Linux boots, nothing emulated) | 56 | 5,960 |
| `665f7f9` | after L4 (PSCI, MMIO decode, GICv3 model, L4 tooling) | 62 | 6,719 |
| | **difference** | +6 | **+759 (+12.7 %)** |

Per file (code lines):

| File | Before | After | Change | Why |
| - | - | - | - | - |
| `src/core/irq/vgic.c` | 0 | 234 | +234 | GICD and GICR model (L3) |
| `src/core/exc/mmio.c` | 0 | 118 | +118 | syndrome decode, dispatch (L2) |
| `src/core/exc/el2_exceptions.c` | 275 | 390 | +115 | HVC/SMC, MMIO and system register traps (L1 to L3) |
| `src/core/exc/psci.c` | 0 | 102 | +102 | PSCI 1.0 (L1) |
| `src/core/partition.c` | 320 | 368 | +48 | Linux profile: PPI 27, GIC model setup (L3) |
| `include/haven/vgic.h`, `psci.h`, `mmio.h` | 0 | 91 | +91 | interfaces of the three modules |
| `arch/arm64/irq.c` | 269 | 293 | +24 | delivery counters (L3) |
| `arch/arm64/entry.S` | 140 | 150 | +10 | x19 to x28 in the frame (L2) |
| `arch/arm64/include/asm/sysregs.h`, `page.h` | 303 | 320 | +17 | formatting only (`UINT64_C`, Windows fix `d9c2c61`) |

## Observations

- The functional growth is about 742 code lines (759 minus 17 of
  formatting), of which 454 are the three new modules and their headers.
- The repository's own directory counter (`tools/scripts/tcb-loc.sh`,
  with cloc installed) reports 6,044 lines and warns that this exceeds its
  5,000-line target. It counts whole directories (including headers the
  image does not use, such as the guest UART and FreeRTOS headers) and
  misses headers outside them; the image-based count above is the one to
  use for the thesis.
- Before L1 the image was already above the 5,000-line target (5,960).

## Interpretation (inference)

The research question's "without growing HAVEN's trusted code" needs a
precise reading: the read-back tool itself adds nothing to the image
(it runs outside), but supporting a Linux partition grew the image by
12.7 %. Two ways to keep the question honest, for the owner to decide:

1. measure read-back's contribution separately (zero lines in the image)
   and report the Linux support as the price of a realistic guest, with
   this table; or
2. make the Linux-guest code a build option (it is already partly behind
   `HAVEN_PART_A_DTB_IPA`), so the default image stays at 5,960 lines plus
   PSCI, and report both images.

For comparison (source-derived, Martins and Pinto, RTAS 2023, different
counting tool): Bao 8,423 SLoC, Jailhouse 9,929 plus a 2,180-line Linux
driver, Xen 67,342.

## Decision (2026-11-06, Claude, delegated by the owner)

Option 1. Read-back is measured separately (it adds 0 lines to the image:
it runs outside HAVEN), and the Linux-guest support is reported as its own
line item, the cost of a realistic guest: 759 code lines, 12.7 % of the
image (this table). Option 2 was not taken because it changes code and
adds a second image to test just before the Linux chain case set is
frozen, for a reporting gain the table already gives. The thesis states
the research question's "without growing HAVEN's trusted code" as
"read-back adds no code to HAVEN's image", and reports the image's size
by feature group with this tool.
