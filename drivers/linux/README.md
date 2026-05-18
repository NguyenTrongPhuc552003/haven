# Haven Linux Guest Kernel Module

## Purpose

`haven_driver.ko` is a loadable Linux kernel module that exposes a `/dev/haven`
character device to guest userspace.  Guest applications use this device to
submit requests to the Haven hypervisor via hypercalls, and to receive
notifications (e.g. budget exhaustion signals).

This module is intended to run inside a Linux EL1 guest partition.  It does
**not** run in the hypervisor itself.

---

## Hypercall ABI

Haven uses `HVC #0` as its hypercall instruction.  The calling convention
follows the ARM64 AAPCS with the following register assignments:

| Register | Role                        | Notes                             |
| -------- | --------------------------- | --------------------------------- |
| `x0`     | Function ID (input)         | See function ID table below       |
| `x1`     | Argument 1 / Return value 0 | Return value overwrites on return |
| `x2`     | Argument 2                  |                                   |
| `x3`     | Argument 3                  |                                   |
| `x4–x7`  | Arguments 4–7               | Preserved if not used as return   |

### Return Convention

On return from `HVC #0`:
- `x0` contains the status code: `0` = success, negative values are
  `-ERRNO`-style error codes.
- `x1` optionally contains a result value (function-specific).

### Function IDs

| ID  | Name                     | Args                         | Description                            |
| --- | ------------------------ | ---------------------------- | -------------------------------------- |
| `0` | `HV_FUNC_VERSION`        | -                            | Returns hypervisor ABI version in `x1` |
| `1` | `HV_FUNC_YIELD`          | -                            | Voluntarily yield CPU to scheduler     |
| `2` | `HV_FUNC_IRQ_CLAIM`      | `x1` = irq number            | Claim ownership of a virtual IRQ       |
| `3` | `HV_FUNC_IRQ_RELEASE`    | `x1` = irq number            | Release previously claimed IRQ         |
| `4` | `HV_FUNC_BUDGET_QUERY`   | -                            | Return remaining budget ticks in `x1`  |
| `5` | `HV_FUNC_PARTITION_INFO` | `x1` = partition id          | Return partition descriptor address    |
| `6` | `HV_FUNC_CONSOLE_WRITE`  | `x1` = buf PA, `x2` = length | Debug console write (development only) |

---

## IOCTL Reference

The `/dev/haven` character device exposes the following `ioctl` commands,
defined in `haven_ioctl.h`:

| IOCTL constant             | Direction | Payload struct             | Action                             |
| -------------------------- | --------- | -------------------------- | ---------------------------------- |
| `HAVEN_IOC_VERSION`        | `_IOR`    | `struct haven_version`     | Read hypervisor ABI version        |
| `HAVEN_IOC_YIELD`          | `_IO`     | -                          | Issue `HV_FUNC_YIELD` hypercall    |
| `HAVEN_IOC_IRQ_CLAIM`      | `_IOW`    | `struct haven_irq_claim`   | Claim a virtual IRQ number         |
| `HAVEN_IOC_IRQ_RELEASE`    | `_IOW`    | `struct haven_irq_release` | Release a previously claimed IRQ   |
| `HAVEN_IOC_BUDGET_QUERY`   | `_IOR`    | `struct haven_budget`      | Query remaining CPU budget (ticks) |
| `HAVEN_IOC_PARTITION_INFO` | `_IOWR`   | `struct haven_part_info`   | Query partition descriptor         |

---

## Build

The module uses the standard out-of-tree kernel module build system.

**Prerequisites:**
- Linux kernel headers for the target kernel (installed in
  `/lib/modules/$(uname -r)/build` on most distributions, or provide the
  kernel source tree path).
- `aarch64-unknown-linux-gnu-gcc` cross-compiler when building on an x86_64 host.

**Build command:**

```bash
make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
```

For cross-compilation from an x86_64 host targeting AArch64:

```bash
make -C /path/to/linux-src M=$(pwd) \
    ARCH=arm64 CROSS_COMPILE=aarch64-unknown-linux-gnu- modules
```

The build produces `haven_driver.ko` in the current directory.

---

## Install

Copy to the guest rootfs and load:

```bash
insmod haven_driver.ko
```

Or use `modprobe` after installing to the module tree:

```bash
cp haven_driver.ko /lib/modules/$(uname -r)/extra/
depmod -a
modprobe haven_driver
```

The character device `/dev/haven` is created automatically via `misc_register`.
Verify with:

```bash
ls -l /dev/haven
# crw------- 1 root root 10, <minor> ...
```

---

## Notes

- The kernel module requires Linux 5.10 or later (misc device API stability).
- On debug builds, `HAVEN_IOC_CONSOLE_WRITE` is enabled; it is compiled out
  (`CONFIG_HAVEN_DEBUG=n`) for production images.
- Source: `drivers/linux/haven_driver.c`
- IOCTL definitions: `drivers/guest-tools/haven_ioctl.h`
