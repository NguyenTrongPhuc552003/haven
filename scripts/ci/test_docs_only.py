#!/usr/bin/env python3
"""docs_only.py: which pull requests may skip the build, boot and host tests."""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import docs_only  # noqa: E402


def never(_):
    return False


class DocsOnly(unittest.TestCase):
    def test_markdown_records_only(self):
        files = ["docs/methodology/stage2/20261128-readback-n3/README.md", "docs/architecture/multi-vcpu.md"]
        self.assertFalse(docs_only.code_changed(files, never))

    def test_any_other_file_runs_everything(self):
        for f in ["src/core/irq/lr.c", "docs/methodology/stage2/20261128-readback-n3/grant-n3.json",
                  "tools/campaign/cases/n3.json", ".github/workflows/ci.yml"]:
            self.assertTrue(docs_only.code_changed(["README.md", f], never), f)

    def test_a_markdown_file_a_script_reads_runs_everything(self):
        self.assertTrue(docs_only.code_changed(["docs/x/README.md"], lambda f: True))

    def test_nothing_changed_runs_everything(self):
        self.assertTrue(docs_only.code_changed([], never))

    def test_quoted_names(self):
        names = {"cix-p1-loading.md", "README.md", "scripts/release/try/README.md", "docs/safety/ASSUMPTIONS.md"}
        self.assertTrue(docs_only.named("docs/porting/cix-p1-loading.md", names))
        self.assertTrue(docs_only.named("README.md", names))
        self.assertTrue(docs_only.named("scripts/release/try/README.md", names))
        self.assertTrue(docs_only.named("docs/safety/ASSUMPTIONS.md", names))
        self.assertFalse(docs_only.named("docs/methodology/stage2/20261128-readback-n3/README.md", names))
        self.assertFalse(docs_only.named("docs/architecture/multi-vcpu.md", names))

    def test_the_repository_names_the_loading_study(self):
        self.assertTrue(docs_only.named_by_code("docs/porting/cix-p1-loading.md"))
        self.assertFalse(docs_only.named_by_code("docs/methodology/stage2/20261128-readback-n3/README.md"))

if __name__ == "__main__":
    unittest.main()
