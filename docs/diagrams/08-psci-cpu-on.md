# How are CPUs started, for HAVEN and for a guest?

Two kinds of CPU_ON: HAVEN starting partition B's CPU at boot, and a Linux
guest in partition A starting its own vCPUs, which HAVEN answers itself.
Sources: `CMakeLists.txt` (boot profiles), `src/core/partition.c`
(`partitions_launch`, `hv_partition_start_vcpu`),
`src/core/exc/el2_exceptions.c` (`hv_handle_psci_call`),
`src/core/exc/psci.c`.

```mermaid
sequenceDiagram
  participant LX as Linux in partition A<br/>(vCPU 0)
  participant HV as HAVEN at EL2
  participant FW as PSCI provider<br/>QEMU (qemu-psci) or TF-A BL31 (qemu-tfa)
  participant C as Target physical CPU
  participant G as Partition guest code (EL1)
  rect rgba(128,128,128,0.08)
    Note over HV,G: At boot: partition B on CPU 2
    alt qemu-direct (no PSCI provider)
      HV->>HV: CPU_ON unavailable, no SMC or HVC issued
      HV->>HV: log "partition B not started"
    else qemu-psci or qemu-tfa
      HV->>FW: SMC CPU_ON (CPU 2, secondary_entry)
      FW->>C: power on at secondary_entry
      FW-->>HV: SUCCESS
      HV->>C: mailbox and SEV: partition B's CPU init
      C->>G: ERET into partition B
      HV->>HV: wait until CPU 2 is ready, log "partition B started"
    end
  end
  rect rgba(128,128,128,0.08)
    Note over LX,G: At run time: Linux starts vCPU 1 (and 2)
    LX->>HV: PSCI CPU_ON (target MPIDR, entry, context)<br/>by HVC, or SMC trapped by HCR_EL2.TSC
    HV->>HV: target among partition A's vCPUs?<br/>no: INVALID_PARAMETERS, already on: ALREADY_ON
    HV->>FW: SMC CPU_ON for the vCPU's physical CPU
    FW->>C: power on
    HV->>C: start the vCPU at the guest's entry
    C->>G: ERET into Linux's secondary entry
    HV-->>LX: SUCCESS (vCPU state ON)
  end
```

- Both paths are observed: partition B starts in every partition B check
  (qemu-psci) and under TF-A; Linux brings up 2 and 3 vCPUs and takes one
  down and up again (CPU hotplug) in the CI job "Linux in partition A".
- A guest's PSCI calls never reach the firmware directly: HAVEN answers
  PSCI_VERSION, PSCI_FEATURES, MIGRATE_INFO_TYPE, AFFINITY_INFO,
  CPU_SUSPEND, CPU_ON, CPU_OFF, SYSTEM_OFF and SYSTEM_RESET itself, and
  returns NOT_SUPPORTED for the rest (`src/core/exc/psci.c`).
