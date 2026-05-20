#!/usr/bin/env python3
from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import time


COMMANDS = [
    ["./scripts/build.sh"],
    ["./scripts/style-check.sh"],
    ["./scripts/test.sh"],
]


def run_timed(cmd: list[str]) -> dict[str, object]:
    start = time.perf_counter()
    result = subprocess.run(cmd, check=False, capture_output=True, text=True)
    elapsed = time.perf_counter() - start
    return {
        "command": " ".join(cmd),
        "exit_code": result.returncode,
        "elapsed_seconds": round(elapsed, 6),
        "stdout_tail": "\n".join(result.stdout.splitlines()[-15:]),
        "stderr_tail": "\n".join(result.stderr.splitlines()[-15:]),
    }


def main() -> int:
    report = {
        "timestamp_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "results": [],
        "status": "pass",
    }

    for cmd in COMMANDS:
        entry = run_timed(cmd)
        report["results"].append(entry)
        if entry["exit_code"] != 0:
            report["status"] = "fail"

    out_dir = pathlib.Path("build/benchmarks")
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / "baseline.json"
    out_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    print(f"[benchmark] wrote {out_path}")

    if report["status"] != "pass":
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
