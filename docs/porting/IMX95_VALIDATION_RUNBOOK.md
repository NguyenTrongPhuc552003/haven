# i.MX95 Validation Runbook

Primary target board: NXP i.MX95 Dev Kit (8 Cortex-A55 + 1 Cortex-M33)  
Thesis chapter: Chapter 6 - Platform Evaluation and Evidence

---

## Prerequisites

### Hardware

| Item               | Required version / notes              |
| ------------------ | ------------------------------------- |
| NXP i.MX95 Dev Kit | EVK board revision B1 or later        |
| USB-C power supply | 5 V / 3 A minimum                     |
| USB-UART cable     | Connected to J23 (Debug UART)         |
| USB Type-A cable   | Connected to J17 (UUU download)       |
| Host with `uuu`    | NXP Universal Update Utility ≥ 1.5.21 |

### Software

```bash
# Verify uuu is in PATH
uuu --version            # expect: uuu, Version: 1.5.21 or newer

# Verify cross-compiler toolchain
aarch64-none-elf-gcc --version   # expect: GCC 12 or newer

# Verify QEMU for pre-flight
qemu-system-aarch64 --version    # expect: QEMU 8.x
```

---

## Step 1 - Build the Haven image

```bash
cd /path/to/haven

# Configure for i.MX95
cmake -B build-imx95 \
      -DCMAKE_TOOLCHAIN_FILE=cmake/arm64.cmake \
      -DHAVEN_PLATFORM=imx95-devkit \
      -DHAVEN_CONFIG=configs/arm64/imx95-devkit.yaml

# Build
cmake --build build-imx95 --parallel $(nproc)

# Expected output:
#   build-imx95/haven.elf
#   build-imx95/haven.bin
ls -lh build-imx95/haven.bin
```

---

## Step 2 - Flash via UUU

1. Put the board in **download mode**: hold **SW1** while applying power.
2. Connect the USB-A download cable to J17 and the host.
3. Run UUU:

```bash
uuu -b emmc_all \
    imx-boot-imx95evk-sd.bin-flash_singleboot \
    build-imx95/haven.bin
```

4. Power-cycle the board after UUU completes (remove the download-mode jumper).

---

## Step 3 - Connect the UART console

```bash
# Find the serial device (macOS example)
ls /dev/tty.usbserial-*         # pick the console port (usually first)

# Open at 115200 baud
screen /dev/tty.usbserial-XXXX 115200

# Linux alternative
picocom -b 115200 /dev/ttyUSB0
```

---

## Step 4 - Expected boot output

```
Haven Hypervisor v0.1.0-dev (arm64 imx95-devkit)
EL2 init: HCR_EL2=0x80000031 VTCR_EL2=0x80023558
Platform: NXP i.MX95 Dev Kit  CPUs: 8  RAM: 3840 MB
SMMUv3: 256 stream IDs, linear table, ABORT policy active
GICv3: GICD/GICR init OK, 160 SPI, 16 LR
Partition A [Linux 6.6]: IPA=0x80000000 size=512M CPUs=0-5 budget=8ms
Partition B [FreeRTOS]:  IPA=0x80000000 size=64M  CPU=6  budget=2ms
Launching partitions...
[PartA] Linux booting ...
[PartB] FreeRTOS task loop running, period=10ms
```

If boot hangs:
- Check UUU completed without error
- Verify `imx-boot` image is the correct i.MX95 variant
- Re-check J23 UART cable orientation

---

## Step 5 - Spatial isolation tests

```bash
# Connect to partition A Linux console via virtio-uart (ch1)
# (Replace /dev/ttyUSB1 with your secondary UART)
screen /dev/ttyUSB1 115200

# Inside Linux (Partition A) - attempt to access Partition B IPA
devmem2 0xA0000000    # should trigger Stage-2 fault and be blocked

# On host, verify the hypervisor fault log appears on the debug UART:
#   [Haven] Stage-2 fault: Partition A accessed PA outside its region
#   [Haven] HPFAR=0x0000000200000000 ESR=0x93000006
```

```bash
# DMA isolation: trigger a bounce-buffer write outside assigned region
# (requires test harness module - see tests/integration/test_smmu_dma)
insmod /lib/modules/haven_smmu_test.ko
dmesg | grep haven_smmu    # expect: "DMA fault injected, SMMU blocked access"
```

---

## Step 6 - Temporal isolation tests

```bash
# Inside Partition A (Linux) - start CPU stress
stress-ng --cpu 6 --timeout 30s &

# On host - capture RTOS latency log (Partition B prints to UART ch2)
screen /dev/ttyUSB2 115200
# Expected lines:
#   [FreeRTOS] Period deadline met: 10 ms (actual: 10.031 ms)
#   ...
# Collect 500+ lines for statistical baseline
```

```bash
# Or use the evidence capture script:
scripts/evidence/capture_imx95.sh \
  --uart-rtos /dev/ttyUSB2 \
  --duration 60 \
  --output build/evidence/imx95-temporal.json
```

---

## Step 7 - Capture evidence artifacts

```bash
# Generate the full evidence bundle for this run
scripts/evidence/gen_summary.sh \
  --platform imx95-devkit \
  --config configs/arm64/imx95-devkit.yaml \
  --boot-log boot-$(date +%Y%m%d).txt \
  --test-log test-results-$(date +%Y%m%d).txt \
  --output build/evidence/imx95/

# Verify traceability report was updated
cat build/evidence/traceability-report.json | \
  python3 -c "import json,sys; r=json.load(sys.stdin); \
  [print(c['chapter'],c['status']) for c in r['chapters']]"
```

---

## Exit criteria

| Check                       | Pass condition                                                                   |
| --------------------------- | -------------------------------------------------------------------------------- |
| Spatial isolation           | All cross-partition memory accesses produce Stage-2 fault; none silently succeed |
| DMA isolation               | `test_smmu_dma` reports all unauthorized DMA aborted by SMMUv3                   |
| RTOS latency (no stress)    | ≤ 61 µs max response time (see `docs/methodology/BENCHMARK_BASELINE.md`)         |
| RTOS latency (under stress) | No deadline miss over 30-second window                                           |
| Boot stability              | Clean boot with no panics over 5 consecutive power cycles                        |
| Evidence linked             | `traceability-report.json` shows Chapter 6 status = "evidence_collected"         |

---

## Troubleshooting

| Symptom                                  | Likely cause               | Fix                                                     |
| ---------------------------------------- | -------------------------- | ------------------------------------------------------- |
| No UART output after flash               | Wrong `imx-boot` variant   | Use `imx-boot-imx95evk-sd.bin-flash_singleboot`         |
| Stage-2 fault not triggered by `devmem2` | SMMU not enabled           | Check `smmu_v3_init()` return in boot log               |
| FreeRTOS UART silent                     | Wrong `ttyUSBx` port       | Try each `/dev/ttyUSB[0-3]`                             |
| UUU times out                            | Board not in download mode | Hold SW1 before applying power                          |
| Latency spikes > 200 µs                  | EL2 timer misconfigured    | Verify `CNTHP_CTL_EL2.ENABLE=1` in `arch/arm64/timer.c` |
