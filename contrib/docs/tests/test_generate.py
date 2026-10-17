import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from urllib.parse import parse_qs, urlsplit


ROOT = Path(__file__).resolve().parents[3]


class GenerateDocsTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.upstream = self.root / "external/feather-docs/content/_guides"
        self.upstream.mkdir(parents=True)
        self.output = self.root / "src/assets/docs"
        self.output.mkdir(parents=True)
        shutil.copytree(ROOT / "contrib/docs/overrides", self.root / "contrib/docs/overrides")

    def write_upstream(self, name, body):
        (self.upstream / name).write_text(
            '---\ntitle: "Upstream guide"\nnav_title: "Upstream"\ncategory: "help"\n---\n'
            + body
        )

    def generate(self):
        result = subprocess.run(
            [sys.executable, "-B", str(ROOT / "contrib/docs/generate.py")],
            cwd=self.root, capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def assert_wownero_contacts(self, document):
        urls = [urlsplit(url) for url in re.findall(r"\]\(([^)]+)\)", document)]
        self.assertTrue(any(
            url.hostname == "matrix.to" and url.fragment == "/#wownero-gen:wowne.ro"
            for url in urls
        ), "Matrix must lead to Wownero's published room")
        self.assertTrue(any(
            url.hostname == "webchat.oftc.net"
            and "wownero" in parse_qs(url.query).get("channels", [])
            for url in urls
        ), "Webchat must join Wownero's IRC channel")
        self.assertTrue(any(
            url.hostname == "github.com" and url.path == "/wownero/wowlet/issues"
            for url in urls
        ), "Bug reports must go to the Wowlet tracker")
        self.assertTrue(any(
            url.scheme == "mailto" and url.path == "wownero@wownero.org"
            for url in urls
        ), "Email must reach the published Wownero contact")
        for url in urls:
            self.assertNotIn("feather", url.geturl().lower(), "Support must not route to Feather")

    def test_support_override_survives_regeneration(self):
        self.write_upstream("report_an_issue.md", "Upstream support contacts")
        self.write_upstream("accounts.md", "An upstream guide without an override")
        (self.output / "stale.md").write_text("Obsolete generated guide")
        for body in ("Upstream support contacts", "Changed upstream support contacts"):
            self.write_upstream("report_an_issue.md", body)
            self.generate()
            support = (self.output / "report_an_issue.md").read_text()
            self.assert_wownero_contacts(support)
            self.assertNotIn(body, support)
            self.assertRegex(support, r"(?m)^\[nav_title\]: # \(.+\)$")
            self.assertRegex(support, r"(?m)^\[category\]: # \(.+\)$")
            self.assertIn("An upstream guide without an override", (self.output / "accounts.md").read_text())
            self.assertFalse((self.output / "stale.md").exists())

    def test_readme_and_security_contacts_agree_with_bundled_help(self):
        readme = (ROOT / "README.md").read_text()
        support = readme.split("## Support\n", 1)[1].split("\n## ", 1)[0]
        self.assert_wownero_contacts(support)
        policy = (ROOT / "SECURITY.md").read_text()
        self.assertIn("mailto:wownero@wownero.org", policy)
        self.assertNotIn("featherwallet.org", policy)


if __name__ == "__main__":
    unittest.main()
