"""README 语言版本和本地导航的轻量检查。"""

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
LOCALES = (
    "zh-TW", "en", "ja", "ko", "fr", "de", "es", "pt", "ru", "ar", "hi",
    "it", "nl", "tr", "vi", "th", "id", "ms", "pl",
)


class MultilingualReadmeTests(unittest.TestCase):
    def test_every_non_default_locale_has_a_readme(self):
        for locale in LOCALES:
            with self.subTest(locale=locale):
                self.assertTrue((ROOT / "docs" / "readme" / f"README.{locale}.md").is_file())

    def test_readme_navigation_targets_exist(self):
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        links = re.findall(r"\[[^\]]+\]\((docs/readme/README\.[^)]+\.md)\)", readme)
        self.assertEqual(set(links), {f"docs/readme/README.{locale}.md" for locale in LOCALES})
        for relpath in links:
            self.assertTrue((ROOT / relpath).is_file(), relpath)


if __name__ == "__main__":
    unittest.main()
