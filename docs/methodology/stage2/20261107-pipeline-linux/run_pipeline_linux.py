#!/usr/bin/env python3
"""Interference pipeline check with Linux in partition A (README.md).

Usage: run_pipeline_linux.py HAVEN_REPO KDAL_SRC IMAGE KDAL_KO OUT [CPU]

Per cell: HAVEN's Linux profile with qemu-psci (partition B's budget
patched as in ../20261102-interference/run_pipeline.py), a /init built from
load-init.c for the cell's load, the case set's T1 device tree; boot,
read partition B's status record over QMP until its probe is done (stage
5) or 60 s pass, then the pipeline checks. Writes OUT/<cell>/{uart.log,
status.json} and OUT/results.md.
"""
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


rc = load("cases", os.path.join(HERE, "..", "20261106-linux-chain-cases", "run_cases.py"))
PART = "src/core/partition.c"
B_BUDGET = "\t.budget_ns = 2000000ULL, /*  2 ms */"
B_OFF = "\t.budget_ns = 10000000ULL, /* = period */"
CELLS = [  # name, load mode, partition B budget off?
    ("idle-Boff", 0, True),
    ("cpu-Boff", 1, True),
    ("mem-Boff", 2, True),
    ("idle-B2ms", 0, False),
]
MODES = {0: "idle", 1: "cpu", 2: "mem"}


def build(repo, b_off, out, log):
    src = tempfile.mkdtemp(prefix="pipe-src-")
    subprocess.run(f"git -C {repo} archive HEAD | tar -x -C {src}", shell=True, check=True)
    if b_off:
        p = os.path.join(src, PART)
        s = open(p).read()
        assert B_BUDGET in s
        open(p, "w").write(s.replace(B_BUDGET, B_OFF))
    with open(log, "w") as fh:
        subprocess.run(f"cmake --preset arm64-qemu -B {out} -DHAVEN_BOOT_PROFILE=qemu-psci "
                       f"-DHAVEN_PART_A_DTB_IPA=0x50000000 && cmake --build {out}",
                       shell=True, cwd=src, stdout=fh, stderr=fh, check=True)
    shutil.rmtree(src)
    return out


