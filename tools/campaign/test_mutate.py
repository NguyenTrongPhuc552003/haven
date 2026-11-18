# SPDX-License-Identifier: Apache-2.0
"""Tests of mutate.py (docs/methodology/stage2/20261119-evaluation-campaign,
rows K1 and K2). K1 needs commit e47b722 in the repository (CI's quality
gate job fetches it); without it, K1 is skipped."""
import json
import os
import subprocess
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import mutate  # noqa: E402

REPO = os.path.join(HERE, "..", "..")
V3 = os.path.join(REPO, "docs", "methodology", "stage2", "20261102-a1-cases-v3", "heldout-random", "draw.json")
SAMPLE = """/* a comment: 0x1000 < 2 */
#include <stdint.h>
#define SIZE 0x2000
int f(int a, int b)
{
\tif (a < b && b != 3)
\t\tg(a | 4);
\th();
\treturn a & b;
}
"""


class Operators(unittest.TestCase):
    def test_sample(self):
        got = [m[5] for m in mutate.mutants_of("x.c", SAMPLE)]
        self.assertEqual(sorted(got), sorted([
            "constant 0x2000 -> 0x3000", "comparison < -> <=", "comparison != -> ==", "logic && -> ||",
            "constant 3 -> 4", "constant 4 -> 5", "bitwise | -> &", "delete call: g(a | 4);",
            "delete call: h();", "bitwise & -> |"]))

    def test_never_comments_or_includes(self):
        lines = {m[1] for m in mutate.mutants_of("x.c", SAMPLE)}
        self.assertNotIn(0, lines)  # the comment
        self.assertNotIn(1, lines)  # #include
        self.assertIn(2, lines)  # #define carries the memory map's values

    def test_return_is_not_deleted(self):
        self.assertFalse([m for m in mutate.mutants_of("x.c", SAMPLE) if "return" in m[5]])

    def test_apply_changes_one_place(self):
        mut = next(m for m in mutate.mutants_of("x.c", SAMPLE) if m[5] == "comparison < -> <=")
        lines = SAMPLE.split("\n")
        i, a, b, rep = mut[1], mut[2], mut[3], mut[4]
        self.assertEqual(lines[i][:a] + rep + lines[i][b:], "\tif (a <= b && b != 3)")


class Draw(unittest.TestCase):
    def test_same_seed_same_order(self):
        self.assertEqual(mutate.shuffled(500, "seed")[0], mutate.shuffled(500, "seed")[0])
        self.assertNotEqual(mutate.shuffled(500, "seed")[0], mutate.shuffled(500, "other")[0])

    def test_k1_reproduces_case_set_v3(self):
        with open(V3) as f:
            v3 = json.load(f)
        commit = v3["haven_commit"]
        if subprocess.run(["git", "-C", REPO, "cat-file", "-e", commit + "^{commit}"],
                          capture_output=True).returncode:
            self.skipTest(f"commit {commit[:7]} not in this clone (CI's quality gate job fetches it)")

        def read(path):
            return subprocess.run(["git", "-C", REPO, "show", f"{commit}:{path}"], capture_output=True,
                                  text=True, check=True).stdout
        allm = mutate.enumerate_all(REPO, mutate.FILES, read)
        order, seed = mutate.shuffled(len(allm), v3["seed_text"])
        self.assertEqual((len(allm), seed), (v3["mutants_total"], v3["seed_sha256"]))
        drawn = v3["kept"] + v3["skipped"]
        first = [mutate.entry(k, allm[k]) for k in order[:len(drawn)]]
        self.assertEqual(sorted(e["index"] for e in first), sorted(e["index"] for e in drawn))
        for e in drawn:
            self.assertIn({k: e[k] for k in ("index", "file", "line", "mutation")}, first)
        self.assertEqual([e["index"] for e in first if e["index"] in {d["index"] for d in v3["kept"]}],
                         [e["index"] for e in v3["kept"]])



class Taxonomy(unittest.TestCase):
    def test_classes_unique_and_files_exist(self):
        with open(os.path.join(HERE, "taxonomy.json")) as f:
            tax = json.load(f)
        ids = [c["id"] for c in tax["classes"]]
        self.assertEqual(len(ids), len(set(ids)))
        points = {c["point"] for c in tax["classes"]}
        self.assertTrue(points <= {"stage-2", "SMMU", "GIC", "CPU and EL2", "control"})
        for files in tax["files"].values():
            for path in files:
                self.assertTrue(os.path.isfile(os.path.join(REPO, path)), path)


if __name__ == "__main__":
    unittest.main()
