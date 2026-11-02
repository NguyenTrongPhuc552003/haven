# Physical Board Validation Matrix

> **Predates the current research question (2026-11-02).** Written before the A1
> read-back direction (2026-11-01) and not yet revised: statements here may not match
> the code. Current state: [docs/README.md](../README.md).

This document defines physical validation strategy for i.MX95 and secondary boards.

## Primary board

1. NXP i.MX95 Dev Kit.

## Secondary heterogeneous boards (target set)

1. NXP i.MX8 family board.
2. One additional heterogeneous SoC board selected by availability and support maturity.

## Validation dimensions

1. Bring-up stability.
2. Spatial isolation enforcement.
3. Temporal isolation under interference.
4. Device ownership and interrupt routing behavior.

## Board-level verification checklist

1. Boot sequence integrity and log capture.
2. Memory map correctness and reserved region enforcement.
3. Interrupt ownership setup and denial behavior.
4. Linux stress and RTOS latency campaign.
5. Evidence package generation with commit-bound metadata.

## Release mapping

Release 2:
1. Initial i.MX95 bring-up and smoke evidence.

Release 3:
1. Full i.MX95 isolation campaign.
2. Secondary board baseline validation.

Release 4:
1. Revalidation on all boards.
2. Final thesis evidence freeze.
