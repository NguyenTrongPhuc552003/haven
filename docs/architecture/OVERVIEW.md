# Architecture Overview

Haven is a static partition hypervisor architecture:

- Partition boundaries are fixed before runtime.
- CPU, memory, and interrupt ownership are explicitly configured.
- Hypervisor control logic remains minimal to reduce trusted computing base.

Design focus:
- Deterministic behavior for mixed-criticality workloads.
- Isolation-first execution model over feature-rich virtualization.
