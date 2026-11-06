#!/usr/bin/env python3
"""Run the Linux chain case set (CASES.md): L4 with one change per case.

Usage: run_cases.py HAVEN_REPO KDAL_SRC IMAGE KDAL_KO OUT [--only NAME ...]
                    [--held-out DIR]

For each case: build HAVEN's Linux profile (once per source variant, from
`git archive HEAD` plus the case's patch), write the device tree and grant
for the case, boot Linux with KDAL in partition A (QEMU, until HAVEN
stops the partition or 45 s), check KDAL's demand against the grant
(demand_check.py), then run read-back on the same grant (readback.py,
read at KDAL-DEMAND-END or after 60 s). Writes OUT/<case>/ and
OUT/results.md. Observations only: scoring against the predictions is
done by hand in RESULTS.md.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
L4 = os.path.join(HERE, "..", "20261104-linux-partition")
DEV = "virtio-rng-pci,disable-legacy=on,iommu_platform=on"
PA_OFFSET = 0x40800000  # partition A: PA = IPA + 0x40800000
DTB_PA = 0x90800000  # IPA 0x50000000

# name: (kind, change, parameters); parameters are read by dts() and grant()
CASES = {
    "B0": ("baseline", "L4 unchanged (reference, not scored)", {}),
    "T1": ("T", "KDAL's node over the granted UART", {"kdal": ("0x09000000", "0x1000", "0 1 4")}),
    "T2": ("T", "KDAL's node over the UART, 2 pages", {"kdal": ("0x09000000", "0x2000", "0 1 4")}),
    "T3": ("T", "KDAL's node: GPIO registers, UART interrupt", {"kdal": ("0x09030000", "0x1000", "0 1 4")}),
    "T4": ("T", "memory node 1 GiB", {"memory": "0x40000000"}),
    "T5": ("T", "virtual timer listed as PPI 12", {"timer": "<1 13 4>, <1 14 4>, <1 12 4>, <1 10 4>"}),
    "G1": ("G", "grant also gives A the GPIO page and SPI 39", {"grant": "add-gpio"}),
    "G2": ("G", "grant omits PPI 27 on CPU 0", {"grant": "drop-ppi27"}),
    "H1": ("H", "GIC model enables any interrupt", {"patch": "patches/H1.patch"}),
    "H2": ("H", "GPIO page mapped into A's stage-2", {"patch": "patches/H2.patch"}),
    "H3": ("H", "GIC model leaves owned interrupts enabled at start", {"patch": "patches/H3.patch"}),
    "C1": ("control", "device-tree nodes reordered", {"kdal_first": True}),
    "C2": ("control", "initramfs at IPA 0x4c000000", {"initrd": 0x4C000000}),
}


def sh(cmd, log, cwd=None):
    with open(log, "a") as f:
        return subprocess.run(cmd, shell=True, cwd=cwd, stdout=f, stderr=subprocess.STDOUT).returncode


def build(repo, patch, out, log):
    """HAVEN's Linux profile from HEAD plus an optional patch."""
    src = tempfile.mkdtemp(prefix="chain-src-")
    subprocess.run(f"git -C {repo} archive HEAD | tar -x -C {src}", shell=True, check=True)
    if patch and sh(f"git apply {os.path.join(HERE, patch)}", log, cwd=src):
        raise RuntimeError(f"patch {patch} does not apply")
    if sh(f"cmake --preset arm64-qemu -B {out} -DHAVEN_BOOT_PROFILE=qemu-direct "
          f"-DHAVEN_PART_A_DTB_IPA=0x50000000 && cmake --build {out}", log, cwd=src):
        raise RuntimeError(f"build failed ({patch or 'baseline'})")
    shutil.rmtree(src)
    return out


def dts(p, initrd_ipa, initrd_end):
    s = open(os.path.join(L4, "partition-a.dts")).read()
    s = s.replace('stdout-path = "/pl011@9000000";',
                  'stdout-path = "/pl011@9000000";\n\t\tlinux,initrd-start = <0x0 %#x>;\n'
                  '\t\tlinux,initrd-end = <0x0 %#x>;' % (initrd_ipa, initrd_end), 1)
    s = s.replace('panic=-1"', 'panic=-1 rdinit=/init"', 1)
    if "memory" in p:
        s = s.replace("reg = <0x0 0x40000000 0x0 0x20000000>;", f"reg = <0x0 0x40000000 0x0 {p['memory']}>;", 1)
    if "timer" in p:
        s = s.replace("interrupts = <1 13 4>, <1 14 4>, <1 11 4>, <1 10 4>;", f"interrupts = {p['timer']};", 1)
    base, size, irq = p.get("kdal", ("0x09030000", "0x1000", "0 7 4"))
    label = "kdal-gpio" if int(base, 16) == 0x09030000 else "kdal-uart"
    node = (f"\t{label}@{int(base, 16):x} {{\n\t\tcompatible = \"kdal,device\";\n"
            f"\t\treg = <0x0 {base} 0x0 {size}>;\n\t\tinterrupts = <{irq}>;\n\t}};\n")
    if p.get("kdal_first"):
        return s.replace("\tchosen {", node + "\n\tchosen {", 1)
    i = s.rstrip().rfind("};")
    return s[:i].rstrip() + "\n\n" + node + "};\n"


