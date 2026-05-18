# i.MX95 Evidence Capture Methodology

## Overview

This document defines the procedure for capturing, archiving, and validating
isolation evidence from the Haven hypervisor running on the i.MX95 Development
Kit.  Evidence bundles produced by this procedure serve as primary artefacts for
thesis Chapter 5 (Evaluation) and are cross-referenced by the traceability
matrix in `docs/safety/THREAT_MODEL.md`.

All captures must be reproducible from a clean flash of the signed firmware
image.  A capture that cannot be reproduced under identical hardware and
software conditions is not accepted as thesis evidence.

---

## Evidence Categories

| ID  | Category           | Acceptance criterion                                           |
| --- | ------------------ | -------------------------------------------------------------- |
| E1  | Boot log           | Haven prints its banner and enters the partition dispatch loop |
| E2  | Spatial isolation  | Stage-2 fault fires on cross-partition memory access           |
| E3  | Temporal isolation | RTOS P99 deadline miss rate < 0.1 % under 100 % Linux CPU load |
| E4  | DMA isolation      | SMMUv3 fault fires on out-of-window DMA descriptor             |
| E5  | Fault injection    | All 8 fault scenarios (F1–F8) report DENY in the audit log     |

---

## Hardware Setup

### Required equipment

- i.MX95 Development Kit (NXP part IMX95-19x19-EVK or equivalent)
- USB-to-UART adapter connected to J13 (LPUART1, 115200 8N1)
- JTAG/SWD probe (J-Link or similar) for fault injection campaigns
- Host workstation running Linux (Ubuntu 22.04 LTS recommended)
- Cross toolchain: `aarch64-unknown-linux-gnu-gcc` 12 or later

### Power-on checklist

1. Set boot mode switches to eMMC (SW1 = 0b0010 on the Dev Kit).
2. Connect UART adapter; confirm `/dev/ttyUSB0` enumerated on host.
3. Flash the signed U-Boot + ATF + Haven image:
   ```
   uuu -b emmc_all imx-boot-imx95-devkit.bin haven.itb
   ```
4. Open serial console:
   ```
   screen /dev/ttyUSB0 115200
   ```
5. Power cycle the board and confirm U-Boot hands off to Haven.

---

## Running the Evidence Suite

### Build for i.MX95

```bash
make ARCH=arm64 PLATFORM=imx95-devkit \
     CROSS_COMPILE=aarch64-unknown-linux-gnu- \
     all
```

The resulting ELF and ITB images are placed in `build/imx95-devkit/`.

### Package evidence

After a successful hardware run, collect serial logs and copy them into
`build/evidence/imx95/logs/`, then invoke:

```bash
./scripts/package-evidence.sh --platform imx95
```

The script creates a self-contained evidence bundle under
`build/evidence/imx95/` and writes a SHA256 manifest.

### Manual capture steps

For each evidence category, the following commands are run on the board or
logged from the serial console:

**E1 - Boot log**
Captured automatically from UART output at power-on.  Save to
`build/evidence/imx95/logs/boot.log`.

**E2 - Spatial isolation**
Deploy the `test_spatial_isolation` binary into Partition B.  Trigger a
write to a Partition A physical address.  The hypervisor must log a stage-2
data abort within 5 µs of the faulting instruction.  Save fault log to
`build/evidence/imx95/logs/spatial_fault.log`.

**E3 - Temporal isolation**
Run `tests/isolation/test_temporal_isolation` in Partition B while a
stress-ng CPU-burn workload runs at 100 % utilization in Partition A.
Record 10,000 deadline samples.  Export to
`build/evidence/imx95/metrics/temporal_deadlines.csv`.

**E4 - DMA isolation**
Program a DMA descriptor pointing outside the SMMU window for Partition B.
The SMMUv3 must raise a transaction fault.  Save the SMMU fault status
register dump to `build/evidence/imx95/captures/smmu_fault.txt`.

**E5 - Fault injection**
Execute `tests/integration/test_fault_injection` with JTAG breakpoints
set at each fault-injection site (F1–F8).  Each scenario must produce a
DENY entry in the hypervisor audit ring buffer.  Export audit log to
`build/evidence/imx95/captures/fault_audit.log`.

---

## Evidence JSON Schema

Each discrete test result is written as a single JSON object conforming to
this schema before being aggregated into `summary.json`:

```json
{
  "platform": "imx95-devkit",
  "timestamp": "<ISO-8601 UTC>",
  "git_commit": "<40-char SHA>",
  "test_name": "<E1|spatial_isolation|temporal_isolation|dma_isolation|fault_injection>",
  "result": "<PASS|FAIL|SKIP>",
  "details": {
    "latency_us": 0,
    "deadline_miss_pct": 0.0,
    "fault_id": ""
  }
}
```

Fields within `details` are test-specific; unused fields may be omitted.

---

## Archive Location and Manifest

Evidence bundles are stored in `build/evidence/imx95/` with the following
layout:

```
build/evidence/imx95/
  logs/           # UART boot and runtime logs
  metrics/        # Latency and deadline CSV files
  captures/       # SMMU dumps, fault audit logs, serial captures
  manifest.sha256 # SHA256 of every file in this bundle
  summary.json    # Aggregated pass/fail with git commit and timestamp
```

The `package-evidence.sh` script populates `manifest.sha256` using:

```bash
find build/evidence/imx95 -type f | sort | xargs sha256sum > build/evidence/imx95/manifest.sha256
```

The manifest must be committed alongside the evidence bundle tag so that
the thesis examiner can independently verify file integrity.

---

## Acceptance Criteria

All of the following conditions must hold before an evidence bundle is tagged
as a thesis milestone artefact:

- **Stage-2 fault latency (E2):** maximum observed latency < 5 µs.
- **RTOS deadline miss rate (E3):** P99 miss rate < 0.1 % under 100 % Linux
  CPU load over 10,000 samples.
- **DMA isolation (E4):** zero out-of-window transactions reach DRAM; all
  blocked transactions appear in the SMMU fault log.
- **Fault injection (E5):** all 8 scenarios (F1–F8) produce a confirmed DENY
  in the audit log with no false negatives.
- **Build reproducibility:** `sha256sum haven.elf` matches the value recorded
  in `build/evidence/imx95/summary.json` for the same git commit.

Any failing criterion blocks milestone sign-off and must be resolved with a
new evidence capture run before thesis submission.

---

## Relationship to Thesis Milestones

| Milestone        | Evidence required                 |
| ---------------- | --------------------------------- |
| Phase 3 M8       | E1, E2 (QEMU baseline)            |
| Phase 3 M9       | E3, E4 on real i.MX95 hardware    |
| Phase 4 M10      | E5 full fault injection campaign  |
| Final submission | Complete bundle, all criteria met |
