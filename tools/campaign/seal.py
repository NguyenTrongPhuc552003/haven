#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Seal the owner's held-out cases into one archive (campaign step C3).

docs/methodology/stage2/20261119-evaluation-campaign/c3/README.md, design
items 1 and 2. DIR holds H<n>.patch and H<n>.json, the latter
{"class": CLASS} with an optional "expect" in the written cases' form.
seal.py writes DIR.tar.gz (the archive, byte-reproducible) and DIR.b64
(its base64, for the repository secret) next to DIR, and prints only the
count, each case's id and class, and the archive's SHA-256: never a file
name inside the archive or a patch's text.

--build-check applies each patch to an export of the repository's HEAD
and builds the host tests, qemu-direct and qemu-psci, printing only
"H1: builds", "H1: does not build" or "H1: does not apply".

Usage: seal.py DIR [--build-check] [--repo REPO]
"""
import argparse
import base64
import gzip
import hashlib
import io
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SCHEMA = "haven-campaign-cases/1"
GRANT = "docs/methodology/stage2/20261108-el3-readback/grant-demo-cpus.json"
METHODS = ("M1", "M2", "M3", "M4", "M5", "M6", "M7")
VERDICTS = ("pass", "finding", "n/a", "error")
MAX_B64 = 46 * 1024  # a repository secret holds at most 48 KB
CASE_ID = re.compile(r"[HT][0-9]+")
MEMBER = re.compile(r"[A-Za-z0-9_][A-Za-z0-9_.-]*")


class SealError(Exception):
    """A reason to stop; its text never names a file inside the archive."""


def known_classes():
    with open(os.path.join(HERE, "taxonomy.json")) as f:
        return {c["id"] for c in json.load(f)["classes"]}


def check_cases(doc, members):
    """Refuse a cases file the campaign could not run as sealed cases."""
    if doc.get("schema") != SCHEMA:
        raise SealError(f"cases.json: schema is not {SCHEMA}")
    cases = doc.get("cases") or []
    if not cases:
        raise SealError("cases.json: no cases")
    classes, seen = known_classes(), set()
    for c in cases:
        cid = c.get("case", "")
        if not CASE_ID.fullmatch(cid) or cid in seen:
            raise SealError("cases.json: a case id is missing, malformed or repeated")
        seen.add(cid)
        if c.get("source") != "heldout":
            raise SealError(f"{cid}: source is not heldout")
        if c.get("class") not in classes:
            raise SealError(f"{cid}: class is not in taxonomy.json")
        if c.get("patch") not in members:
            raise SealError(f"{cid}: its patch is not in the archive")
    for cid, exp in (doc.get("expect") or {}).items():
        if cid not in seen:
            raise SealError("cases.json: an expectation names no case")
        if not isinstance(exp, dict) or any(m not in METHODS or v not in VERDICTS for m, v in exp.items()):
            raise SealError(f"{cid}: an expectation is not METHOD: pass|finding|n/a|error")


def build_archive(files):
    """Name-to-bytes into a gzip tar that depends on nothing but the bytes."""
    raw = io.BytesIO()
    with tarfile.open(fileobj=raw, mode="w", format=tarfile.USTAR_FORMAT) as t:
        for name in sorted(files):
            info = tarfile.TarInfo(name)
            info.size, info.mtime, info.mode = len(files[name]), 0, 0o644
            info.uid = info.gid = 0
            info.uname = info.gname = ""
            t.addfile(info, io.BytesIO(files[name]))
    return gzip.compress(raw.getvalue(), compresslevel=9, mtime=0)


def collect(d, prefix="H", source="campaign step C3, the owner's held-out cases"):
    """DIR's patches and their .json files into (cases document, archive bytes)."""
    pat = re.compile(prefix + r"([0-9]+)\.patch")
    patches = sorted((n for n in os.listdir(d) if pat.fullmatch(n)), key=lambda n: int(pat.fullmatch(n).group(1)))
    if not patches:
        raise SealError(f"no {prefix}<n>.patch files")
    files, cases, expect = {}, [], {}
    for p in patches:
        cid = p[: -len(".patch")]
        try:
            with open(os.path.join(d, cid + ".json")) as f:
                meta = json.load(f)
        except (OSError, ValueError):
            raise SealError(f"{cid}: {cid}.json is missing or not JSON") from None
        with open(os.path.join(d, p), "rb") as f:
            files[p] = f.read()
        cases.append({"case": cid, "class": meta.get("class"), "source": "heldout", "patch": p})
        if meta.get("expect"):
            expect[cid] = meta["expect"]
    doc = {"schema": SCHEMA, "source": source, "grant": GRANT, "cases": cases, "expect": expect}
    check_cases(doc, set(files))
    files["cases.json"] = (json.dumps(doc, indent=1, sort_keys=True) + "\n").encode()
    return doc, build_archive(files)


def encode(archive):
    text = base64.b64encode(archive).decode() + "\n"
    if len(text) > MAX_B64:
        raise SealError(f"the archive's base64 is {len(text)} bytes, over the secret's limit ({MAX_B64})")
    return text


def builds(repo, patch):
    """Apply one patch to an export of HEAD and build; nothing is printed."""
    src = tempfile.mkdtemp(prefix="seal-")
    quiet = {"stdout": subprocess.DEVNULL, "stderr": subprocess.DEVNULL}
    try:
        subprocess.run(f"git -C '{repo}' archive HEAD | tar -x -C '{src}'", shell=True, check=True, **quiet)
        if subprocess.run(["git", "apply", patch], cwd=src, **quiet).returncode != 0 and \
                subprocess.run(["patch", "-p1", "-i", patch], cwd=src, **quiet).returncode != 0:
            return "does not apply"
        for cmd in ("cmake --preset host-tests && cmake --build build-host",
                    "cmake --preset arm64-qemu -DHAVEN_BOOT_PROFILE=qemu-direct && cmake --build build",
                    "cmake --preset arm64-qemu -B build-psci -DHAVEN_BOOT_PROFILE=qemu-psci && cmake --build build-psci"):
            if subprocess.run(cmd, cwd=src, shell=True, **quiet).returncode != 0:
                return "does not build"
        return "builds"
    finally:
        shutil.rmtree(src, ignore_errors=True)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("dir")
    ap.add_argument("--build-check", action="store_true")
    ap.add_argument("--repo", default=os.path.join(HERE, "..", ".."))
    args = ap.parse_args(argv)
    d = os.path.abspath(args.dir).rstrip(os.sep)
    try:
        doc, archive = collect(d)
        text = encode(archive)
    except SealError as e:
        print(f"seal: {e}", file=sys.stderr)
        return 1
    print(f"sealed archive: {len(doc['cases'])} cases")
    for c in doc["cases"]:
        line = f"  {c['case']} {c['class']}"
        if args.build_check:
            line += ": " + builds(os.path.abspath(args.repo), os.path.join(d, c["patch"]))
        print(line)
    with open(d + ".tar.gz", "wb") as f:
        f.write(archive)
    with open(d + ".b64", "w") as f:
        f.write(text)
    print(f"sha256 {hashlib.sha256(archive).hexdigest()}  {len(doc['cases'])}")
    print(f"wrote {d}.tar.gz and {d}.b64 ({len(text)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
