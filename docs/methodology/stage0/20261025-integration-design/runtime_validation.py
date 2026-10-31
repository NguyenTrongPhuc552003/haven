#!/usr/bin/env python3
"""Bounded isolated ARM64 ABI/alignment experiment. Never edits tracked code.

Uses only pinned HAVEN and preserved TF-A/probe inputs. Local Unix GDB sockets
allow register observation without inserting diagnostic instructions/layouts.
QEMU runs are diagnostic; neither SMC nor probe initialization is a proposal.
"""
import argparse
import difflib
import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import tarfile
import time

HERE = Path(__file__).resolve().parent
PREV = HERE.parent / "20261025-boundaries"
CORR = HERE.parent / "20261025-corrections"
REV = "f775304ce9443dc9861ba1880e4208c7ae19b314"
ap = argparse.ArgumentParser()
ap.add_argument("--haven", required=True, type=Path)
ap.add_argument("--work", required=True, type=Path)
a = ap.parse_args()
repo, work = a.haven.resolve(), a.work.resolve()
work.mkdir(parents=True, exist_ok=False)
commands, results = [], {}


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def command(name, argv, cwd=work):
    start = time.monotonic()
    argv = list(map(str, argv))
    with (work / (name + ".log")).open("wb") as f:
        r = subprocess.run(argv, cwd=cwd, stdout=f, stderr=subprocess.STDOUT,
                           timeout=120, check=False)
    commands.append(dict(name=name, argv=argv, cwd=str(cwd), timeout_s=120,
                         returncode=r.returncode, elapsed_s=time.monotonic()-start))
    (work / "commands.json").write_text(json.dumps(commands, indent=2)+"\n")
    if r.returncode:
        raise RuntimeError(name)


for binary in ["qemu-system-aarch64", "aarch64-linux-gnu-gcc", "aarch64-linux-gnu-gdb", "cmake", "ninja"]:
    command("version-" + binary, [binary, "--version"])
archive = subprocess.check_output(["git", "-C", str(repo), "archive", REV])
with tarfile.open(PREV / "experiment-evidence.tar.gz") as t:
    control = work / "archived-control"
    control.mkdir()
    for n in ["haven.bin", "haven.elf", "guest_a.bin", "guest_b.bin"]:
        (control/n).write_bytes(t.extractfile("physical-spi-aligned/"+n).read())
    firmware = work / "firmware"
    firmware.mkdir()
    for n in ["bl1.bin", "bl2.bin", "bl31.bin"]:
        (firmware/n).write_bytes(t.extractfile("replay-a/"+n).read())
    aligned_patch = t.extractfile("physical-spi-aligned/source.patch").read()
(work/"inputs.json").write_text(json.dumps({
    "revision": REV, "git_archive_sha256": hashlib.sha256(archive).hexdigest(),
    "reference_archive_sha256": sha(PREV/"experiment-evidence.tar.gz"),
    "abi_patch_sha256": sha(CORR/"0001-arm64-export-memcpy.patch"),
    "diagnostic_smc": "scaffolding only; not adopted",
    "firmware_sha256": {p.name:sha(p) for p in firmware.iterdir()},
}, indent=2)+"\n")


def qemu(name, images, expected, guest_probe=False):
    out = work / name
    out.mkdir()
    for n in ["haven.bin", "haven.elf", "guest_a.bin", "guest_b.bin"]:
        shutil.copy2(images/n, out/n)
    for p in firmware.iterdir():
        shutil.copy2(p, out/p.name)
    sock = work / (name + ".sock")
    argv = ["qemu-system-aarch64", "-machine", "virt,secure=on,virtualization=on,gic-version=3,iommu=smmuv3",
            "-cpu", "cortex-a72", "-smp", "4", "-m", "2G", "-display", "none", "-monitor", "none",
            "-serial", "file:uart.log", "-bios", "bl1.bin", "-semihosting-config", "enable=on,target=native"]
    for n, addr in [("haven.bin", "0x80000000"), ("guest_a.bin", "0x80800000"), ("guest_b.bin", "0xa0800000")]:
        argv += ["-device", f"loader,file={n},addr={addr},force-raw=on"]
    argv += ["-chardev", f"socket,path={sock},server=on,wait=off,id=gdb0", "-gdb", "chardev:gdb0", "-S"]
    script = ["set pagination off", "set confirm off", "set remotetimeout 3",
              f"target remote {sock}", "hbreak _start", "continue",
              'printf "CHECKPOINT PRIMARY_ENTRY\\n"', "info all-registers", "delete breakpoints",
              "hbreak hv_fatal_exception", "hbreak partitions_launch", "continue",
              'printf "CHECKPOINT PRIMARY_POST_INIT_OR_FATAL\\n"', "info all-registers"]
    if guest_probe and expected != "fault":
        script += ["delete breakpoints", "hbreak stage0_prepare", "continue",
                   'printf "CHECKPOINT SECONDARY_BEFORE_CPU_INIT\\n"', "info all-registers"]
    script += ["detach", "quit"]
    (out/"observe.gdb").write_text("\n".join(script)+"\n")
    gdb = ["aarch64-linux-gnu-gdb", "-q", "-nx", "-batch", "haven.elf", "-x", "observe.gdb"]
    start = time.monotonic()
    timed = False
    killed = False
    dbgcode = None
    try:
        with (out/"console.log").open("wb") as f:
            p = subprocess.Popen(argv, cwd=out, stdout=f, stderr=subprocess.STDOUT)
            try:
                for _ in range(100):
                    if sock.exists() or p.poll() is not None:
                        break
                    time.sleep(.01)
                with (out/"gdb.log").open("wb") as gf:
                    try:
                        dbgcode = subprocess.run(gdb, cwd=out, stdout=gf, stderr=subprocess.STDOUT,
                                                 timeout=12, check=False).returncode
                    except subprocess.TimeoutExpired:
                        dbgcode = 124
                try:
                    p.wait(timeout=4)
                except subprocess.TimeoutExpired:
                    timed = True
            finally:
                if p.poll() is None:
                    p.terminate()
                    try:
                        p.wait(timeout=2)
                    except subprocess.TimeoutExpired:
                        killed = True
                        p.kill()
                        p.wait()
    finally:
        log = (out/"uart.log").read_text(errors="replace") if (out/"uart.log").exists() else ""
        dbg = (out/"gdb.log").read_text(errors="replace") if (out/"gdb.log").exists() else ""
        verdict = dbgcode == 0 and "CHECKPOINT PRIMARY_ENTRY" in dbg
        if expected == "fault":
            verdict &= "EL2 FATAL:" in log and "hv_fatal_exception" in dbg
        else:
            verdict &= "EL2 FATAL:" not in log and "PARTITION_A: Post-fault check OK" in log
            verdict &= ("PROBE RESULT mode=2" in log and " PASS" in log) if guest_probe else "ABI TARGET PASS" in log
        result = dict(argv=argv, gdb_argv=gdb, cwd=str(out), debugger_timeout_s=12,
                      post_detach_timeout_s=4, kill_fallback_s=2, timed_out=timed,
                      forced_kill=killed, gdb_returncode=dbgcode, returncode=p.returncode,
                      elapsed_s=time.monotonic()-start, expected=expected, verdict=bool(verdict),
                      image_sha256={p.name:sha(p) for p in out.glob("*.bin")})
        (out/"run.json").write_text(json.dumps(result, indent=2)+"\n")
        results[name] = result
        (work/"results.json").write_text(json.dumps(results, indent=2)+"\n")
        print(name, "PASS" if verdict else "FAIL", flush=True)
    return verdict


