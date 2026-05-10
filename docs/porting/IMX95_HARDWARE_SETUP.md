# i.MX95 Hardware Setup

Primary target board:
- NXP i.MX95 Dev Kit

Purpose:
- Provide the board-specific setup needed to execute the validation runbook in [docs/porting/IMX95_VALIDATION_RUNBOOK.md](/Users/trongphucnguyen/Documents/haven/docs/porting/IMX95_VALIDATION_RUNBOOK.md).
- Establish one checked-in candidate configuration for initial Linux plus FreeRTOS bring-up.

## Board baseline

Reference configuration:
- [configs/arm64/imx95-devkit.yaml](/Users/trongphucnguyen/Documents/haven/configs/arm64/imx95-devkit.yaml)

Host assumptions:
- macOS or Linux host with serial terminal access.
- USB-C or UART debug adapter available for console capture.
- Known-good power supply and board jumper settings confirmed.

Software assumptions:
- Haven tree is current and local tests are green.
- Hypervisor image, Linux guest image, and FreeRTOS guest image are built from the same revision.
- Boot firmware variables and reserved memory ranges are documented before the first boot attempt.

## Pre-flight checks

1. Validate the checked-in config with `./scripts/check-configs.sh`.
2. Record the Git commit for the hypervisor, Linux guest payload, and FreeRTOS payload.
3. Confirm the boot chain in use: ROM, SPL, U-Boot, and any board firmware packaging.
4. Confirm the board serial device path on the host and verify console output at the expected baud rate.
5. Confirm the candidate memory map does not overlap boot firmware, secure world reservations, or device MMIO windows.

## Serial console and logging

Recommended capture requirements:
- One persistent UART log per bring-up session.
- Timestamped filenames under a local evidence directory.
- Separate logs for boot, Linux health checks, RTOS task loop, and isolation fault attempts where practical.

Example host commands:

```sh
mkdir -p build/evidence/imx95
script -q build/evidence/imx95/boot-console.log screen /dev/tty.usbserial-IMX95 115200
```

If `screen` is unavailable, use any equivalent serial logger that preserves raw console output.

## Initial bring-up sequence

1. Run `./scripts/test.sh` locally before touching hardware.
2. Validate [configs/arm64/imx95-devkit.yaml](/Users/trongphucnguyen/Documents/haven/configs/arm64/imx95-devkit.yaml) and compare every reserved range with the board manual.
3. Load the hypervisor image and confirm early boot logs appear on UART.
4. Start the Linux partition and verify a minimal health signal:
   - boot completes without panic
   - console login or init banner appears
   - timer and UART remain responsive
5. Start the FreeRTOS partition and verify a deterministic task loop:
   - periodic heartbeat output
   - stable tick cadence
   - no unexpected resets or watchdog faults

## Isolation validation checklist

Spatial isolation:
- Attempt one controlled cross-partition memory access and confirm it is blocked.
- Attempt one non-owner interrupt or DMA path where the harness allows it and confirm the denial is logged.
- Capture the exact console messages and software revision metadata alongside the test result.

Temporal isolation:
- Apply sustained CPU load inside Linux.
- Measure the FreeRTOS heartbeat or task-loop jitter during that window.
- Record the observation window, collection method, and worst-case latency seen.

## Evidence to archive

Store the following under `build/evidence/imx95/` or an equivalent retained path:
- Boot console log
- Linux partition launch log
- FreeRTOS partition launch log
- Isolation pass or fail notes
- Timing measurements and methodology
- Board firmware version, bootloader version, and Git commit IDs

## Open items before formal campaign

1. Replace candidate memory ranges in [configs/arm64/imx95-devkit.yaml](/Users/trongphucnguyen/Documents/haven/configs/arm64/imx95-devkit.yaml) with board-manual-confirmed values.
2. Add exact serial adapter identifiers once the lab setup is fixed.
3. Define the acceptance threshold for RTOS latency during Linux stress.
4. Link collected evidence into the thesis traceability material after the first successful board run.