def grant(p, out):
    g = json.load(open(os.path.join(L4, "grant-l4.json")))
    if p.get("grant") == "add-gpio":
        g["partitions"]["1"].append({"name": "A GPIO", "ipa": "0x09030000", "pa": "0x09030000",
                                     "size": "0x1000", "attrs": "DEVICE_RW"})
        g["gic"]["spis"]["39"] = {"partition": 1, "cpu": 0}
    if p.get("grant") == "drop-ppi27":
        del g["gic"]["ppis"]["0"]
    json.dump(g, open(out, "w"), indent=1)
    return out


def boot(build_dir, image, dtb, cpio, initrd_pa, uart, limit=45):
    cmd = ["qemu-system-aarch64", "-machine", "virt,virtualization=on,gic-version=3,iommu=smmuv3",
           "-cpu", "cortex-a72", "-smp", "4", "-m", "2G", "-display", "none", "-monitor", "none",
           "-nic", "none", "-device", DEV, "-serial", f"file:{uart}",
           "-device", f"loader,file={build_dir}/haven.bin,addr=0x80000000,force-raw=on,cpu-num=0",
           "-device", f"loader,file={image},addr=0x80800000,force-raw=on",
           "-device", f"loader,file={dtb},addr={DTB_PA:#x},force-raw=on",
           "-device", f"loader,file={cpio},addr={initrd_pa:#x},force-raw=on",
           "-device", f"loader,file={build_dir}/guest_b.bin,addr=0xA0800000,force-raw=on"]
    q = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    t0 = time.time()
    while time.time() - t0 < limit and q.poll() is None:
        text = open(uart, errors="replace").read() if os.path.exists(uart) else ""
        if "partition stopped" in text or "EL2 FATAL" in text:
            time.sleep(0.5)
            break
        time.sleep(0.25)
    q.kill()
    q.wait()
    return round(time.time() - t0, 1)


def haven_log(text):
    pick = lambda pat: [l for l in text.splitlines() if re.search(pat, l)]
    return {
        "denied": len(pick(r"Denied stage-2")),
        "decoded": pick(r"HAVEN: decoded")[:5],
        "trapped": pick(r"trapped system register")[:5],
        "not_delivered": pick(r"not delivered")[:5],
        "stopped": pick(r"partition stopped|EL2 FATAL")[:2],
        "delivered": pick(r"virtual interrupts delivered")[:1],
        "first_denied": pick(r"Denied stage-2")[:3],
    }


def run_case(name, a, builds, cpio, out_root):
    kind, change, p = CASES[name] if name in CASES else ("held-out", "drawn at random", {"patch": name})
    out = os.path.join(out_root, name)
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    log = os.path.join(out, "build.log")
    key = p.get("patch", "")
    if key not in builds:
        builds[key] = build(a.repo, key, os.path.join(a.work, f"build-{re.sub(r'[^A-Za-z0-9]', '_', key) or 'base'}"), log)
    b = builds[key]
    initrd_ipa = p.get("initrd", 0x48000000)
    size = os.path.getsize(cpio)
    open(os.path.join(out, "partition-a.dts"), "w").write(dts(p, initrd_ipa, initrd_ipa + size))
    dtb = os.path.join(out, "partition-a.dtb")
    subprocess.run(["dtc", "-q", "-I", "dts", "-O", "dtb", "-o", dtb, os.path.join(out, "partition-a.dts")], check=True)
    g = grant(p, os.path.join(out, "grant.json"))
    uart = os.path.join(out, "uart.log")
    secs = boot(b, a.image, dtb, cpio, initrd_ipa + PA_OFFSET, uart)
    text = open(uart, errors="replace").read()
    m = re.search(r"KDAL-DEMAND-BEGIN\n(.*?)\nKDAL-DEMAND-END", text.replace("\r", ""), re.S)
    res = {"case": name, "kind": kind, "change": change, "boot_seconds": secs,
           "boot_completes": "KDAL-DEMAND-END" in text and "partition stopped by PSCI OFF" in text,
           "haven": haven_log(text)}
    if m:
        open(os.path.join(out, "demand.json"), "w").write(m.group(1) + "\n")
        dc = subprocess.run([sys.executable, os.path.join(a.repo, "tools/readback/demand_check.py"), g,
                             os.path.join(out, "demand.json"), "--partition", "1"], capture_output=True, text=True)
        open(os.path.join(out, "demand-check.txt"), "w").write(dc.stdout + dc.stderr)
        res["demand_check"] = {"exit": dc.returncode,
                               "findings": [l.strip() for l in dc.stdout.splitlines() if "FINDING" in l]}
    else:
        res["demand_check"] = {"exit": None, "findings": ["no demand printed (boot did not reach it)"]}
    rb = subprocess.run([sys.executable, os.path.join(a.repo, "tools/readback/readback.py"), b, g,
                         os.path.join(out, "readback"), "--cpu", "cortex-a72", "--device", DEV, "--wait", "60",
                         "--marker", "KDAL-DEMAND-END", "--guest-a", a.image, "--load", f"{dtb}@{DTB_PA:#x}",
                         "--load", f"{cpio}@{initrd_ipa + PA_OFFSET:#x}"], capture_output=True, text=True)
    open(os.path.join(out, "readback.txt"), "w").write(rb.stdout + rb.stderr)
    res["readback"] = {"exit": rb.returncode,
                       "read_at": next((l for l in rb.stdout.splitlines() if l.startswith("read at")), ""),
                       "result": next((l for l in rb.stdout.splitlines() if l.startswith("RESULT")), "no RESULT"),
                       "findings": [l.strip() for l in rb.stdout.splitlines() if "FINDING" in l]}
    rbdir = os.path.join(out, "readback")
    if os.path.isdir(rbdir):  # keep the report, not the memory dumps
        if os.path.exists(os.path.join(rbdir, "readback.json")):
            shutil.copy(os.path.join(rbdir, "readback.json"), os.path.join(out, "readback.json"))
        shutil.rmtree(rbdir)
    os.remove(dtb)
    json.dump(res, open(os.path.join(out, "summary.json"), "w"), indent=1)
    return res


