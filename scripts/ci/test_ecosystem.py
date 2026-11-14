# SPDX-License-Identifier: Apache-2.0
"""Tests of ecosystem.py check-workflow (docs/methodology/stage2/
20261114-console-drain, row C6). Standard library only."""
import contextlib
import glob
import io
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ecosystem  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
LOCK = {"kdal": {"commit": "0123456789abcdef0123456789abcdef01234567"},
        "elmos_sdk": {"tag": "linux-9.9-1", "digest": "sha256:" + "fedcba98" * 8,
                      "elmos_commit": "89abcdef0123456789abcdef0123456789abcdef"},
        "ci": {"runner": "ubuntu-24.04"}}
GOOD = """name: x
on: push
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - run: true
  # a comment between jobs
  boot:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - uses: ./.github/actions/linux-partition
"""


class CheckWorkflow(unittest.TestCase):
    def run_check(self, *texts, lock=LOCK):
        """True if check-workflow passes on files holding TEXTS."""
        with tempfile.TemporaryDirectory() as d:
            paths = []
            for i, text in enumerate(texts):
                paths.append(os.path.join(d, f"w{i}.yml"))
                with open(paths[-1], "w") as f:
                    f.write(text)
            with contextlib.redirect_stdout(io.StringIO()):
                try:
                    ecosystem.check_workflow(lock, paths)
                except SystemExit:
                    return False
        return True

    def test_jobs(self):
        self.assertEqual([name for name, _ in ecosystem.jobs(GOOD)], ["build", "boot"])

    def test_good(self):
        self.assertTrue(self.run_check(GOOD))

    def test_literal_in_any_file(self):
        for literal in (LOCK["kdal"]["commit"], LOCK["kdal"]["commit"][:7], LOCK["elmos_sdk"]["tag"],
                        "fedcba9", LOCK["elmos_sdk"]["elmos_commit"][:7]):
            self.assertFalse(self.run_check(GOOD, f"# ref: {literal}\n"), literal)

    def test_other_runner(self):
        self.assertFalse(self.run_check(GOOD.replace("runs-on: ubuntu-24.04", "runs-on: ubuntu-26.04")))

    def test_runner_of_every_user(self):
        other = GOOD.replace("runs-on: ubuntu-24.04", "runs-on: ${{ matrix.os }}")
        self.assertFalse(self.run_check(GOOD, other))

    def test_no_user(self):
        self.assertFalse(self.run_check(GOOD.replace("./.github/actions/linux-partition", "./x")))

    def test_real_files(self):
        files = glob.glob(os.path.join(ROOT, ".github", "workflows", "*.yml"))
        files += glob.glob(os.path.join(ROOT, ".github", "actions", "*", "action.yml"))
        with contextlib.redirect_stdout(io.StringIO()):
            ecosystem.check_workflow(ecosystem.lock(), files)


if __name__ == "__main__":
    unittest.main()
