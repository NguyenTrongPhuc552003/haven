# L4 results: KDAL in the Linux partition, demand check and read-back

**Run:** 2026-11-06, macOS, QEMU virt (cortex-a72, qemu-direct profile,
try bundle's virtio-rng-pci behind the SMMU), `run_l4.sh IMAGE KDAL_KO
KDAL_SRC runs/l4`. Inputs (SHA-256 in `runs/l4/SHA256SUMS`): ELMOS's
`demo-linux-v6.18-1` Image and `kdal.ko` (kdal `6701e5a`), KDAL's CI init
and cpio writer, `partition-a-l4.dts` (written by the script, kept in
`runs/l4/`), HAVEN at this record's commit. Raw output: `runs/l4/uart.log`
(264 lines), `demand.json`, `demand-check.txt`, `readback.txt`,
`readback.json`. Design and expectations: `PLAN.md` section 8 and
`grant-l4.json`, committed before the run (`44e1235`).

## Observations

- **Linux reached user space** from the initramfs (`Run /init as init
  process`, uart.log line 239) and loaded KDAL (`KDAL-CI: insmod ok`, line
  255). KDAL's device-tree backend found `kdal-gpio@9030000` (line 246).
  KDAL's qemu backend also registered its simulated uart0, i2c0, spi0 and
  gpu0 devices (lines 247 to 254), which report no hardware demand.
- **D (demand)**, printed between the markers (lines 256 to 260):

  ```
  {"description": "kdal-gpio@9030000", "base-address": "0x9030000", "pages-count": 1,
   "attributes": "0x3", "interrupts": [{"id": 39}]}
  ```

  identical to `../20261104-ffa-grant/kdal-demand-qemu.json`, which came
  from the same node on a Linux without HAVEN.
- **D against G** (`demand-check.txt`, exit 1):
  - `unmet MMIO demand (0x9030000-0x9031000 not granted to partition 1)`
  - `unmet interrupt demand (INTID 39 not granted to partition 1)`
  - not demanded (information): A UART, A PCI config, A PCI BAR.
- **No access was denied**: no `Denied stage-2 violation`, no decoded
  access, no trapped system register, no undelivered interrupt (0 lines).
- Linux powered off: `PSCI SYSTEM_OFF`, then `virtual interrupts delivered
  on CPU 0: 5 x120 27 x107`, `partition stopped by PSCI OFF` (lines 262 to
  264).
- **H against G** (read-back, read at `KDAL-DEMAND-END`): `enforced state
  matches the grant`. Partition A's stage-2 holds RAM, UART, PCI config and
  BAR and the read-only window as granted; SPI 33 is enabled (Linux's PL011
  driver enabled it through the emulated distributor), SPI 39 is not; CPU 0
  has PPIs 26 (HAVEN's timer) and 27 (Linux's timer); the SMMU translates
  the virtio device's stream 8. Read-back also lists three observations
  (partition A's device pages are mapped at EL2 too), which it reported
  before L4 and which are not findings under the grant.

## Against the expectations (PLAN.md section 8)

| # | Expectation | Result |
| - | - | - |
| 1 | insmod ok; demand is exactly the GPIO region with interrupt 39 | held |
| 2 | demand check: two findings (MMIO, interrupt), exit 1 | held |
| 3 | no denied access (KDAL only ioremaps) | held |
| 4 | SYSTEM_OFF stops A; interrupt 27 delivered | held (27: 107) |
| 5 | read-back matches `grant-l4.json`; SPI 39 not enabled | held |

## Interpretation (inference)

This is the first time the whole chain ran on one system: the guest
description (T) gave KDAL a device the grant does not contain, KDAL
reported it as demand (D), the check against the grant (G) caught it, and
read-back confirmed that the enforced state (H) still matches G. The
mismatch was found **without any access happening**: KDAL never touched
the GPIO, so HAVEN's enforcement had nothing to deny and a fault log would
show nothing. That is the configuration-assurance gap the charter's
section 14 names: the demand check sees the disagreement before it
becomes a fault, and read-back shows the isolation held regardless.

## Limits

- One mismatch class (an ungranted device in the guest description).
  Seeded cases for the other relations (D within E, T exactly G) are the
  next step and need owner-written expectations.
- `grant-l4.json` is Claude's expected configuration, not owner-approved;
  read-back agreeing with it shows HAVEN does what Claude expected from
  `partition.c`, which is weaker than agreeing with an independent oracle.
- qemu-direct: partition B is configured but not started; the grant says so.
- KDAL's simulated qemu-backend devices report no demand; whether they
  should is a KDAL design question.
