#!/usr/bin/env python3
"""G2: real DMA from partition A through the SMMU in QEMU, seen from inside
(partition A's UART report) and from outside (read-back v3).

Usage: run_g2.py <haven-repo> <out-dir> [check ...]
Each check exports HEAD into a scratch copy, applies its change (each edit
must match exactly once), builds the qemu-direct image and runs read-back on
cortex-a72 and max, reading memory after partition A prints "DMA test done"
(or after 20 s). Writes per-check logs, results.json and results.md. The
repository is not modified.
"""
import json
import os
import re
import subprocess
import sys
import tempfile

HERE = "docs/methodology/stage2/20261102-g2-dma"
GRANT = f"{HERE}/grant-g2.json"
PART = "src/core/partition.c"
DEV_OK = "virtio-rng-pci,disable-legacy=on,iommu_platform=on"
DEV_BYPASS = "virtio-rng-pci"
WIDE = [(PART, "\t\t.iova = PART_A_DMA_IPA,\n\t\t.size = PART_A_DMA_SIZE,\n\t\t.pa = PART_A_DMA_PA,",
         "\t\t.iova = PART_A_DMA_IPA - 0x1000000UL,\n\t\t.size = 0x2000000UL,\n"
         "\t\t.pa = PART_A_DMA_PA - 0x1000000UL,")]
# (edits, device or None, SMMU stage, expected UART lines, expected findings, expected events)
CHECKS = {
    "G0": ([], None, "nested", ["DMA device absent"], [], False),
    "G1": ([], DEV_OK, "nested", ["DMA inside window OK", "DMA outside window blocked"], [], True),
    "G2": ([], DEV_BYPASS, "nested", ["bypasses the IOMMU"], [("pci-1af4:1005", "unenforced DMA master")], False),
    "G3": ([], DEV_OK, "1", ["withheld (rc=-95)", "no DMA device granted"],
           [("A PCI config", "missing binding"), ("A PCI BAR", "missing binding"),
            ("pci-1af4:1044", "DMA enforcement drift")], False),
    "G4": (WIDE, DEV_OK, "nested", ["DMA inside window OK", "DMA outside window NOT blocked"],
           [("SID 8", "DMA extent drift")], False),
}
CPUS = ["cortex-a72", "max"]
TOOLCHAIN = "/opt/homebrew/Cellar/aarch64-unknown-linux-gnu/15.2.0/toolchain/bin"
ENV = dict(os.environ, PATH=f"/opt/homebrew/bin:/usr/local/bin:{TOOLCHAIN}:" + os.environ.get("PATH", ""))


def sh(cmd, cwd, log, timeout=900):
    with open(log, "a") as f:
        f.write(f"$ {cmd}\n")
        f.flush()
        try:
            return subprocess.run(cmd, cwd=cwd, shell=True, stdout=f, stderr=subprocess.STDOUT,
                                  env=ENV, timeout=timeout).returncode
        except subprocess.TimeoutExpired:
            f.write("TIMEOUT\n")
            return 124


def mutate(src, edits):
    for path, old, new in edits:
        p = os.path.join(src, path)
        s = open(p).read()
        if s.count(old) != 1:
            raise SystemExit(f"edit does not match exactly once in {path} (found {s.count(old)})")
        open(p, "w").write(s.replace(old, new))


