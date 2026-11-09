#!/usr/bin/env python3
"""Partition A's device tree for the Orange Pi 6 Plus, from sky1-xcp.dts and a grant (D3).

Usage: gen_partition_a.py LINUX_DTS_DIR GRANT OUTDIR [--seeds]

LINUX_DTS_DIR holds the mainline files of D1 (`arch/arm64/boot/dts/cix/`
and `include/dt-bindings/`, Linux v7.3). The board tree is preprocessed with
clang and compiled with dtc in a temporary directory; nothing is written
next to it. OUTDIR receives partition-a.dts and partition-a.dtb, and with
--seeds the seeded trees of the record's Expectations table under
OUTDIR/seeds/. The tree is built from an allow-list (README.md, Design):
CPUs and RAM from the grant, the granted PL011 devices with a fixed clock,
HAVEN's emulated GICv3, PSCI over HVC and the virtual-timer node. Board
facts the design relies on are checked first; a mismatch stops the run.
Standard library only (Python 3.9); needs clang and dtc.
"""
import argparse
import hashlib
import json
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "..", "tools", "readback"))
import dt_check  # noqa: E402  (parse_fdt, cells, num, strs)

BOARD_DTS = "arch/arm64/boot/dts/cix/sky1-xcp.dts"
SOC_DTSI = "arch/arm64/boot/dts/cix/sky1.dtsi"
# HAVEN logical CPU -> MPIDR (core number in Aff1), boot core 4 first:
# ledger "CPU numbering" (CIX BIOS MADT order). D4 fixes this in code.
CPU_MAP = [0x400, 0x000, 0x100, 0x200, 0x300, 0x500, 0x600, 0x700, 0x800, 0x900, 0xA00, 0xB00]
UART_CLK_HZ = 100000000  # PL011UartClkInHz, CIX BIOS Sky1Common.dsc.inc (boards)
GICR_FRAME = 0x20000  # one GICv3 redistributor frame (RD + SGI), HAVEN's vGIC
NORMAL = {"R", "RW", "RX", "RWX"}
DEVICE = {"DEVICE_R", "DEVICE_RW"}
# Properties written in decimal in the .dts (counts, interrupt numbers, rates).
DECIMAL = {"#address-cells", "#size-cells", "#interrupt-cells", "#clock-cells", "interrupts",
           "capacity-dmips-mhz", "clock-frequency"}
SEEDS = ["S1", "S2a", "S2b", "S3", "S3b", "S4", "S5", "S6"]


