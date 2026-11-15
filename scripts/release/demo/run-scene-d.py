#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Scene D of the R1 demo: Linux with KDAL in HAVEN's partition A on 2 and
3 vCPUs, partition B beside it, from this bundle's files only.

Usage: ./run-scene-d.py [OUT]   (default: out/ next to this file)
Needs qemu-system-aarch64 and python3. Writes OUT/N2/uart.log and
OUT/N3/uart.log and checks them with the CI job's markers
(tools/linux-partition-check.py); exit 1 if a marker is missing.
"""
import importlib.util
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def load(name):
    spec = importlib.util.spec_from_file_location(name, os.path.join(HERE, "tools", name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    out = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "out"))
    boot, check = load("free_boot"), load("linux-partition-check")
    os.chdir(HERE)  # the commands name the bundle's files relative to it
    for n in (2, 3):
        d = os.path.join(out, f"N{n}")
        os.makedirs(d, exist_ok=True)
        with open(os.path.join("scene-d", f"N{n}", "qemu.json")) as f:
            argv = json.load(f)["argv"]
        uart = os.path.join(d, "uart.log")
        print(f"N = {n}: booting (about 10 s)...", flush=True)
        secs = boot.wait([a.replace("@UART@", uart) for a in argv], uart)
        print(f"N = {n}: stopped after {secs} s", flush=True)
    sys.argv = ["linux-partition-check.py", out]
    check.main()


if __name__ == "__main__":
    main()
