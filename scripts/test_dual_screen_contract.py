#!/usr/bin/env python3
"""Host-side Dirt dual-routing contract v1 checks (no hardware required)."""

from __future__ import annotations

import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]


def read(rel: str) -> str:
    return (REPO / rel).read_text(encoding="utf-8", errors="replace")


class DualScreenContractTests(unittest.TestCase):
    def test_platformio_dual_env_flag_and_stock_untouched(self) -> None:
        ini = read("platformio.ini")
        self.assertIn("[env:M5-Cardputer-Adv]", ini)
        self.assertIn("[env:M5-Cardputer-Adv-dual]", ini)
        self.assertIn("M5-Cardputer-Adv-dual", ini.split("default_envs", 1)[1][:500])

        stock = ini.split("[env:M5-Cardputer-Adv]")[1].split("[env:")[0]
        self.assertNotIn("NERDMINER_DUAL_SCREEN", stock)
        self.assertIn("-D M5_CARDPUTER_ADV=1", stock)

        dual = ini.split("[env:M5-Cardputer-Adv-dual]")[1].split("[env:")[0]
        self.assertIn("-D NERDMINER_DUAL_SCREEN=1", dual)
        self.assertIn("-D M5_CARDPUTER_ADV=1", dual)

    def test_ext_and_sd_and_int_pin_porkchop(self) -> None:
        adv = read("src/drivers/devices/m5CardputerAdv.h")
        self.assertIn("#define EXT_TFT_CS    5", adv)
        self.assertIn("#define EXT_TFT_DC    6", adv)
        self.assertIn("#define EXT_TFT_RST   3", adv)
        self.assertIn("#define SDSPI_CS    12", adv)
        self.assertIn("#define SDSPI_MOSI  14", adv)
        self.assertIn("#define SDSPI_CLK   40", adv)
        self.assertIn("#define SDSPI_MISO  39", adv)
        self.assertIn("mutually exclusive", adv.lower())

        setup = read("lib/TFT_eSPI/User_Setups/Setup215_M5_Cardputer_Adv.h")
        self.assertIn("#define TFT_MOSI   35", setup)
        self.assertIn("#define TFT_SCLK   36", setup)
        self.assertIn("#define TFT_CS     37", setup)
        self.assertIn("#define TFT_DC     34", setup)
        self.assertIn("#define TFT_RST    33", setup)
        self.assertIn("#define TFT_BL     38", setup)

    def test_routing_apis_exist(self) -> None:
        header = read("src/drivers/displays/nerdMinerDual.h")
        self.assertIn("nerd_nav()", header)
        self.assertIn("nerd_mining()", header)
        self.assertIn("nerd_quiesce_ext()", header)
        disp = read("src/drivers/displays/display.cpp")
        self.assertIn("nerd_dual_init()", disp)
        self.assertIn("nerd_mining()", disp)
        self.assertIn("nerd_nav()", disp)
        self.assertIn("nerd_draw_int_nav_hud", disp)

    def test_sd_quiesces_ext_only(self) -> None:
        sd = read("src/drivers/storage/SDCard.cpp")
        self.assertIn("nerd_quiesce_ext()", sd)
        self.assertGreaterEqual(sd.count("nerd_quiesce_ext()"), 3)

    def test_keyboard_v12_and_gpio5_idle_high(self) -> None:
        kb = read("src/drivers/input/tca8418Keyboard.cpp")
        self.assertIn("key == ';'", kb)
        self.assertIn("key == 'p' || key == ','", kb)
        self.assertIn("KEY_BACKSPACE", kb)
        self.assertIn("EXT_TFT_CS", kb)
        self.assertIn("digitalWrite(EXT_TFT_CS, HIGH)", kb)
        self.assertIn("0x34", read("src/drivers/devices/m5CardputerAdv.h"))

    def test_ext_screens_do_not_fillscreen_every_tick(self) -> None:
        driver = read("src/drivers/displays/ili9341ExtDriver.cpp")
        self.assertIn("enterExtScreen", driver)
        self.assertIn("s_extScreen", driver)
        self.assertIn("takeField", driver)
        self.assertIn("dirty7Seg", driver)
        self.assertIn("dirtyStat", driver)
        self.assertNotIn("static void fillContentBand", driver)
        # Full clear lives only in enterExtScreen (screen-index change).
        self.assertEqual(driver.count("ili9341ExtFillScreen("), 1)
        for name in (
            "extMinerScreen",
            "extClockScreen",
            "extGlobalScreen",
            "extPriceScreen",
        ):
            body = driver.split(f"static void {name}", 1)[1].split("static void ", 1)[0]
            self.assertIn("enterExtScreen(", body)
            self.assertNotIn("ili9341ExtFillScreen", body)

        low = read("src/drivers/displays/ili9341Ext.cpp")
        self.assertIn("writeBytes", low)
        self.assertIn("extSpi.begin(EXT_TFT_SCK, EXT_TFT_MISO, EXT_TFT_MOSI, -1)", low)
        self.assertIn("sdCsIdle", low)
        self.assertNotIn("while (count--)", low)
        self.assertIn("ili9341ExtBeginFrame", low)
        self.assertIn("while (s_frame > 0)", low)
        dual = read("src/drivers/displays/nerdMinerDual.cpp")
        self.assertIn("GRAM is retained", dual)
        self.assertIn("nerd_ext_begin_frame", dual)
        disp = read("src/drivers/displays/display.cpp")
        self.assertIn("nerd_ext_begin_frame()", disp)
        self.assertIn("nerd_ext_end_frame()", disp)

    def test_docs_and_launcher_recipe(self) -> None:
        docs = read("docs/cardputer-adv-dual-screen.md")
        self.assertIn("nerd_nav()", docs)
        self.assertIn("nerd_mining()", docs)
        self.assertIn("0xE9", docs)
        self.assertIn("M5-Cardputer-Adv-dual", docs)
        self.assertIn("factory", docs.lower())
        self.assertTrue(re.search(r"LoRa|Hydra", docs))
        self.assertIn("320", docs)
        self.assertIn("wipe", docs.lower())
        self.assertIn("Field retest", docs)
        launcher = read("docs/cardputer-adv-launcher.md")
        self.assertIn("M5-Cardputer-Adv-dual", launcher)


if __name__ == "__main__":
    unittest.main()