def main():
    ap = argparse.ArgumentParser()
    for x in ("repo", "kdal_src", "image", "kdal_ko", "out"):
        ap.add_argument(x)
    ap.add_argument("--only", nargs="*")
    ap.add_argument("--held-out", help="directory with R*.patch (random draw)")
    a = ap.parse_args()
    a.repo, a.out = os.path.abspath(a.repo), os.path.abspath(a.out)
    os.makedirs(a.out, exist_ok=True)
    a.work = tempfile.mkdtemp(prefix="chain-work-")
    init = os.path.join(a.work, "init")
    subprocess.run(["clang", "--target=aarch64-linux-gnu", "-ffreestanding", "-nostdlib", "-static", "-O2",
                    "-fno-stack-protector", "-fuse-ld=lld", "-Wl,-e,_start", "-o", init,
                    os.path.join(a.kdal_src, "scripts/ci/kunit-init.c")], check=True)
    cpio = os.path.join(a.work, "initramfs.cpio")
    subprocess.run([sys.executable, os.path.join(a.kdal_src, "scripts/ci/mkcpio.py"), cpio, init, a.kdal_ko], check=True)
    names = a.only or list(CASES)
    held = sorted(f for f in os.listdir(a.held_out) if f.endswith(".patch")) if a.held_out else []
    for f in held:
        CASES[f[:-6]] = ("held-out", "drawn at random (see draw.json)",
                         {"patch": os.path.join(os.path.abspath(a.held_out), f)})
        names.append(f[:-6])
    head = subprocess.check_output(["git", "-C", a.repo, "rev-parse", "--short", "HEAD"], text=True).strip()
    builds, results = {}, []
    for n in names:
        print(f"== {n}", flush=True)
        try:
            r = run_case(n, a, builds, cpio, a.out)
        except Exception as e:  # a case that cannot run is recorded, not hidden
            r = {"case": n, "error": str(e)}
        results.append(r)
        print(json.dumps(r)[:400], flush=True)
    rows = ["| Case | Change | Demand check | Read-back | Boot | HAVEN log |", "| - | - | - | - | - | - |"]
    for r in results:
        if "error" in r:
            rows.append(f"| {r['case']} | error: {r['error']} | | | | |")
            continue
        dc = r["demand_check"]
        rb = r["readback"]
        h = r["haven"]
        log = f"denied {h['denied']}; " + ("; ".join(h["not_delivered"][:1] + h["trapped"][:1]) or "no other events")
        rows.append(f"| {r['case']} | {r['change']} | exit {dc['exit']}: {'; '.join(dc['findings']) or 'no finding'} | "
                    f"{rb['result']}{': ' + '; '.join(rb['findings']) if rb['findings'] else ''} ({rb['read_at']}) | "
                    f"{'completes' if r['boot_completes'] else 'does not complete'} ({r['boot_seconds']} s) | {log} |")
    open(os.path.join(a.out, "results.md"), "w").write(
        f"# Linux chain case set v1: observations\n\nhaven {head}; generated by run_cases.py.\n\n" + "\n".join(rows) + "\n")
    json.dump(results, open(os.path.join(a.out, "results.json"), "w"), indent=1)
    shutil.rmtree(a.work, ignore_errors=True)
    print("\n".join(rows))


if __name__ == "__main__":
    main()
