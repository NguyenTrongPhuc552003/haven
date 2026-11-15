#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Build the R1 demo bundle (docs/methodology/stage2/20261115-r1-demo-bundle,
design item 2): every scene's inputs in one archive.

Usage: make-demo-bundle.py HAVEN_REPO KDAL_SRC IMAGE KDAL_KO TRY_ARCHIVE OUT
Writes OUT/haven-r1-demo-<describe>.tar.gz (entries owned by uid 0 with no
names, times SOURCE_DATE_EPOCH or the commit time) and prints its path.
Scene D's inputs are built with run_n.py's steps (initramfs, partition A's
tree and HAVEN for N = 2 and N = 3); each QEMU command is stored as JSON
with the bundle's file names, for run-scene-d.py.
"""
import gzip
import hashlib
import importlib.util
import json
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.join(HERE, "..", "..")


def load(name, *path):
    spec = importlib.util.spec_from_file_location(name, os.path.join(REPO, *path))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


run_n = load("run_n", "docs", "methodology", "stage2", "20261112-multi-vcpu-n", "run_n.py")


def scene_d(repo, ksrc, image, ko, stage, work):
    """Scene D's files under STAGE/scene-d/ and one qemu.json per N."""
    sd = os.path.join(stage, "scene-d")
    os.makedirs(sd)
    cpio = run_n.initramfs(ksrc, ko, work)
    shutil.copy(image, os.path.join(sd, "Image"))
    shutil.copy(cpio, os.path.join(sd, "initramfs.cpio"))
    for n in (2, 3):
        logs = os.path.join(work, f"logs{n}")  # tree and build log stay out of the bundle
        os.makedirs(logs)
        b, dtb = run_n.prepare(repo, cpio, n, logs, work)
        d = os.path.join(sd, f"N{n}")
        os.makedirs(d)
        for f in ("haven.bin", "guest_b.bin"):
            shutil.copy(os.path.join(b, f), os.path.join(d, f))
        shutil.copy(dtb, os.path.join(d, "partition-a.dtb"))
        names = {f"{b}/haven.bin": f"scene-d/N{n}/haven.bin", f"{b}/guest_b.bin": f"scene-d/N{n}/guest_b.bin",
                 dtb: f"scene-d/N{n}/partition-a.dtb", image: "scene-d/Image", cpio: "scene-d/initramfs.cpio"}
        argv = run_n.smp1.free_boot_cmd(b, image, dtb, cpio, run_n.IPA + run_n.rc.PA_OFFSET, "@UART@")
        for old, new in names.items():
            argv = [a.replace(old, new) for a in argv]
        leftover = [a for a in argv if work in a or os.path.isabs(a.split("file=")[-1].split(",")[0])]
        assert not leftover, f"absolute paths left in the command: {leftover}"
        with open(os.path.join(d, "qemu.json"), "w") as f:
            json.dump({"argv": argv}, f, indent=1)


def manifest(stage):
    lines = []
    for root, _, files in os.walk(stage):
        for f in files:
            p = os.path.join(root, f)
            with open(p, "rb") as fh:
                lines.append(f"{hashlib.sha256(fh.read()).hexdigest()}  {os.path.relpath(p, stage)}")
    with open(os.path.join(stage, "MANIFEST.sha256"), "w") as f:
        f.write("\n".join(sorted(lines, key=lambda line: line.split("  ", 1)[1])) + "\n")


def archive(stage, out, epoch):
    """A gzip'd tar of STAGE: sorted names, uid 0, no names, fixed times."""
    name = os.path.basename(stage)

    def reset(info):
        info.uid = info.gid = 0
        info.uname = info.gname = ""
        info.mtime = epoch
        return info
    path = os.path.join(out, name + ".tar.gz")
    with open(path, "wb") as raw, gzip.GzipFile(fileobj=raw, mode="wb", mtime=0, filename="") as gz, \
            tarfile.open(fileobj=gz, mode="w", format=tarfile.PAX_FORMAT) as tar:
        for root, dirs, files in os.walk(stage):
            dirs.sort()
            for f in sorted(files):
                p = os.path.join(root, f)
                tar.add(p, arcname=os.path.join(name, os.path.relpath(p, stage)), filter=reset)
    return path


def main():
    if len(sys.argv) != 7:
        sys.exit(__doc__)
    repo, ksrc, image, ko, try_archive, out = (os.path.abspath(a) for a in sys.argv[1:])
    git = ["git", "-C", repo]
    desc = subprocess.run(git + ["describe", "--tags", "--match", "v*", "--always", "--dirty"],
                          capture_output=True, text=True, check=True).stdout.strip()
    epoch = int(os.environ.get("SOURCE_DATE_EPOCH") or subprocess.run(
        git + ["log", "-1", "--format=%ct"], capture_output=True, text=True, check=True).stdout)
    work = tempfile.mkdtemp(prefix="r1-demo-")
    stage = os.path.join(work, f"haven-r1-demo-{desc}")
    os.makedirs(os.path.join(stage, "tools"))
    os.makedirs(os.path.join(stage, "scenes-a-c"))
    try:
        scene_d(repo, ksrc, image, ko, stage, work)
        shutil.copy(try_archive, os.path.join(stage, "scenes-a-c"))
        for src, dst in (("scripts/qemu/free_boot.py", "tools/free_boot.py"),
                         ("scripts/qemu/linux-partition-check.py", "tools/linux-partition-check.py"),
                         ("scripts/release/demo/run-scene-d.py", "run-scene-d.py"),
                         ("scripts/release/demo/README.md", "README.md"),
                         ("ecosystem.lock.json", "ecosystem.lock.json")):
            shutil.copy(os.path.join(repo, src), os.path.join(stage, dst))
        os.chmod(os.path.join(stage, "run-scene-d.py"), 0o755)
        manifest(stage)
        os.makedirs(out, exist_ok=True)
        print(archive(stage, out, epoch))
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
