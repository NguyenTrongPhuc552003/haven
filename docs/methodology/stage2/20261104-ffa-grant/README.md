# Grants in the FF-A vocabulary (format 2)

**Status:** done 2026-11-04 (owner approval 2026-11-03, item 5 of the
survey follow-up). Claude's change; read-back's comparison logic is
unchanged.

## Why

Arm's FF-A partition manifest binding (Hafnium, TF-A) already defines how
a partition declares memory regions, device regions, SMMU stream IDs,
DMA permissions and interrupts. Writing HAVEN's grant in the same
vocabulary makes it comparable with KDAL's demand (KDAL audit K4) and with
a published Arm format instead of a HAVEN-only one (survey,
`stages/ECOSYSTEM.md`; definitions checked against the Hafnium binding
document, v2.11).

## What changed

- `tools/readback/grant.py`: format 1 (HAVEN's own) to format 2 and back;
  `load()` accepts both and returns format 1, so the frozen case sets
  (all format 1) are read unchanged. The module docstring lists the
  mapping, the `haven,` extensions and one deliberate deviation (a memory
  region with `stream-ids` is a DMA window inside a CPU-mapped region, not
  a second CPU mapping).
- `readback.py` reads grants through `grant.load()`.
- `test_grant.py`: every format 1 grant in the repository (15 files)
  survives format 1 to 2 to 1 with equal values (region order is not
  compared; it changes only the order of findings); FF-A attribute
  encodings; a DMA window outside the partition's memory is rejected.
  Runs in CI (host tests job).
- The try bundle ships `grant.py` next to `readback.py`.

## Check (observation, 2026-11-04, macOS, QEMU cortex-a72)

`grant-budget-ffa.json` is `grant-budget.json` converted with
`python3 tools/readback/grant.py ../20261102-budgets/grant-budget.json`.
Both grants, against both try-bundle images:

| Image | grant.json (format 1) | grant-budget-ffa.json (format 2) |
| - | - | - |
| good | matches the grant | matches the grant |
| fault-el2-maps-b | 1 finding: partition 2 memory mapped at EL2 | the same finding |

## Limits

- Format 1 does not say which device an SPI belongs to, so the converter
  puts SPIs on a pseudo device region named "interrupts" with no
  registers. Grants written in format 2 by hand should attach each
  interrupt to its device region.
- `pages-count` assumes 4 KiB pages.
- This is a JSON rendering of the binding's properties, not a device
  tree; a DT form (for a board's firmware or for KDAL) is a later step.

## KDAL's demand against a grant (2026-11-04)

KDAL now reports each device's resource demand in the same vocabulary
(`/sys/kernel/debug/kdal/demand`, kdal `6701e5a`), and
`tools/readback/demand_check.py` checks a demand against a grant (unmet
MMIO, insufficient access, unmet interrupt, unmet DMA; granted regions
nothing demands are listed as information). Unit tests:
`tools/readback/test_demand_check.py` (CI).

Observation, QEMU `virt` (Linux v6.18 arm64, KDAL's CI run): a
`kdal,device` node over the PL061 GPIO produced
`kdal-demand-qemu.json` (MMIO 0x9030000, 1 page, read and write; INTID
39). Checked against two example grants for a Linux partition 3 (Claude's
examples; HAVEN has no Linux partition yet, so neither grant is
enforced):

| Grant | Result |
| - | - |
| `grant-linux-gpio.json` (GPIO MMIO and SPI 39 granted) | every demand met, exit 0 |
| `grant-linux-gpio-noirq.json` (MMIO only) | 1 finding: unmet interrupt demand (INTID 39), exit 1 |

This closes the D side of the charter's E/D/G/T/H chain in QEMU at the
level of files: demand (D) and grant (G) are now comparable. What is
still missing is the live chain: KDAL running inside a HAVEN Linux
partition, with read-back (H) on the same grant.
