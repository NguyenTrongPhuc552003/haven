# SPDX-License-Identifier: Apache-2.0
"""Tests of check-public-assets.py (docs/methodology/stage2/20261113-e7-releases).
Home paths are built at run time, so this file holds none."""
import gzip
import importlib.util
import io
import os
import tarfile
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("cpa", os.path.join(HERE, "check-public-assets.py"))
cpa = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cpa)

MAC_HOME = "/" + "Users" + "/alice/src"
CI_HOME = "/" + "home" + "/runner/work"


def tar_gz(path, members, uid=0, gid=0, uname="", gname=""):
    with tarfile.open(path, "w:gz") as tar:
        for name, data in members:
            info = tarfile.TarInfo(name)
            info.size, info.uid, info.gid, info.uname, info.gname = len(data), uid, gid, uname, gname
            tar.addfile(info, io.BytesIO(data))


class Check(unittest.TestCase):
    def run_on(self, files, deny=()):
        """check() on FILES ({name: bytes, or a tar member list})."""
        with tempfile.TemporaryDirectory() as d:
            for name, data in files.items():
                p = os.path.join(d, name)
                if name.endswith(".tar.gz"):
                    tar_gz(p, *data) if isinstance(data, tuple) else tar_gz(p, data)
                elif name.endswith(".gz"):
                    with gzip.open(p, "wb") as f:
                        f.write(data)
                else:
                    with open(p, "wb") as f:
                        f.write(data)
            return cpa.check([d], [w.encode() for w in deny])

    def test_clean(self):
        out = self.run_on({"b.tar.gz": [("b/try.sh", b"#!/bin/sh\n")], "SHA256SUMS": b"00  b.tar.gz\n"})
        self.assertEqual(out, {"fail": [], "note": []})

    def test_entry_owner(self):
        out = self.run_on({"b.tar.gz": ([("b/x", b"x")], 501, 20, "someone", "staff")})
        self.assertEqual(len(out["fail"]), 1)
        self.assertIn("owned by 501:20", out["fail"][0])

    def test_mac_home_in_member_and_gz(self):
        out = self.run_on({"b.tar.gz": [("b/log", MAC_HOME.encode())], "n.gz": MAC_HOME.encode()})
        self.assertEqual(len(out["fail"]), 2)

    def test_banner(self):
        ok = self.run_on({"k.log": b"Linux version 6.18.0 (haven@ci) #1"})
        bad = self.run_on({"k.log": b"Linux version 6.18.0 (bob@mac) #1"})
        self.assertEqual((len(ok["fail"]), len(bad["fail"])), (0, 1))

    def test_denied_word_not_printed(self):
        out = self.run_on({"b.tar.gz": [("b/x", b"built by Carol-Mac")]}, deny=("carol-mac",))
        self.assertEqual(len(out["fail"]), 1)
        self.assertNotIn("carol", out["fail"][0].lower())

    def test_denied_owner_name(self):
        out = self.run_on({"b.tar.gz": ([("b/x", b"x")], 0, 0, "carol", "")}, deny=("carol",))
        self.assertTrue(any("entry's name or owner" in f for f in out["fail"]))

    def test_ci_home_is_a_note(self):
        out = self.run_on({"b.log": CI_HOME.encode()})
        self.assertEqual((out["fail"], len(out["note"])), ([], 1))


if __name__ == "__main__":
    unittest.main()