def sha256(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def fail(msg):
    sys.exit(f"gen_partition_a: board fact differs from the design: {msg}")


# ------------------------------------------------------------ board tree
def load_board(src):
    with tempfile.TemporaryDirectory() as d:
        pp, dtb = os.path.join(d, "xcp.dts"), os.path.join(d, "xcp.dtb")
        subprocess.run(["clang", "-E", "-P", "-nostdinc", "-I", "include", "-I", "arch/arm64/boot/dts",
                        "-I", "arch/arm64/boot/dts/cix", "-undef", "-D__DTS__", "-x", "assembler-with-cpp",
                        BOARD_DTS, "-o", pp], cwd=src, check=True)
        subprocess.run(["dtc", "-q", "-I", "dts", "-O", "dtb", "-o", dtb, pp], check=True)
        with open(dtb, "rb") as f:
            return dt_check.parse_fdt(f.read())


def node(root, path):
    n = root
    for part in path.strip("/").split("/"):
        n = next((c for c in n["children"] if c["name"] == part), None)
        if n is None:
            return None
    return n


def regs(n, ac=2, sc=2):
    return dt_check.ranges_of(n["props"]["reg"], ac, sc)


def prop_str(n, name, default=""):
    v = dt_check.strs(n["props"].get(name, b""))
    return v[0] if v else default


def check_board(b):
    """The facts README.md states; stop if the source says otherwise."""
    u2, u3 = node(b, "/soc@0/serial@40d0000"), node(b, "/soc@0/serial@40e0000")
    if not u2 or regs(u2) != [(0x040D0000, 0x1000)] or prop_str(u2, "status") != "okay":
        fail("UART2 is not an enabled PL011 at 0x040D0000")
    if not u3 or regs(u3) != [(0x040E0000, 0x1000)] or prop_str(u3, "status") != "disabled":
        fail("UART3 is not a disabled PL011 at 0x040E0000")
    if dt_check.cells(u3["props"]["interrupts"])[:2] != [0, 299]:
        fail("UART3 is not SPI 299 (INTID 331)")
    if prop_str(node(b, "/chosen"), "stdout-path") != "/soc@0/serial@40d0000":
        fail("stdout-path is not UART2")
    gic = node(b, "/soc@0/interrupt-controller@e010000")
    if not gic or regs(gic) != [(0x0E010000, 0x10000), (0x0E090000, 0x300000)]:
        fail("GIC is not GICD 0x0E010000 + 64 KiB, GICR 0x0E090000 + 3 MiB")
    if dt_check.cells(gic["props"]["#interrupt-cells"]) != [4]:
        fail("GIC #interrupt-cells is not 4")
    if prop_str(node(b, "/psci"), "method") != "smc":
        fail("PSCI method is not smc")
    if len(dt_check.cells(node(b, "/timer")["props"]["interrupts"])) != 20:
        fail("timer does not have five 4-cell interrupts")
    cma = node(b, "/reserved-memory/linux,cma")
    if not cma or dt_check.num(dt_check.cells(cma["props"]["size"])) != 0x28000000:
        fail("no 640 MiB linux,cma pool")
    return {"uart2": u2, "uart3": u3, "gic": gic, "cma": cma}


def board_cpu(b, mpidr):
    for c in node(b, "/cpus")["children"]:
        if "reg" in c["props"] and dt_check.num(dt_check.cells(c["props"]["reg"])) == mpidr:
            return c
    fail(f"no cpu node with MPIDR {mpidr:#x}")


# ------------------------------------------------------------ output tree
class N:
    """A node of the output tree: props are (name, value) with value None
    (empty), ("s", [str]) or ("c", [[cell or "&label"], ...])."""

    def __init__(self, name, label=None):
        self.name, self.label, self.props, self.children = name, label, [], []

    def s(self, name, *vals):
        self.props.append((name, ("s", list(vals))))
        return self

    def c(self, name, *groups):
        self.props.append((name, ("c", [list(g) for g in groups])))
        return self

    def e(self, name):
        self.props.append((name, None))
        return self

    def add(self, child):
        self.children.append(child)
        return child


def render(n, depth=0):
    tab = "\t" * depth
    head = f"{n.label}: {n.name}" if n.label else n.name
    out = [f"{tab}{head} {{"]
    for name, v in n.props:
        if v is None:
            out.append(f"{tab}\t{name};")
        elif v[0] == "s":
            out.append(f"{tab}\t{name} = " + ", ".join('"%s"' % s for s in v[1]) + ";")
        else:
            fmt = "%d" if name in DECIMAL else "0x%x"
            gs = ["<" + " ".join(x if isinstance(x, str) else fmt % x for x in g) + ">" for g in v[1]]
            out.append(f"{tab}\t{name} = " + ", ".join(gs) + ";")
    for ch in n.children:
        if len(out) > 1:
            out.append("")
        out.extend(render(ch, depth + 1))
    out.append(f"{tab}}};")
    return out


def hi_lo(v):
    return [v >> 32, v & 0xFFFFFFFF]


def three_cells(raw, keep4=False):
    """A board specifier list (4 cells each) -> groups of 3 (or 4 for S5)."""
    cs = dt_check.cells(raw)
    return [cs[i:i + 4] if keep4 else cs[i:i + 3] for i in range(0, len(cs), 4)]


def pl011(src, keep4=False):
    reg = regs(src)[0]
    n = N(src["name"])
    n.s("compatible", *dt_check.strs(src["props"]["compatible"]))
    n.c("reg", hi_lo(reg[0]) + hi_lo(reg[1]))
    n.c("interrupts", *three_cells(src["props"]["interrupts"], keep4))
    n.c("clocks", ["&uart_clk"], ["&uart_clk"])
    n.s("clock-names", "uartclk", "apb_pclk")
    n.s("status", "okay")
    return n


def cpu_node(src):
    mpidr = dt_check.num(dt_check.cells(src["props"]["reg"]))
    n = N(f"cpu@{mpidr:x}", label=f"cpu{mpidr >> 8}")
    n.s("device_type", "cpu")
    n.s("compatible", *dt_check.strs(src["props"]["compatible"]))
    n.c("reg", hi_lo(mpidr))
    n.s("enable-method", "psci")
    n.c("capacity-dmips-mhz", dt_check.cells(src["props"]["capacity-dmips-mhz"]))
    return n


def build(b, facts, g, seed=None):
    pid = "1"
    regions = g["partitions"][pid]
    ram = [(int(r["ipa"], 16), int(r["size"], 16)) for r in regions if r["attrs"] in NORMAL]
    devs = [r for r in regions if r["attrs"] in DEVICE]
    for r in regions:
        if r["ipa"].lower() != r["pa"].lower():
            sys.exit(f"gen_partition_a: region {r['name']} is not identity mapped (design: IPA = PA)")
    logical = sorted(int(c) for c, p in g["gic"]["cpus"].items() if str(p) == pid)
    mpidrs = sorted(CPU_MAP[c] for c in logical)
    if seed == "S2a":
        mpidrs.append(0x800)
    if seed == "S2b":
        mpidrs = [0x800 if m == 0x700 else m for m in mpidrs]
    keep4 = seed == "S5"

    root = N("/")
    root.s("model", prop_str(b, "model"))
    root.s("compatible", *dt_check.strs(b["props"]["compatible"]))
    root.c("#address-cells", [2]).c("#size-cells", [2]).c("interrupt-parent", ["&gic"])

    granted_uarts = []
    for r in devs:
        base, size = int(r["ipa"], 16), int(r["size"], 16)
        src = next((c for c in node(b, "/soc@0")["children"]
                    if "reg" in c["props"] and regs(c)[:1] == [(base, size)]), None)
        if src is None or "arm,pl011" not in dt_check.strs(src["props"].get("compatible", b"")):
            sys.exit(f"gen_partition_a: region {r['name']} is not a PL011 of the board tree "
                     "(only PL011 devices are supported in the first bring-up)")
        granted_uarts.append(src)
    aliases = root.add(N("aliases"))
    board_aliases = node(b, "/aliases")["props"]
    alias_of = {}
    for name, val in board_aliases.items():
        alias_of[val.rstrip(b"\0").decode()] = name
    for u in granted_uarts:
        path = "/soc@0/" + u["name"]
        if path in alias_of:
            aliases.s(alias_of[path], path)
    chosen = root.add(N("chosen"))
    chosen.s("bootargs", "earlycon panic=-1")
    stdout = alias_of.get(prop_str(node(b, "/chosen"), "stdout-path"))
    if stdout and any("/soc@0/" + u["name"] == prop_str(node(b, "/chosen"), "stdout-path") for u in granted_uarts):
        chosen.s("stdout-path", f"{stdout}:115200n8")

    for base, size in ram:
        if seed == "S3":
            size *= 2
        if seed == "S3b":
            base, size = 0x80000000, 0x80000000
        root.add(N(f"memory@{base:x}")).s("device_type", "memory").c("reg", hi_lo(base) + hi_lo(size))

    resv = root.add(N("reserved-memory"))
    resv.c("#address-cells", [2]).c("#size-cells", [2]).e("ranges")
    cma = resv.add(N("linux,cma"))
    cma.s("compatible", "shared-dma-pool").e("reusable")
    cma.c("size", dt_check.cells(facts["cma"]["props"]["size"])).e("linux,cma-default")

    cpus = root.add(N("cpus"))
    cpus.c("#address-cells", [2]).c("#size-cells", [0])
    for m in mpidrs:
        cpus.add(cpu_node(board_cpu(b, m)))
    cluster = cpus.add(N("cpu-map")).add(N("cluster0"))
    for i, m in enumerate(mpidrs):
        cluster.add(N(f"core{i}")).c("cpu", [f"&cpu{m >> 8}"])

    psci = root.add(N("psci"))
    psci.s("compatible", "arm,psci-1.0", "arm,psci-0.2")
    psci.s("method", "smc" if seed == "S6" else "hvc")

    gicr = regs(facts["gic"])
    gicd, gicr_base, gicr_board = gicr[0], gicr[1][0], gicr[1][1]
    nvcpu = len(logical)
    gicr_size = gicr_board if seed == "S4" else nvcpu * GICR_FRAME
    if nvcpu * GICR_FRAME > gicr_board:
        sys.exit("gen_partition_a: more vCPU frames than the board's redistributor region")
    gic = root.add(N(f"interrupt-controller@{gicd[0]:x}", label="gic"))
    gic.s("compatible", "arm,gic-v3")
    gic.c("#interrupt-cells", [4 if keep4 else 3]).c("#address-cells", [0]).e("interrupt-controller")
    gic.c("reg", hi_lo(gicd[0]) + hi_lo(gicd[1]), hi_lo(gicr_base) + hi_lo(gicr_size))

    timer = root.add(N("timer"))
    timer.s("compatible", "arm,armv8-timer")
    timer.c("interrupts", *three_cells(node(b, "/timer")["props"]["interrupts"], keep4)[:4])

    clk = root.add(N(f"clock-{UART_CLK_HZ}", label="uart_clk"))
    clk.s("compatible", "fixed-clock").c("#clock-cells", [0]).c("clock-frequency", [UART_CLK_HZ])
    clk.s("clock-output-names", "uart_clk")

    soc_src = node(b, "/soc@0")
    soc = root.add(N("soc@0"))
    soc.s("compatible", "simple-bus")
    soc.c("ranges", dt_check.cells(soc_src["props"]["ranges"]))
    soc.e("dma-ranges").c("#address-cells", [2]).c("#size-cells", [2])
    for u in granted_uarts:
        soc.add(pl011(u, keep4))
    if seed == "S1":
        soc.add(pl011(facts["uart3"], keep4))
    return root


def write_tree(root, header, out_base):
    text = "/dts-v1/;\n\n" + "".join(f"// {h}\n" for h in header) + "\n" + "\n".join(render(root)) + "\n"
    with open(out_base + ".dts", "w") as f:
        f.write(text)
    r = subprocess.run(["dtc", "-I", "dts", "-O", "dtb", "-o", out_base + ".dtb", out_base + ".dts"],
                       capture_output=True, text=True)
    return r.returncode, r.stderr.strip()


def summary(dtb):
    with open(dtb, "rb") as f:
        root = dt_check.parse_fdt(f.read())
    nodes, ph = [], {}
    dt_check.walk(root, "", 2, 1, ph, nodes)
    ncpu = sum(1 for p, n, _, _ in nodes if p.startswith("/cpus/") and n["props"].get("device_type", b"") == b"cpu\0")
    nmem = sum(1 for _, n, _, _ in nodes if dt_check.strs(n["props"].get("device_type", b"")) == ["memory"])
    devs = [p for p, n, _, _ in nodes if "reg" in n["props"] and not p.startswith("/cpus")
            and "interrupt-controller" not in n["props"]
            and dt_check.strs(n["props"].get("device_type", b"")) != ["memory"]
            and dt_check.strs(n["props"].get("status", b"okay")) in ([], ["okay"], ["ok"])]
    return ncpu, nmem, devs


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("linux_dts_dir")
    ap.add_argument("grant")
    ap.add_argument("outdir")
    ap.add_argument("--seeds", action="store_true")
    a = ap.parse_args()
    b = load_board(a.linux_dts_dir)
    facts = check_board(b)
    with open(a.grant) as f:
        g = json.load(f)
    header = [
        "Partition A (Linux) of HAVEN on the Orange Pi 6 Plus (CIX P1). GENERATED, do not edit:",
        "docs/methodology/stage2/20261109-board-partition-tree/gen_partition_a.py",
        f"from Linux v7.3 sky1-xcp.dts sha256 {sha256(os.path.join(a.linux_dts_dir, BOARD_DTS))}",
        f"and sky1.dtsi sha256 {sha256(os.path.join(a.linux_dts_dir, SOC_DTSI))}",
        f"with grant {os.path.basename(a.grant)} sha256 {sha256(a.grant)}",
    ]
    jobs = [(None, os.path.join(a.outdir, "partition-a"))]
    if a.seeds:
        os.makedirs(os.path.join(a.outdir, "seeds"), exist_ok=True)
        jobs += [(s, os.path.join(a.outdir, "seeds", f"partition-a-{s}")) for s in SEEDS]
    status = 0
    for seed, base in jobs:
        h = header + ([f"SEEDED FAULT {seed} (README.md, Expectations): not a tree to boot"] if seed else [])
        rc, err = write_tree(build(b, facts, g, seed), h, base)
        ncpu, nmem, devs = summary(base + ".dtb") if rc == 0 else (0, 0, [])
        print(f"{os.path.relpath(base, a.outdir)}: dtc exit {rc}, warnings: {err or 'none'}; "
              f"{ncpu} cpu nodes, {nmem} memory node(s), devices with reg besides the GIC: {', '.join(devs) or 'none'}")
        status |= rc
    return 1 if status else 0


if __name__ == "__main__":
    sys.exit(main())
