#!/usr/bin/env python3
"""Boot a qemu-psci HAVEN build through haven-boot.efi under UEFI at EL2
(L1b, docs/methodology/stage2/20261109-uefi-loading/README.md) and check it.

Usage: scripts/qemu/uefi-boot-check.py BUILD EFIDIR CPU [seconds] [--seed mmu|e2h]
  BUILD    qemu-psci build directory (haven.bin, haven.elf, guest_a.bin,
           guest_b.bin, guest_b.elf)
  EFIDIR   output of boot/efi/build.sh (haven-boot*.efi)
  CPU      QEMU CPU model (cortex-a72 or max)
  seconds  time limit (default: 120)
  --seed   run a test-only loader build instead and check that HAVEN's entry
           check refuses: CPU 0 parks in _entry_mmu or _entry_e2h

The ESP is a directory given to QEMU as a FAT disk on virtio-blk PCI slot 5,
so function 00:01.0 (partition A's DMA device in memory.h) stays empty as in
the other checks. Firmware: $UEFI_FW, else the Ubuntu or Homebrew path.
Writes BUILD/evidence/uefi-boot-<cpu>[-seed].log; exits 0 only if every
check holds. Needs qemu-system-aarch64 and an aarch64 nm.
"""

import hashlib
import importlib.util
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("pbc", os.path.join(HERE, "partition-b-check.py"))
pbc = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(pbc)

FIRMWARE = ("/usr/share/qemu-efi-aarch64/QEMU_EFI.fd", "/usr/share/AAVMF/AAVMF_CODE.fd",
            "/opt/homebrew/share/qemu/edk2-aarch64-code.fd", "/usr/local/share/qemu/edk2-aarch64-code.fd")
# src/platform/qemu-virt/memory.h, as listed in the L1b record (row U1)
RANGES = (("haven", 0x80000000, 0x800000), ("part-a", 0x80800000, 0x20000000),
          ("part-b", 0xA0800000, 0x4000000), ("shared", 0xA4800000, 0x400000),
          ("part-a-ro", 0xA4C00000, 0x200000))
FILES = (("haven.bin", 0x80000000), ("guest_a.bin", 0x80800000), ("guest_b.bin", 0xA0800000))


def make_esp(build, efidir, seed, out):
    esp = os.path.join(out, "esp-seed" if seed else "esp")
    shutil.rmtree(esp, ignore_errors=True)
    os.makedirs(esp)
    loader = f"haven-boot-seed-{seed}.efi" if seed else "haven-boot.efi"
    shutil.copy(os.path.join(efidir, loader), os.path.join(esp, "haven-boot.efi"))
    for f, _ in FILES:
        shutil.copy(os.path.join(build, f), esp)
    with open(os.path.join(esp, "startup.nsh"), "w", newline="") as f:
        f.write("fs0:\r\nhaven-boot.efi\r\nreset -s\r\n")
    return esp


def firmware(out):
    fw = os.environ.get("UEFI_FW") or next((f for f in FIRMWARE if os.path.isfile(f)), None)
    if not fw:
        raise SystemExit("uefi-boot-check: no AArch64 UEFI firmware (set UEFI_FW)")
    pad = os.path.join(out, "fw.fd")  # pflash is 64 MiB; the image is padded
    shutil.copy(fw, pad)
    os.chmod(pad, 0o644)
    with open(pad, "r+b") as f:
        f.truncate(64 << 20)
    return fw, pad


