# Try HAVEN without hardware

This bundle runs HAVEN, a small static-partitioning hypervisor for ARM64, in
QEMU's `virt` machine: no board, no cross compiler, no build.

```sh
./try.sh
```

It needs `qemu-system-aarch64` (9.2 or later for the DMA part), `python3`
and `dtc`:

| Host | Install |
| - | - |
| macOS | `brew install qemu dtc` |
| Debian, Ubuntu | `sudo apt install qemu-system-arm python3 device-tree-compiler` |
| Windows | WSL2 with Ubuntu, then the line above |

## What you see

1. **Two partitions on two CPUs.** HAVEN starts at EL2, gives partition A
   CPU 0 and partition B CPU 2, each with its own memory, interrupts and a
   CPU time budget enforced by HAVEN's timer. Partition A prints on the
   serial port; partition B has no serial port and reports through a
   record in its own memory, read over QEMU's monitor. B's attempt to read
   A's memory is denied and logged.
2. **Read-back.** A separate tool stops the machine and reads what the
   hardware actually enforces: HAVEN's stage-2 page tables, the SMMU's
   stream tables, the interrupt controller's routing and priorities, and
   HAVEN's own memory map. It compares them with the grant (`grant.json`,
   what HAVEN is supposed to allow). Result: no difference.
3. **A seeded fault.** The second image contains one wrong line: HAVEN
   also maps 2 MiB of partition B's memory into its own address space.
   Both partitions keep working, so a functional test passes; read-back
   reports "partition memory mapped at EL2".

That last step is the research question of the HAVEN thesis: can reading
back the enforced hardware state find enforcement drift that tests and
configuration checks miss?

## Contents

| Path | What it is |
| - | - |
| `images/good/` | HAVEN and its two demo partitions (`haven.bin`, `guest_a.bin`, `guest_b.bin`), symbol lists (`*.nm`) |
| `images/fault-el2-maps-b/` | the same with the seeded fault |
| `grant.json` | the grant both images are checked against |
| `tools/` | `readback.py` and `partition-b-check.py` from the HAVEN repository |
| `BUILD-INFO.txt` | the HAVEN commit and toolchain these images were built from |

## Build it yourself

Source, build instructions and every experiment record:
<https://github.com/NguyenTrongPhuc552003/haven>. On macOS,
[ELMOS](https://github.com/NguyenTrongPhuc552003/elmos) installs the
toolchain and runs the same checks (`task haven:all`).
