# Read-back v12a: routing and group of disabled SPIs

**Status:** owner-approved 2026-11-08 ("Approve", item 3). Change and
expectations by Claude, committed before the change is made and run.

## Problem (`../20261108-el3-readback/RESULTS.md`, addendum)

For a granted SPI that is disabled, read-back reports "missing interrupt
binding" and stops checking it. In case I1 (SPI 33 routed to CPU 2),
HAVEN disabled the interrupt after it arrived on the wrong CPU, so
read-back reported only the missing binding; the wrong IROUTER (CPU 2,
partition B's) went unreported. An SPI waiting for the guest (v8) is
skipped the same way.

## Change

In `compare_gic`, a granted SPI's routing (1-of-N, another partition's
CPU, another CPU) and group are checked whether it is enabled, disabled
or waiting for the guest. The missing-binding and waiting rules stay as
they are; priorities of disabled SPIs are still not checked (v7, RB3).

## Expectations (Claude's, before the change)

Unit tests (`tools/readback/test_disabled_spi.py`, synthetic GIC state):

| State of granted SPI 33 | Expected findings |
| - | - |
| disabled, routed to the granted CPU | missing binding only |
| disabled, routed to another partition's CPU | missing binding and routing drift |
| waiting for the guest (model and GIC off), routed elsewhere | routing drift only |
| disabled, Group 0 (groups visible) | missing binding and group drift |
| the 24 existing read-back tests | unchanged |

Reruns of three EL3-record cases, both channels (`run_el3.py`'s builder,
grant and options):

| Case | Expected, both channels |
| - | - |
| E0 | as before: CPU 2 PPI 27 missing binding |
| I1 | as before **plus** SPI 33 routing drift (routed to CPU 2, partition 2; granted partition 1) |
| S1 | as before (SPI 33 is routed to CPU 0, the CPU the grant gives partition 1; the routing check uses the grant, not HAVEN's table) |