def main():
    repo, out = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    only = sys.argv[3:] or list(CHECKS)
    head = subprocess.check_output(["git", "-C", repo, "rev-parse", "--short", "HEAD"], text=True).strip()
    readback = os.path.join(repo, "tools/readback/readback.py")
    os.makedirs(out, exist_ok=True)
    results = {}
    for name in only:
        edits, dev, stage, want_uart, expect, want_events = CHECKS[name]
        cdir = os.path.join(out, name)
        os.makedirs(cdir, exist_ok=True)
        log = os.path.join(cdir, "build.log")
        open(log, "w").close()
        src = tempfile.mkdtemp(prefix=f"g2-{name}-")
        subprocess.run(f"git -C {repo} archive HEAD | tar -x -C {src}", shell=True, check=True)
        mutate(src, edits)
        with open(os.path.join(cdir, "change.diff"), "w") as f:
            for path in sorted({e[0] for e in edits}):
                subprocess.run(["diff", "-u", f"{repo}/{path}", f"{src}/{path}"], stdout=f)
        r = {"check": name, "device": dev, "smmu_stage": stage, "expect_uart": want_uart,
             "expect": expect, "expect_events": want_events}
        r["build_rc"] = sh("cmake --preset arm64-qemu -DHAVEN_BOOT_PROFILE=qemu-direct && cmake --build build", src, log)
        for cpu in CPUS:
            c = {"readback_rc": None, "findings": [], "uart": [], "events": []}
            if r["build_rc"] == 0:
                rb_out = os.path.join(cdir, f"readback-{cpu}")
                args = [sys.executable, readback, os.path.join(src, "build"), os.path.join(repo, GRANT), rb_out,
                        "--cpu", cpu, "--wait", "20", "--smmu-stage", stage, "--marker", "DMA test done"]
                if dev:
                    args += ["--device", dev]
                p = subprocess.run(args, capture_output=True, text=True, env=ENV, timeout=240)
                open(os.path.join(cdir, f"readback-{cpu}.txt"), "w").write(p.stdout + p.stderr)
                c["readback_rc"] = p.returncode
                try:
                    rep = json.load(open(os.path.join(rb_out, "readback.json")))
                    c["findings"] = [f[:2] for f in rep["findings"]]
                    c["events"] = rep.get("smmu_events", [])
                except (OSError, ValueError, KeyError):
                    pass
                uart = os.path.join(rb_out, "uart.log")
                if os.path.exists(uart):
                    c["uart"] = re.findall(r"(?:PARTITION_A: [^\r\n]*DMA[^\r\n]*|HAVEN: DMA grant[^\r\n]*)",
                                           open(uart, errors="replace").read())
            r[cpu] = c
        results[name] = r
        print(name, json.dumps({k: v for k, v in r.items() if k not in CPUS}),
              json.dumps({cpu: {k: r[cpu][k] for k in ("readback_rc", "findings", "uart")} for cpu in CPUS}),
              flush=True)
        subprocess.run(["rm", "-rf", src])
    json.dump({"haven_commit": head, "results": results}, open(os.path.join(out, "results.json"), "w"), indent=1)

    rows = [f"# G2 DMA checks (haven {head})", "",
            "| Check | CPU | SMMU | Device | Partition A reports | UART as expected | Read-back findings as expected "
            "| Extra findings | SMMU events (SID 8, outside window / inside window) |",
            "| - | - | - | - | - | - | - | - | - |"]
    for name, r in results.items():
        exp = [tuple(e) for e in r["expect"]]
        for cpu in CPUS:
            c = r[cpu]
            got = [tuple(x) for x in c["findings"]]
            uart_ok = all(any(w in u for u in c["uart"]) for w in r["expect_uart"])
            if c["readback_rc"] in (2, None):
                fok = "no verdict"
            elif not exp:
                fok = "yes" if c["readback_rc"] == 0 else "no"
            else:
                fok = "yes" if all(e in got for e in exp) else "no"
            extra = sorted({f"{n}: {k}" for n, k in got if (n, k) not in exp})
            out_ev = sum(1 for e in c["events"] if e["sid"] == 8 and not 0x5F000000 <= e["addr"] < 0x5F100000)
            in_ev = sum(1 for e in c["events"] if e["sid"] == 8 and 0x5F000000 <= e["addr"] < 0x5F100000)
            said = "; ".join(u.replace("PARTITION_A: ", "") for u in c["uart"] if u.startswith("PARTITION_A"))
            rows.append(f"| {name} | {cpu} | {r['smmu_stage']} | {r['device'] or '-'} | {said or '-'} | "
                        f"{'yes' if uart_ok else 'no'} | {fok} | {'; '.join(extra) or '-'} | {out_ev} / {in_ev} |")
    open(os.path.join(out, "results.md"), "w").write("\n".join(rows) + "\n")
    print("\n".join(rows))


if __name__ == "__main__":
    main()