def main():
    repo, ksrc, image, ko, out = (os.path.abspath(x) for x in sys.argv[1:6])
    cpu = sys.argv[6] if len(sys.argv) > 6 else "cortex-a72"
    pb = load("partb", os.path.join(repo, "scripts/qemu/partition-b-check.py"))
    nm = shutil.which("aarch64-unknown-linux-gnu-nm") or shutil.which("aarch64-linux-gnu-nm")
    os.makedirs(out, exist_ok=True)
    work = tempfile.mkdtemp(prefix="pipe-")
    builds = {}
    rows = ["| Cell | Load | B budget | Checks | B probe: samples, avg / max (us), < 1 ms, 1 to 10 ms, >= 10 ms | Linux /init at |",
            "| - | - | - | - | - | - |"]
    for name, mode, b_off in CELLS:
        d = os.path.join(out, name)
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
        if b_off not in builds:
            builds[b_off] = build(repo, b_off, os.path.join(work, f"b{int(b_off)}"),
                                  os.path.join(out, f"build-B{'off' if b_off else '2ms'}.log"))
        b = builds[b_off]
        init, cpio = os.path.join(work, f"init{mode}"), os.path.join(work, f"init{mode}.cpio")
        subprocess.run(["clang", "--target=aarch64-linux-gnu", "-ffreestanding", "-nostdlib", "-static", "-O2",
                        "-fno-stack-protector", "-fuse-ld=lld", "-Wl,-e,_start", f"-DLOAD_MODE={mode}", "-o", init,
                        os.path.join(HERE, "load-init.c")], check=True)
        subprocess.run([sys.executable, os.path.join(ksrc, "scripts/ci/mkcpio.py"), cpio, init, ko], check=True)
        ipa = 0x48000000
        dts = os.path.join(d, "partition-a.dts")
        with open(dts, "w") as fh:
            fh.write(rc.dts(rc.CASES["T1"][2], ipa, ipa + os.path.getsize(cpio)))
        dtb = os.path.join(work, f"{name}.dtb")
        subprocess.run(["dtc", "-q", "-I", "dts", "-O", "dtb", "-o", dtb, dts], check=True)
        status_pa = pb.symbol(nm, os.path.join(b, "guest_b.elf"), "guest_b_status") - pb.PART_B_IPA_BASE \
            + pb.PART_B_PA_BASE
        uart = os.path.join(d, "uart.log")
        sock = f"/tmp/pipe-{os.getpid()}.sock"
        q = subprocess.Popen([
            "qemu-system-aarch64", "-machine", "virt,virtualization=on,gic-version=3,iommu=smmuv3",
            "-cpu", cpu, "-smp", "4", "-m", "2G", "-display", "none", "-monitor", "none", "-nic", "none",
            "-device", rc.DEV, "-serial", f"file:{uart}", "-qmp", f"unix:{sock},server,nowait",
            "-device", f"loader,file={b}/haven.bin,addr=0x80000000,force-raw=on,cpu-num=0",
            "-device", f"loader,file={image},addr=0x80800000,force-raw=on",
            "-device", f"loader,file={dtb},addr={rc.DTB_PA:#x},force-raw=on",
            "-device", f"loader,file={cpio},addr={ipa + rc.PA_OFFSET:#x},force-raw=on",
            "-device", f"loader,file={b}/guest_b.bin,addr={pb.PART_B_PA_BASE:#x},force-raw=on"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        status = dict.fromkeys(pb.FIELDS, 0)
        try:
            t0 = time.time()
            while not os.path.exists(sock) and time.time() - t0 < 10:
                time.sleep(0.05)
            m = pb.QMP(sock)
            while time.time() - t0 < 60 and q.poll() is None:
                status = dict(zip(pb.FIELDS, (m.read32(status_pa + 4 * i) for i in range(len(pb.FIELDS)))))
                if status["stage"] == 5:
                    break
                time.sleep(0.25)
            time.sleep(0.5)
        finally:
            q.kill()
            q.wait()
            if os.path.exists(sock):
                os.remove(sock)
        with open(uart, errors="replace") as fh:
            log = fh.read()
        with open(os.path.join(d, "status.json"), "w") as fh:
            json.dump(status, fh, indent=1)
        init_t = re.search(r"\[\s*([0-9.]+)\] Run /init as init process", log)
        checks = {
            "B started": "CPU 2 ready, partition B started" in log,
            "load running": f"LOAD-INIT: mode {MODES[mode]} running" in log,
            "/init before B's probe (4 s)": bool(init_t) and float(init_t.group(1)) < 4.0,
            "partition A not stopped": "partition stopped" not in log,
            "B magic": status["magic"] == pb.GUEST_B_MAGIC,
            "B stage 5": status["stage"] == 5,
            "B 500 samples": status["lat_samples"] == 500,
            "B result pass": status["result"] == 1,
            "B caches on (M, C)": status["sctlr_el1"] & 0x5 == 0x5,
            "B's read of A denied on CPU 2": any("IPA=0x40000000" in l and "cpu=2" in l
                                                 for l in log.splitlines() if "Denied" in l),
            "no undelivered interrupt": "not delivered" not in log,
        }
        miss = [k for k, v in checks.items() if not v]
        rows.append(f"| {name} | {MODES[mode]} | {'off' if b_off else '2 ms / 10 ms'} | "
                    f"{'all ' + str(len(checks)) + ' ok' if not miss else 'MISSING: ' + '; '.join(miss)} | "
                    f"{status['lat_samples']}, {status['lat_avg_ns'] // 1000} / {status['lat_max_ns'] // 1000}, "
                    f"{status['lat_lt10us'] + status['lat_lt100us'] + status['lat_lt1ms']}, {status['lat_lt10ms']}, "
                    f"{status['lat_ge10ms']} | {init_t.group(1) + ' s' if init_t else '-'} |")
        print(rows[-1], flush=True)
    with open(os.path.join(out, "results.md"), "w") as fh:
        fh.write(f"# Pipeline with Linux in partition A ({cpu})\n\n" + "\n".join(rows) + "\n")
    shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
