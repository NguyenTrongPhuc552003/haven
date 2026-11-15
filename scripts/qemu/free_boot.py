#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""A QEMU free boot with HAVEN's stop condition, standard library only, so
the R1 demo bundle carries this file unchanged
(docs/methodology/stage2/20261115-r1-demo-bundle, design item 1).

wait(cmd, uart, limit=120): start QEMU (cmd, whose serial output goes to
the file uart, written fresh); one second after the UART shows that
partition A stopped by PSCI, or an EL2 fatal error, or after limit
seconds, stop QEMU. Returns the seconds taken.
"""
import os
import subprocess
import time

STOP = ("partition stopped by PSCI", "EL2 FATAL")


def wait(cmd, uart, limit=120):
    if os.path.exists(uart):
        os.remove(uart)  # an old log would end the wait at once
    q = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    t0 = time.time()
    while time.time() - t0 < limit and q.poll() is None:
        text = open(uart, errors="replace").read() if os.path.exists(uart) else ""
        if any(s in text for s in STOP):
            time.sleep(1.0)
            break
        time.sleep(0.25)
    q.kill()
    q.wait()
    return round(time.time() - t0, 1)
