"""界面静态文案与语言方向的轻量回归测试。"""

import unittest

from cybercat_i18n import (
    CATALOGS,
    KEYS,
    LANGUAGE_NAMES,
    is_rtl,
    mood_text,
    state_text,
    translate,
)
from cybercat_bridge_config import BridgeConfigurator


class LocalizationTests(unittest.TestCase):
    def test_all_requested_locales_have_every_static_label(self):
        self.assertEqual(set(CATALOGS), set(LANGUAGE_NAMES))
        self.assertEqual(len(CATALOGS), 20)
        for catalog in CATALOGS.values():
            self.assertEqual(set(catalog), set(KEYS))

    def test_known_text_translates_and_unknown_user_text_is_preserved(self):
        self.assertEqual(translate("刷新端口", "en"), "Refresh ports")
        self.assertEqual(translate("刷新端口", "ja"), "ポートを更新")
        self.assertEqual(translate("APPLE-home", "ar"), "APPLE-home")

    def test_mood_and_status_text_follow_selected_locale(self):
        self.assertEqual(mood_text("idle_blue", "zh-CN"), "待机中")
        self.assertEqual(mood_text("idle_blue", "ar"), "استعداد")
        self.assertEqual(state_text("connected", "fr"), "Connecté")

    def test_arabic_is_the_only_rtl_language(self):
        self.assertTrue(is_rtl("ar"))
        self.assertFalse(is_rtl("fa"))
        self.assertFalse(is_rtl("en"))

    def test_center_anchor_is_not_treated_as_compass_letters(self):
        self.assertEqual(BridgeConfigurator._mirror_compass("center"), "center")
        self.assertEqual(BridgeConfigurator._mirror_compass("nw"), "ne")


if __name__ == "__main__":
    unittest.main()
