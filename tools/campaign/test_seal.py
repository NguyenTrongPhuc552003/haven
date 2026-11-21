#!/usr/bin/env python3
"""seal.py and unseal.py (row K7a of the campaign's step C3)."""

import contextlib
import gzip
import hashlib
import io
import json
import os
import sys
import tarfile
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import seal  # noqa: E402
import unseal  # noqa: E402

SECRET_TEXT = "secret_fault_text_for_the_test"
PATCH = f"--- a/x.c\n+++ b/x.c\n@@ -1 +1 @@\n-int a;\n+int {SECRET_TEXT};\n"


def owner_dir(root, n=2, cls="S2-PERM", expect=None):
    d = os.path.join(root, "heldout")
    os.makedirs(d)
    for i in range(1, n + 1):
        with open(os.path.join(d, f"H{i}.patch"), "w") as f:
            f.write(PATCH)
        meta = {"class": cls}
        if expect:
            meta["expect"] = expect
        with open(os.path.join(d, f"H{i}.json"), "w") as f:
            json.dump(meta, f)
    return d


def raw_archive(entries):
    """A gzip tar from (TarInfo, bytes) pairs, for members seal.py would never write."""
    raw = io.BytesIO()
    with tarfile.open(fileobj=raw, mode="w") as t:
        for info, data in entries:
            t.addfile(info, io.BytesIO(data) if data is not None else None)
    return gzip.compress(raw.getvalue(), mtime=0)


def member(name, data=b"x", kind=tarfile.REGTYPE, link=""):
    info = tarfile.TarInfo(name)
    info.type, info.linkname = kind, link
    info.size = len(data) if kind == tarfile.REGTYPE else 0
    return info, (data if kind == tarfile.REGTYPE else None)


class Seal(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = self.tmp.name

    def tearDown(self):
        self.tmp.cleanup()

    def test_reproducible(self):
        d = owner_dir(self.root)
        self.assertEqual(seal.collect(d)[1], seal.collect(d)[1])

    def test_cases_document(self):
        doc, _ = seal.collect(owner_dir(self.root, n=3, expect={"M6": "finding"}))
        self.assertEqual([c["case"] for c in doc["cases"]], ["H1", "H2", "H3"])
        self.assertTrue(all(c["source"] == "heldout" for c in doc["cases"]))
        self.assertEqual(doc["expect"]["H2"], {"M6": "finding"})

    def test_refusals(self):
        with self.assertRaises(seal.SealError):
            seal.collect(owner_dir(self.root, cls="NOT-A-CLASS"))
        with self.assertRaises(seal.SealError):
            seal.collect(owner_dir(os.path.join(self.root, "b"), expect={"M9": "pass"}))
        doc = {"schema": seal.SCHEMA, "cases": [{"case": "H1", "class": "S2-PERM", "source": "written",
                                                 "patch": "H1.patch"}]}
        with self.assertRaises(seal.SealError):
            seal.check_cases(doc, {"H1.patch"})
        doc["cases"][0]["source"] = "heldout"
        seal.check_cases(doc, {"H1.patch"})
        with self.assertRaises(seal.SealError):
            seal.check_cases(doc, set())

    def test_prints_no_name_or_text(self):
        d = owner_dir(self.root)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(seal.main([d]), 0)
        text = out.getvalue()
        self.assertIn("H1 S2-PERM", text)
        self.assertNotIn("H1.patch", text)
        self.assertNotIn(SECRET_TEXT, text)
        self.assertTrue(os.path.exists(d + ".b64") and os.path.exists(d + ".tar.gz"))


class Unseal(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = self.tmp.name
        d = owner_dir(self.root)
        with contextlib.redirect_stdout(io.StringIO()):
            seal.main([d])
        self.b64 = d + ".b64"
        with open(d + ".tar.gz", "rb") as f:
            self.archive = f.read()
        self.digest = hashlib.sha256(self.archive).hexdigest()
        self.hashfile = os.path.join(self.root, "HELDOUT-SHA256")
        with open(self.hashfile, "w") as f:
            f.write(f"{self.digest}  2\n")
        self.out = os.path.join(self.root, "sealed")

    def tearDown(self):
        self.tmp.cleanup()

    def run_main(self, *args):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            rc = unseal.main(list(args))
        return rc, out.getvalue() + err.getvalue()

    def test_right_hash(self):
        rc, text = self.run_main(self.hashfile, self.out, "--from-file", self.b64)
        self.assertEqual(rc, 0)
        self.assertEqual(text.strip(), "sealed archive: hash matches, 2 cases")
        with open(os.path.join(self.out, "cases.json")) as f:
            doc = json.load(f)
        with open(doc["cases"][0]["patch"]) as f:
            self.assertEqual(f.read(), PATCH)

    def test_from_env(self):
        with open(self.b64) as f:
            os.environ["K7A_TEST_ARCHIVE"] = f.read()
        try:
            rc, _ = self.run_main(self.hashfile, self.out, "--from-env", "K7A_TEST_ARCHIVE")
        finally:
            del os.environ["K7A_TEST_ARCHIVE"]
        self.assertEqual(rc, 0)
        rc, text = self.run_main(self.hashfile, self.out + "2", "--from-env", "K7A_UNSET_VARIABLE")
        self.assertEqual(rc, 1)
        self.assertIn("not set", text)

    def test_wrong_hash_writes_nothing(self):
        with open(self.hashfile, "w") as f:
            f.write(("0" if self.digest[0] != "0" else "1") + self.digest[1:] + "  2\n")
        rc, text = self.run_main(self.hashfile, self.out, "--from-file", self.b64)
        self.assertEqual(rc, 1)
        self.assertIn("hash mismatch", text)
        self.assertFalse(os.path.exists(self.out))
        self.assertNotIn(SECRET_TEXT, text)
        self.assertNotIn("H1", text)

    def test_wrong_count(self):
        with open(self.hashfile, "w") as f:
            f.write(f"{self.digest}  3\n")
        self.assertEqual(self.run_main(self.hashfile, self.out, "--from-file", self.b64)[0], 1)
        self.assertFalse(os.path.exists(self.out))

    def test_not_base64(self):
        bad = os.path.join(self.root, "bad.b64")
        with open(bad, "w") as f:
            f.write("not base64 at all!\n")
        rc, text = self.run_main(self.hashfile, self.out, "--from-file", bad)
        self.assertEqual(rc, 1)
        self.assertIn("not base64", text)

    def test_members_refused(self):
        cases = json.dumps({"schema": seal.SCHEMA, "cases": [
            {"case": "H1", "class": "S2-PERM", "source": "heldout", "patch": "H1.patch"}]}).encode()
        for bad in (member("../H1.patch"), member("/tmp/H1.patch"), member("sub/H1.patch"),
                    member("H1.patch", kind=tarfile.SYMTYPE, link="/etc/passwd"),
                    member("H1.patch", kind=tarfile.DIRTYPE)):
            data = raw_archive([member("cases.json", cases), bad])
            with self.assertRaises(seal.SealError):
                unseal.open_archive(data, hashlib.sha256(data).hexdigest())
        ok = raw_archive([member("cases.json", cases), member("H1.patch", PATCH.encode())])
        doc, members = unseal.open_archive(ok, hashlib.sha256(ok).hexdigest(), 1)
        self.assertEqual(list(members), ["H1.patch"])


if __name__ == "__main__":
    unittest.main()
