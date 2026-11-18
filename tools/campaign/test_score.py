#!/usr/bin/env python3
"""Row K5 of the campaign record: score.py on a synthetic set."""

import contextlib
import io
import json
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import score  # noqa: E402


def case(name, cls, source="written", equivalent=False, **verdicts):
    methods = {m: {"verdict": v} for m, v in verdicts.items()}
    return {"schema": score.CASE_SCHEMA, "case": name, "source": source, "class": cls,
            "equivalent": equivalent, "methods": methods}


class ClopperPearson(unittest.TestCase):
    def test_record_bounds(self):
        self.assertEqual(round(score.clopper_pearson(10, 10)[0], 3), 0.692)
        self.assertEqual(round(score.clopper_pearson(20, 20)[0], 3), 0.832)

    def test_known_values(self):
        lo, hi = score.clopper_pearson(5, 10)
        self.assertEqual((round(lo, 4), round(hi, 4)), (0.1871, 0.8129))
        lo, hi = score.clopper_pearson(0, 10)
        self.assertEqual((lo, round(hi, 4)), (0.0, 0.3085))

    def test_bad_counts(self):
        for k, n in ((1, 0), (-1, 5), (6, 5)):
            with self.assertRaises(ValueError):
                score.clopper_pearson(k, n)


class Synthetic(unittest.TestCase):
    def setUp(self):
        self.cases = [
            case("W1", "GIC-PRIO", M1="pass", M2="pass", M6="finding"),
            case("W2", "GIC-PRIO", M1="pass", M2="finding", M6="finding"),
            case("W3", "S2-EXTENT", M1="pass", M2="pass", M6="pass", M7="n/a"),
            case("C1", "CONTROL", M1="pass", M2="pass", M6="finding"),
            case("C2", "CONTROL", M1="pass", M2="pass", M6="pass"),
            case("R1", "GIC-PRIO", source="random", M1="pass", M2="error", M6="finding"),
            case("R2", "S2-EXTENT", source="random", equivalent=True, M1="pass", M6="pass"),
        ]
        self.expect = {"W1": {"M6": "finding"}, "W3": {"M6": "finding"}, "C2": {"M6": "pass"}}
        self.report = score.score(self.cases, self.expect)

    def test_detection_table(self):
        allp = self.report["by_source"]["all"]
        self.assertEqual(allp["cases"], 6)
        prio = allp["detection"]["GIC-PRIO"]
        self.assertEqual((prio["M6"]["detected"], prio["M6"]["cases"]), (3, 3))
        self.assertEqual((prio["M2"]["detected"], prio["M2"]["cases"]), (1, 2))
        self.assertEqual(prio["M3"]["cases"], 0)
        self.assertEqual(allp["detection"]["S2-EXTENT"]["M6"]["detected"], 0)
        self.assertEqual(self.report["by_source"]["written"]["detection"]["GIC-PRIO"]["M6"]["cases"], 2)

    def test_controls_and_only_readback(self):
        allp = self.report["by_source"]["all"]
        self.assertEqual(allp["controls"]["M6"], {"false_findings": 1, "controls": 2})
        self.assertEqual(allp["controls"]["M1"], {"false_findings": 0, "controls": 2})
        self.assertEqual(allp["only_readback"], ["R1", "W1"])

    def test_control_findings_listed(self):
        self.cases[3]["methods"]["M6"]["evidence"] = ["cortex-a72: CPU 1: assignment drift"]
        rep = score.score(self.cases)
        self.assertEqual(rep["by_source"]["all"]["control_findings"],
                         [{"case": "C1", "method": "M6", "evidence": ["cortex-a72: CPU 1: assignment drift"]}])
        self.assertIn("- finding on control C1 by M6: cortex-a72: CPU 1: assignment drift", score.markdown(rep))
        self.cases[3]["source"] = "heldout"
        self.assertEqual(score.score(self.cases)["by_source"]["all"]["control_findings"], [])

    def test_apart(self):
        self.assertEqual(self.report["equivalent"], ["R2"])
        self.assertEqual(self.report["not_scored"]["M2"], {"error": ["R1"]})
        self.assertIn("C1", self.report["not_scored"]["M7"]["n/a"])

    def test_disagreements(self):
        self.assertEqual(self.report["disagreements"],
                         [{"case": "W3", "method": "M6", "expected": "finding", "got": "pass"}])

    def test_duplicate_case(self):
        with self.assertRaises(ValueError):
            score.score(self.cases + [self.cases[0]])

    def test_cli(self):
        with tempfile.TemporaryDirectory() as d:
            paths = []
            for c in self.cases:
                paths.append(os.path.join(d, c["case"] + ".json"))
                with open(paths[-1], "w") as f:
                    json.dump(c, f)
            out = os.path.join(d, "report.json")
            printed = io.StringIO()
            with contextlib.redirect_stdout(printed):
                self.assertEqual(score.main(["--json", out] + paths), 0)
            self.assertIn("| 3/3 [0.292, 1.000] |", printed.getvalue())
            with open(out) as f:
                self.assertEqual(json.load(f)["disagreements"], [])
            bad = os.path.join(d, "bad.json")
            with open(bad, "w") as f:
                json.dump({"schema": "other"}, f)
            with self.assertRaises(ValueError):
                score.load_case(bad)


if __name__ == "__main__":
    unittest.main()
