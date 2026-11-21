#!/usr/bin/env python3
"""Row V0 of docs/methodology/stage2/20261121-readback-vttbr: does QEMU's
gdb stub name VTTBR_EL2 and HCR_EL2, and can they be read on a paused vCPU?

Usage: gdb_sysreg_probe.py [qemu-system-aarch64]
"""
import os
import re
import socket
import subprocess
import sys
import tempfile
import time


def pkt(data):
    return b"$" + data + b"#" + b"%02x" % (sum(data) % 256)


class Stub:
    def __init__(self, path):
        self.s = socket.socket(socket.AF_UNIX)
        self.s.connect(path)
        self.s.settimeout(5)
        self.buf = b""

    def cmd(self, data):
        self.s.sendall(pkt(data.encode()))
        while True:
            while b"#" not in self.buf or len(self.buf) < self.buf.index(b"#") + 3:
                self.buf += self.s.recv(65536)
            self.buf = self.buf.lstrip(b"+")
            if not self.buf.startswith(b"$"):
                self.buf = self.buf[1:]
                continue
            end = self.buf.index(b"#")
            body, self.buf = self.buf[1:end], self.buf[end + 3:]
            self.s.sendall(b"+")
            return body.decode(errors="replace")

    def xfer(self, annex):
        out, off = "", 0
        while True:
            r = self.cmd(f"qXfer:features:read:{annex}:{off:x},4000")
            out += r[1:]
            off += len(r) - 1
            if r.startswith("l") or not r:
                return out


def main():
    qemu = sys.argv[1] if len(sys.argv) > 1 else "qemu-system-aarch64"
    d = tempfile.mkdtemp()
    sock = os.path.join(d, "g")
    p = subprocess.Popen([qemu, "-M", "virt,virtualization=on,gic-version=3", "-cpu", "max", "-smp", "2",
                          "-m", "256M", "-display", "none", "-nic", "none", "-S",
                          "-chardev", f"socket,id=g,path={sock},server=on,wait=off", "-gdb", "chardev:g"],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        for _ in range(100):
            if os.path.exists(sock):
                break
            time.sleep(0.05)
        g = Stub(sock)
        g.cmd("qSupported:xmlRegisters=aarch64")
        target = g.xfer("target.xml")
        files = re.findall(r'href="([^"]+)"', target)
        print("version:", subprocess.run([qemu, "--version"], capture_output=True, text=True).stdout.splitlines()[0])
        print("features:", files)
        regnum, regs = 0, {}
        for f in ["target.xml"] + files:
            x = target if f == "target.xml" else g.xfer(f)
            for m in re.finditer(r"<reg ([^>]*)/>", x):
                a = dict(re.findall(r'(\w+)="([^"]*)"', m.group(1)))
                if "regnum" in a:
                    regnum = int(a["regnum"])
                regs[a["name"]] = (regnum, int(a.get("bitsize", "64")))
                regnum += 1
        for name in ("VTTBR_EL2", "HCR_EL2"):
            if name not in regs:
                print(name, "NOT in the target description")
                continue
            n, bits = regs[name]
            ids, r = [], g.cmd("qfThreadInfo")
            while r.startswith("m"):
                ids += r[1:].split(",")
                r = g.cmd("qsThreadInfo")
            for t in ids:
                g.cmd(f"Hg{t}")
                v = g.cmd(f"p{n:x}")
                print(f"{name} regnum {n} ({bits} bits), thread {t}: {v}")
    finally:
        p.kill()


if __name__ == "__main__":
    main()
