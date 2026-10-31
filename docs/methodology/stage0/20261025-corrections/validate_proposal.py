#!/usr/bin/env python3
"""Validate an unapplied ABI proposal in a fresh, disposable source export.

No QEMU, vendor sources, existing build trees or tracked implementation edits.
Each command has a 120 second bound. Logs include exact argv and cwd.
"""
import argparse
import hashlib
import io
import json
import pathlib
import re
import subprocess
import tarfile

REVISION = "f775304ce9443dc9861ba1880e4208c7ae19b314"
HERE = pathlib.Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument("--haven", required=True, type=pathlib.Path)
parser.add_argument("--work", required=True, type=pathlib.Path)
args = parser.parse_args()
repo = args.haven.resolve()
work = args.work.resolve()
work.mkdir(parents=True, exist_ok=False)
records = []


def run(name, argv, cwd=work, expected=0):
    result = subprocess.run([str(a) for a in argv], cwd=cwd,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            timeout=120, check=False)
    (work / (name + ".log")).write_bytes(result.stdout)
    record = {"name": name, "argv": [str(a) for a in argv],
              "cwd": str(cwd), "timeout_s": 120,
              "returncode": result.returncode,
              "expected": expected, "termination": "process exited"}
    records.append(record)
    (work / "commands.json").write_text(json.dumps(records, indent=2) + "\n")
    if expected == "nonzero":
        assert result.returncode != 0, record
    else:
        assert result.returncode == expected, record
    return result.stdout.decode(errors="replace")


for binary in ["aarch64-linux-gnu-gcc", "cmake", "ninja", "clang", "python3"]:
    run("version-" + binary, [binary, "--version"])
run("host", ["uname", "-a"])
archive = subprocess.check_output(["git", "-C", str(repo), "archive", REVISION])
source = work / "source"
source.mkdir()
with tarfile.open(fileobj=io.BytesIO(archive)) as bundle:
    bundle.extractall(source, filter="data")
(work / "inputs.json").write_text(json.dumps({
    "revision": REVISION, "git_archive_sha256": hashlib.sha256(archive).hexdigest(),
    "git_archive_argv": ["git", "-C", str(repo), "archive", REVISION],
    "proposal_sha256": hashlib.sha256((HERE / "0001-arm64-export-memcpy.patch").read_bytes()).hexdigest(),
    "fixture_sha256": hashlib.sha256((HERE / "memcpy_fixture.c").read_bytes()).hexdigest(),
}, indent=2) + "\n")


def configure(name, strict):
    argv = ["cmake", "-S", source, "-B", work / name, "-G", "Ninja",
            "-DCMAKE_TOOLCHAIN_FILE=" + str(source / "cmake/arm64.cmake"),
            "-DPLATFORM=qemu-virt", "-DCMAKE_BUILD_TYPE=MinSizeRel"]
    if strict:
        argv.append("-DCMAKE_C_FLAGS=-mstrict-align")
    run("configure-" + name, argv)


configure("baseline-strict", True)
failure = run("build-baseline-strict", ["cmake", "--build", work / "baseline-strict",
                                      "--parallel", "4"], expected="nonzero")
assert "undefined reference to `memcpy'" in failure
assert "stage2.c" in failure
run("apply-check", ["git", "apply", "--check", HERE / "0001-arm64-export-memcpy.patch"], source)
run("apply", ["git", "apply", HERE / "0001-arm64-export-memcpy.patch"], source)

for name, strict in [("proposal-default", False), ("proposal-strict", True)]:
    configure(name, strict)
    build = work / name
    run("build-" + name, ["cmake", "--build", build, "--parallel", "4"])
    obj = build / "CMakeFiles/haven.dir/src/common/string.c.obj"
    defined = run("symbols-" + name, ["aarch64-linux-gnu-nm", "--defined-only", obj])
    assert re.search(r"\bT memcpy$", defined, re.M)
    undefined = run("undefined-object-" + name, ["aarch64-linux-gnu-nm", "-u", obj])
    assert not undefined.strip(), undefined
    undefined_image = run("undefined-image-" + name, ["aarch64-linux-gnu-nm", "-u", build / "haven.elf"])
    assert not undefined_image.strip(), undefined_image
    dis = run("disassembly-" + name, ["aarch64-linux-gnu-objdump", "-dr", obj])
    # A libcall from any hv_* helper into these ABI names could recurse.
    for symbol, body in re.findall(r"^[0-9a-f]+ <([^>]+)>:\n(.*?)(?=^[0-9a-f]+ <|\Z)", dis, re.M | re.S):
        if symbol.startswith("hv_"):
            assert not re.search(r"R_AARCH64_(?:CALL26|JUMP26)\s+(?:memcpy|memset|memmove|memcmp)\b", body), symbol

for mode, define in [("host-guard", []), ("host-abi", ["-DHAVEN_ARCH_ARM64"])]:
    obj = work / (mode + ".o")
    run("compile-" + mode, ["clang", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                           "-ffreestanding", "-fno-builtin", *define, "-c",
                           source / "src/common/string.c", "-o", obj])
    symbols = run("symbols-" + mode, ["nm", "-g", obj])
    exported = bool(re.search(r"\bT _?memcpy$", symbols, re.M))
    assert exported == (mode == "host-abi")
    if mode == "host-abi":
        run("link-host-fixture", ["clang", "-std=c11", "-O2", "-fno-builtin",
                                  HERE / "memcpy_fixture.c", obj, "-o", work / "memcpy-fixture"])
        run("run-host-fixture", [work / "memcpy-fixture"])

files = []
for path in sorted(work.rglob("*")):
    if path.is_file() and (path.parent == work or path.suffix in [".elf", ".bin"]):
        files.append({"path": str(path.relative_to(work)), "bytes": path.stat().st_size,
                      "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
(work / "summary.json").write_text(json.dumps({
    "result": "PASS", "claims": ["baseline strict build fails specifically on memcpy",
    "unapplied proposal builds default and strict", "no unresolved image symbols",
    "no memory libcall recursion in inspected ARM64 objects", "host export guard and byte semantics pass"],
    "not_run": ["QEMU", "physical boards", "runtime alignment regression", "PSCI/IRQ/timer regression"],
    "files": files}, indent=2) + "\n")
print("PASS: isolated ABI build/link and host fixture; no QEMU run")