qemu("control-aligned", control, "fault")

# On-target byte/canary/self-copy test; no host libc or haven/string.h alias.
fixture = (CORR/"memcpy_fixture.c").read_text().replace("int main(void)", "int stage0_abi_target(void)")
fixture += '\nvoid stage0_check_abi(void) { extern void hv_printk(const char *, ...); int rc=stage0_abi_target(); hv_printk("ABI TARGET %s rc=%d\\n", rc ? "FAIL" : "PASS", rc); }\n'
for name, strict, aligned in [("aligned-abi-default", False, True), ("aligned-abi-strict", True, True),
                              ("target-abi-default", False, False), ("target-abi-strict", True, False)]:
    src = work / (name+"-source")
    src.mkdir()
    with tarfile.open(fileobj=io.BytesIO(archive)) as t:
        t.extractall(src, filter="data")
    changes = ["src/common/string.c"]
    if aligned:
        (work/"aligned-probe.patch").write_bytes(aligned_patch)
        command("apply-probe-"+name, ["git", "apply", work/"aligned-probe.patch"], src)
        changes += ["CMakeLists.txt", "arch/arm64/boot.S", "src/core/partition.c",
                    "src/core/exc/el2_exceptions.c", "tests/demos/guest_b_entry.S", "stage0_probe.c"]
    else:
        command("apply-diagnostic-"+name, ["git", "apply", PREV/"session-start-haven.patch"], src)
        p = src/"arch/arm64/boot.S"
        p.write_text(p.read_text().replace("        hvc     #0", "        smc     #0"))
        p = src/"CMakeLists.txt"
        p.write_text(p.read_text().replace("        arch/arm64/cpu.c", "        stage0_abi_target.c\n        arch/arm64/cpu.c"))
        (src/"stage0_abi_target.c").write_text(fixture)
        p = src/"src/core/init.c"
        p.write_text(p.read_text().replace("\thv_print_banner();", "\thv_print_banner();\n\textern void stage0_check_abi(void); stage0_check_abi();"))
        changes += ["arch/arm64/boot.S", "src/core/exc/el2_exceptions.c", "CMakeLists.txt", "src/core/init.c", "stage0_abi_target.c"]
    command("apply-abi-"+name, ["git", "apply", CORR/"0001-arm64-export-memcpy.patch"], src)
    patch = []
    for rel in changes:
        old = subprocess.run(["git", "-C", str(repo), "show", REV+":"+rel], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL).stdout.decode()
        patch += difflib.unified_diff(old.splitlines(True), (src/rel).read_text().splitlines(True),
                                     fromfile="a/"+rel if old else "/dev/null", tofile="b/"+rel)
    (work/(name+".patch")).write_text("".join(patch))
    build = work/(name+"-build")
    config = ["cmake", "-S", src, "-B", build, "-G", "Ninja",
              "-DCMAKE_TOOLCHAIN_FILE="+str(src/"cmake/arm64.cmake"), "-DPLATFORM=qemu-virt", "-DCMAKE_BUILD_TYPE=MinSizeRel"]
    if strict:
        config += ["-DCMAKE_C_FLAGS=-mstrict-align"]
    command("configure-"+name, config)
    command("build-"+name, ["cmake", "--build", build, "--parallel", "4"])
    qemu(name, build, "pass" if strict or not aligned else "fault", guest_probe=aligned)

print("All expected verdicts:", all(x["verdict"] for x in results.values()), flush=True)