def pc_of_cpu0(m):
    out = m.cmd("human-monitor-command", **{"command-line": "info registers"})
    words = out.replace("=", " ").split()
    for i, w in enumerate(words):
        if w == "PC" and i + 1 < len(words):
            return int(words[i + 1], 16)
    return None


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--seed")]
    seed = None
    if "--seed" in sys.argv:
        seed = sys.argv[sys.argv.index("--seed") + 1]
        args.remove(seed)
    if len(args) < 3:
        print(__doc__)
        return 2
    build, efidir, cpu = args[:3]
    limit = float(args[3]) if len(args) > 3 else 120.0
    nm = shutil.which("aarch64-linux-gnu-nm") or shutil.which("aarch64-unknown-linux-gnu-nm")
    out = os.path.join(build, "evidence")
    os.makedirs(out, exist_ok=True)
    esp = make_esp(build, efidir, seed, out)
    fw, pad = firmware(out)
    log_path = os.path.join(out, f"uefi-boot-{cpu}{'-seed-' + seed if seed else ''}.log")
    if os.path.exists(log_path):
        os.remove(log_path)
    sock = f"/tmp/uefiboot-{os.getpid()}.sock"
    print(f"uefi-boot-check: firmware {fw}")
    t0 = time.time()
    q = subprocess.Popen([
        "qemu-system-aarch64",
        "-machine", "virt,virtualization=on,gic-version=3,iommu=smmuv3",
        "-cpu", cpu, "-smp", "4", "-m", "2G", "-display", "none", "-monitor", "none", "-nic", "none",
        "-serial", f"file:{log_path}", "-qmp", f"unix:{sock},server,nowait",
        "-drive", f"if=pflash,format=raw,readonly=on,file={pad}",
        "-drive", f"if=none,id=esp,format=raw,file=fat:rw:{esp}",
        "-device", "virtio-blk-pci,drive=esp,addr=5",
    ])
    read = lambda: open(log_path, errors="replace").read().replace("\r", "") if os.path.exists(log_path) else ""
    status = dict.fromkeys(pbc.FIELDS, 0)
    pc = None
    try:
        while not os.path.exists(sock) and time.time() - t0 < 10:
            time.sleep(0.05)
        m = pbc.QMP(sock)
        status_pa = None
        if not seed:
            status_pa = (pbc.symbol(nm, os.path.join(build, "guest_b.elf"), "guest_b_status")
                         - pbc.PART_B_IPA_BASE + pbc.PART_B_PA_BASE)
        jumped_at = None
        while time.time() - t0 < limit and q.poll() is None:
            log = read()
            if "haven-boot: jump" in log and jumped_at is None:
                jumped_at = time.time()
            if seed and jumped_at and time.time() - jumped_at > 2:
                pc = pc_of_cpu0(m)
                break
            if not seed and jumped_at:
                status = pbc.read_status(m, status_pa)
                if status["stage"] == 5 and "PARTITION_A: IRQ test done" in log:
                    break
            time.sleep(0.25)
        time.sleep(0.5)
    finally:
        q.kill()
        q.wait()
        if os.path.exists(sock):
            os.remove(sock)
    elapsed = time.time() - t0
    log = read()
    print(f"---- UART ({log_path}) ----")
    print(log, end="")
    print("---------------------")
    lines = log.splitlines()

    def pos(s):
        return next((i for i, l in enumerate(lines) if s in l), -1)

    checks = [("U1 loader started at EL2", "haven-boot: HAVEN-BOOT 1" in log and "haven-boot: CurrentEL 0x0000000000000002" in log)]
    for name, base, size in RANGES:
        checks.append((f"U1 claim {name}", f"haven-boot: claim {name} at {base:#018x} size {size:#018x}" in log))
    for f, base in FILES:
        data = open(os.path.join(esp, f), "rb").read()
        want = f"haven-boot: load {f} at {base:#018x} size {len(data):#018x} sha256 {hashlib.sha256(data).hexdigest()}"
        checks.append((f"U2 load {f}", want in log))
    checks.append(("U3 ExitBootServices in 1 try", "haven-boot: exit-boot-services tries 1" in log))
    checks.append(("U4 jump after the tries line", -1 < pos("exit-boot-services tries") < pos("haven-boot: jump")))
    if seed:
        sym = "_entry_mmu" if seed == "mmu" else "_entry_e2h"
        park = pbc.symbol(nm, os.path.join(build, "haven.elf"), sym)
        checks.append(("K HAVEN did not start", "Haven hypervisor starting" not in log))
        checks.append((f"K CPU 0 parked in {sym} (PC {pc:#x} vs {park:#x})" if pc is not None else f"K CPU 0 PC read for {sym}",
                       pc is not None and park <= pc < park + 8))
    else:
        checks.append(("U5 HAVEN started after the jump", -1 < pos("haven-boot: jump") < pos("Haven hypervisor starting")))
        for s in ("Hello from partition A", "Denied stage-2 violation", "PARTITION_A: Post-fault check OK",
                  "PARTITION_A: IRQ test done"):
            checks.append((f"U6 {s}", s in log))
        checks += [("U7 " + n, ok) for n, ok in pbc.b_checks(log, status)]
        checks.append((f"U8 under 120 s ({elapsed:.1f} s)", elapsed < 120))
    for name, ok in checks:
        print(("ok      " if ok else "FAILED  ") + name)
    ok = all(c for _, c in checks)
    print(f"uefi-boot-check: {'PASS' if ok else 'FAIL'} ({cpu}{', seed ' + seed if seed else ''})")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
