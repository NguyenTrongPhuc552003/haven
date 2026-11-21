#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Open the sealed archive in CI, after checking it against the committed hash.

docs/methodology/stage2/20261119-evaluation-campaign/c3/README.md, design
item 4. The archive's base64 comes from an environment variable (the
repository secret) or, for row K7 only, a committed file. Its SHA-256 is
checked against HASH_FILE ("<sha256>  <count>") before anything is
written; members that are not plain files with plain names are refused;
OUT/cases.json gets each patch's absolute path in OUT. Prints only
"sealed archive: hash matches, N cases" or the reason it stops, never a
name inside the archive.

Usage: unseal.py HASH_FILE OUT (--from-env NAME | --from-file PATH)
"""
import argparse
import base64
import binascii
import hashlib
import io
import json
import os
import re
import sys
import tarfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from seal import MEMBER, SealError, check_cases  # noqa: E402


def read_hash(path):
    """HASH_FILE's first line: a SHA-256 and, optionally, the case count."""
    with open(path) as f:
        fields = f.readline().split()
    if not fields or not re.fullmatch(r"[0-9a-f]{64}", fields[0]):
        raise SealError("the hash file does not start with a SHA-256")
    count = int(fields[1]) if len(fields) > 1 and fields[1].isdigit() else None
    return fields[0], count


def decode(text):
    try:
        return base64.b64decode("".join(text.split()), validate=True)
    except (binascii.Error, ValueError):
        raise SealError("the archive text is not base64") from None


def open_archive(data, want, count=None):
    """Checked bytes into (cases document, name-to-bytes); nothing is written."""
    if hashlib.sha256(data).hexdigest() != want:
        raise SealError("hash mismatch: the archive is not the committed one; no case runs")
    members = {}
    try:
        with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as t:
            for m in t.getmembers():
                if not m.isfile() or not MEMBER.fullmatch(m.name) or m.name in members:
                    raise SealError("the archive holds a member that is not a plain file with a plain name")
                members[m.name] = t.extractfile(m).read()
    except (tarfile.TarError, OSError, EOFError):
        raise SealError("the archive is not a gzip tar") from None
    if "cases.json" not in members:
        raise SealError("the archive has no cases.json")
    try:
        doc = json.loads(members.pop("cases.json"))
    except ValueError:
        raise SealError("the archive's cases.json is not JSON") from None
    check_cases(doc, set(members))
    if count is not None and len(doc["cases"]) != count:
        raise SealError("the archive's case count differs from the committed count")
    return doc, members


def write(out, doc, members):
    os.makedirs(out, exist_ok=False)
    for name, data in members.items():
        with open(os.path.join(out, name), "wb") as f:
            f.write(data)
    for c in doc["cases"]:
        c["patch"] = os.path.join(out, c["patch"])
    with open(os.path.join(out, "cases.json"), "w") as f:
        json.dump(doc, f, indent=1)
        f.write("\n")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("hash_file")
    ap.add_argument("out")
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--from-env", metavar="NAME")
    src.add_argument("--from-file", metavar="PATH")
    args = ap.parse_args(argv)
    try:
        want, count = read_hash(args.hash_file)
        if args.from_env:
            text = os.environ.get(args.from_env, "")
            if not text.strip():
                raise SealError(f"{args.from_env} is empty: the secret is not set; no case runs")
        else:
            with open(args.from_file) as f:
                text = f.read()
        doc, members = open_archive(decode(text), want, count)
        write(os.path.abspath(args.out), doc, members)
    except (SealError, OSError) as e:
        print(f"unseal: {e}", file=sys.stderr)
        return 1
    print(f"sealed archive: hash matches, {len(doc['cases'])} cases")
    return 0


if __name__ == "__main__":
    sys.exit(main())
