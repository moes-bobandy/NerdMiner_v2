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
        self.assertIn("nerd_draw_int_live_pulse", disp)

    def test_sd_quiesces_ext_only(self) -> None:
        sd = read("src/drivers/storage/SDCard.cpp")
        self.assertIn("nerd_quiesce_ext()", sd)
        self.assertGreaterEqual(sd.count("nerd_quiesce_ext()"), 3)

    def test_keyboard_v12_and_gpio5_idle_high(self) -> None:
        kb = read("src/drivers/input/tca8418Keyboard.cpp")
        self.assertIn("key == ';'", kb)
        self.assertIn("key == 'p' || key == ','", kb)
        self.assertIn("KEY_BACKSPACE", kb)
        self.assertIn("KEY_DOWN", kb)
        self.assertIn("Fn+.", kb)
        self.assertIn("g_fn", kb)
        self.assertIn("(keycode - 1)", kb)
        self.assertIn("switchToNextScreen()", kb)
        self.assertIn("EXT_TFT_CS", kb)
        self.assertIn("digitalWrite(EXT_TFT_CS, HIGH)", kb)
        self.assertIn("0x34", read("src/drivers/devices/m5CardputerAdv.h"))
        self.assertIn("TCA8418_SDA_PIN", read("src/drivers/devices/m5CardputerAdv.h"))
        # Down = +1 (next), not an inverted prev delta.
        self.assertRegex(
            kb,
            r"key == KEY_DOWN[\s\S]{0,400}?switchToNextScreen\(\)",
        )
        down_block = kb.split("static void dispatchKey")[1].split("static void handleEvent")[0]
        self.assertLess(
            down_block.find("KEY_DOWN"),
            down_block.find("switchToNextScreen()"),
        )
        next_at = down_block.find("switchToNextScreen()")
        prev_at = down_block.find("switchToPrevScreen()")
        self.assertGreater(next_at, 0)
        self.assertGreater(prev_at, next_at)

    def test_adv_down_keycode_58_maps_to_key_down(self) -> None:
        """Host replica of TCA8418 7x8→4x14 remap (Dirt Adv FIFO: ↓=58)."""

        def map_raw(keycode: int):
            if keycode < 1:
                return None
            raw_row = (keycode - 1) // 10
            raw_col = (keycode - 1) % 10
            if raw_row > 6 or raw_col > 7:
                return None
            col = (raw_row * 2) + (1 if raw_col > 3 else 0)
            row = (raw_col + 4) % 4
            if row < 4 and col < 14:
                return row, col
            return None

        self.assertEqual(map_raw(58), (3, 11))  # printed ↓ / KEY_DOWN
        self.assertEqual(map_raw(57), (2, 11))  # ';' (contract next)
        self.assertEqual(map_raw(54), (3, 10))  # ','
        self.assertEqual(map_raw(64), (3, 12))  # '/'
        kb = read("src/drivers/input/tca8418Keyboard.cpp")
        self.assertIn("',', '.', '/', ' '", kb)
        self.assertIn("key == '.' || key == KEY_DOWN", kb)

    def test_ext_screens_do_not_fillscreen_every_tick(self) -> None:
        driver = read("src/drivers/displays/ili9341ExtDriver.cpp")
        self.assertIn("enterExtScreen", driver)
        self.assertIn("s_extScreen", driver)
        self.assertIn("tDisplayV1ComposeCyclic", driver)
        self.assertIn("blitLiveStock", driver)
        self.assertNotIn("C_ORANGE", driver)
        self.assertNotIn("0xFD20", driver)
        self.assertNotIn("static void fillContentBand", driver)
        self.assertNotIn("drawGoodsBand", driver)
        self.assertNotIn("dirtyUptimeTick", driver)
        self.assertNotIn("ili9341ExtDrawText", driver)
        self.assertNotIn("%luKH", driver)
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
        # v2.6: boot-only MV|BGR = 0x28 (no MX/MY/MH/ML). SW X+Y on push.
        self.assertIn(
            "#define EXT_TFT_MADCTL (ILI9341_MADCTL_MV | ILI9341_MADCTL_BGR)",
            low,
        )
        self.assertIn("#define EXT_TFT_SW_FLIP_Y 1", low)
        self.assertIn("#define EXT_TFT_SW_FLIP_X 1", low)
        self.assertIn("extFlipY", low)
        self.assertIn("extFlipX", low)
        self.assertIn("emitRgb565SwappedRev", low)
        self.assertEqual(low.count("writeCommand(ILI9341_MADCTL)"), 1)
        self.assertNotIn(
            "ILI9341_MADCTL_MX | ILI9341_MADCTL_MY | ILI9341_MADCTL_MV | ILI9341_MADCTL_BGR",
            low,
        )
        self.assertNotIn(
            "writeData(ILI9341_MADCTL_MX | ILI9341_MADCTL_MV | ILI9341_MADCTL_BGR)",
            low,
        )
        self.assertNotIn(
            "#define EXT_TFT_MADCTL (ILI9341_MADCTL_MY | ILI9341_MADCTL_MV | ILI9341_MADCTL_BGR)",
            low,
        )
        self.assertNotIn(
            "#define EXT_TFT_MADCTL (ILI9341_MADCTL_MV | ILI9341_MADCTL_ML | ILI9341_MADCTL_BGR)",
            low,
        )
        self.assertNotIn("static void blitStockOnce", driver)
        dual = read("src/drivers/displays/nerdMinerDual.cpp")
        self.assertIn("GRAM is retained", dual)
        self.assertIn("nerd_ext_begin_frame", dual)
        self.assertIn("cyclic_screens[screenIndex]", dual)
        self.assertIn("nav->cyclic_screens[screenIndex](e)", dual)
        self.assertIn("if (!s_intDirty && !viewChanged && !liveTick)", dual)
        self.assertIn("s_lastElapsed", dual)
        self.assertNotIn("nav->cyclic_screens[screenIndex](0)", dual)
        self.assertIn("DigitalNumbers", read("src/drivers/displays/tDisplayV1Driver.cpp"))
        self.assertIn("0xDEDB", read("src/drivers/displays/tDisplayV1Driver.cpp"))
        self.assertIn("tDisplayV1ComposeCyclic", read("src/drivers/displays/tDisplayV1Driver.cpp"))
        self.assertIn("s_skipIntPush", read("src/drivers/displays/tDisplayV1Driver.cpp"))
        self.assertNotIn("fillSprite(", dual)
        self.assertNotIn("images_240_135.h", dual)
        self.assertNotIn("getMiningData", dual)
        self.assertNotIn("HASHING", dual)
        v1 = read("src/drivers/displays/tDisplayV1Driver.cpp")
        self.assertIn("tDisplayV1StockFrame", v1)
        self.assertIn("tDisplayV1PushStockChrome", v1)
        self.assertIn("MinerScreen", v1)
        self.assertIn("minerClockScreen", v1.split("tDisplayV1StockFrame")[1].split("CyclicScreenFunction")[0])
        self.assertIn("s_intDirty", dual)
        self.assertIn("nerd_draw_int_live_pulse", dual)
        self.assertIn("tDisplayV1PaintLivePulse", dual)
        self.assertNotIn("%luKH", v1)
        self.assertNotIn("drawString(buf, 2, 126", v1)
        self.assertNotIn("NAV  INT ST7789", dual)
        self.assertIn("ili9341ExtPushImageScaled", low)
        disp = read("src/drivers/displays/display.cpp")
        self.assertIn("nerd_ext_begin_frame()", disp)
        self.assertIn("nerd_ext_end_frame()", disp)
        self.assertIn("nerd_mark_int_nav_dirty()", disp)
        self.assertIn("nerd_poll_int_nav()", disp)
        # INT menu before EXT mining paint (lag fix). Live mElapsed, not 0.
        self.assertLess(
            disp.find("nerd_draw_int_nav_hud(idx, mElapsed)"),
            disp.find("nerd_ext_begin_frame()"),
        )
        self.assertNotIn("nerd_draw_int_nav_hud(idx, 0)", disp)
        mon = read("src/monitor.cpp")
        self.assertIn("if (mElapsed == 0)", mon)
        self.assertIn("s_lastHashRate", mon)
        self.assertIn("for (int16_t dx = (int16_t)(dw - 1); dx >= 0; --dx)", low)
        self.assertIn("columns right→left", low)
        self.assertNotIn("tDisplayV1ComposeCyclic(screenIndex, 0, false)", driver)
        # Loading version stays inside 240x135 (not T-Display y=147).
        self.assertIn("HEIGHT - 16", v1)

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
        self.assertIn("0x28", docs)
        self.assertIn("MV|BGR", docs)
        self.assertIn("0xE8", docs)
        self.assertIn("0xA8", docs)
        self.assertIn("0x68", docs)
        self.assertIn("0x38", docs)
        self.assertIn("MX|MY|MV|BGR", docs)
        self.assertIn("SW_FLIP_Y", docs)
        self.assertIn("SW_FLIP_X", docs)
        self.assertIn("BLOCK TEMPLATES", docs)
        self.assertIn("v2.6", docs)
        self.assertIn("v2.5", docs)
        self.assertIn("v2.2", docs)
        self.assertIn("v2.2b", docs)
        self.assertIn("v2.1b", docs)
        self.assertIn("DigitalNumbers", docs)
        self.assertIn("C_ORANGE", docs)
        self.assertIn("not identical clones", docs)
        self.assertIn("nav/status only", docs)
        self.assertIn("Stock V1 chrome only", docs)
        self.assertIn("Portal must not be skipped", docs)
        self.assertIn("drain-while-held", docs)
        launcher = read("docs/cardputer-adv-launcher.md")
        self.assertIn("M5-Cardputer-Adv-dual", launcher)
        self.assertIn("NerdMinerAP", launcher)
        self.assertIn("hold Enter, C, or W", launcher)
        self.assertIn("G0 (BOOT) held during the loading splash", launcher)

    def test_sw_xy_flip_window_geometry(self) -> None:
        """Host replica of extFlipX/extFlipY + row reverse (v2.6)."""
        width, height = 320, 240

        def flip_x(x: int, w: int) -> int:
            return width - x - w

        def flip_y(y: int, h: int) -> int:
            return height - y - h

        # Window X is not remapped (push reverses pixels in-row).
        self.assertEqual(flip_x(0, 320), 0)
        self.assertEqual(flip_y(0, 240), 0)
        self.assertEqual(flip_y(30, 180), 30)  # centered 320x180 art
        row = list(range(320))
        rev = list(reversed(row))
        self.assertEqual(rev[0], 319)
        self.assertEqual(rev[-1], 0)

    def test_boot_portal_drain_while_held_not_flush(self) -> None:
        """HWbot: flushFifo() then 30ms drain wipes a boot-held Enter."""
        kb = read("src/drivers/input/tca8418Keyboard.cpp")
        self.assertIn("drainFifoForHeldConfig", kb)
        self.assertIn("cardputerKeyboardPollConfigHeld", kb)
        self.assertIn("g0Held", kb)
        self.assertIn("PIN_BUTTON_1", kb)
        self.assertIn("isConfigKey", kb)
        self.assertIn("Do NOT flushFifo()", kb)
        self.assertNotIn("flushFifo();", kb)
        self.assertIn("g_wantsConfig = true", kb)
        ino = read("src/NerdMinerV2.ino.cpp")
        self.assertIn("cardputerKeyboardPollConfigHeld()", ino)
        self.assertIn("init_WifiManager()", ino)
        self.assertLess(ino.find("initDisplay()"), ino.find("init_WifiManager()"))
        self.assertLess(ino.find("cardputerKeyboardPollConfigHeld()"), ino.find("init_WifiManager()"))
        wm = read("src/wManager.cpp")
        self.assertIn("!forceConfig && SDCrd.loadConfigFile", wm)
        self.assertIn("armForcePortal", wm)
        self.assertIn("consumeForcePortal", wm)
        self.assertIn("WiFi.disconnect(true, true)", wm)
        self.assertIn("startConfigPortal", wm)
        nv = read("src/drivers/storage/nvMemory.cpp")
        self.assertIn("/force_portal", nv)
        nv_del = nv.split("bool nvMemory::deleteConfig")[1].split("bool nvMemory::armForcePortal")[0]
        self.assertIn("if (!init())", nv_del)
        # No INT menu list in this PR.
        dual = read("src/drivers/displays/nerdMinerDual.cpp")
        self.assertNotIn("INT menu", dual)
        self.assertNotIn("menu list", dual.lower())

        # Host replica: a press already in the FIFO is lost if flushed first.
        def flush_then_drain(fifo):
            fifo.clear()
            return any((ev & 0x80) for ev in fifo)

        def drain_while_held(fifo):
            wants = False
            for ev in fifo:
                if ev & 0x80:
                    wants = True
            return wants

        flushed = [0x80 | 1]
        self.assertFalse(flush_then_drain(flushed))
        self.assertTrue(drain_while_held([0x80 | 1]))

    def test_portal_contract_v11_save_no_splash_bounce(self) -> None:
        """Contract v1.1: /sta_first after save; splash ignores keys; STA UI first."""
        wm = read("src/wManager.cpp")
        kb = read("src/drivers/input/tca8418Keyboard.cpp")
        kbh = read("src/drivers/input/tca8418Keyboard.h")
        disp = read("src/drivers/displays/display.cpp")
        ino = read("src/NerdMinerV2.ino.cpp")
        nv = read("src/drivers/storage/nvMemory.cpp")
        nvh = read("src/drivers/storage/nvMemory.h")

        # Config QR = setupModeScreen via nerd_nav; splash = initScreen loading.
        self.assertIn("nerd_nav()->setupScreen()", disp)
        self.assertIn("nerd_nav()->loadingScreen()", disp)
        v1 = read("src/drivers/displays/tDisplayV1Driver.cpp")
        self.assertIn("setupModeScreen", v1.split("tDisplay_SetupScreen")[1].split("tDisplay_AnimateCurrentScreen")[0])
        self.assertIn("initScreen", v1.split("tDisplay_LoadingScreen")[1].split("tDisplay_SetupScreen")[0])
        dual_h = read("src/drivers/displays/nerdMinerDual.h")
        self.assertIn("Loading / setup / portal stay on INT", dual_h)

        self.assertIn("cardputerKeyboardClearConfigLatch", kbh)
        self.assertIn("cardputerKeyboardIgnoreForcePortal", kbh)
        self.assertIn("cardputerKeyboardIgnoreForcePortal", wm)
        self.assertIn("cardputerKeyboardIgnoreForcePortal", ino)
        self.assertIn("g_ignoreForcePortal", kb)
        self.assertIn("/sta_first", nv)
        self.assertIn("armStaFirst", nvh)
        self.assertIn("peekStaFirst", nvh)
        self.assertIn("consumeStaFirst", nvh)
        self.assertIn("armStaFirst", wm)
        self.assertIn("consumeStaFirst", wm)
        self.assertIn("peekStaFirst", ino)
        self.assertIn("commitPortalSave", wm)
        self.assertIn("setEnableConfigPortal(false)", wm)
        self.assertIn("NM_Connecting", wm)
        self.assertIn("No setup QR until a real STA timeout", wm)

        # Wipe still arms /force_portal; save arms /sta_first; connect-fail does not arm wipe.
        reset_fn = wm.split("void reset_configuration()")[1].split("void init_WifiManager()")[0]
        self.assertIn("armForcePortal", reset_fn)
        self.assertIn("consumeStaFirst", reset_fn)
        self.assertIn("ESP.restart()", reset_fn)
        init_fn = wm.split("void init_WifiManager()")[1]
        self.assertNotIn("armForcePortal", init_fn)
        self.assertIn("armStaFirst", init_fn)
        commit = init_fn.split("auto commitPortalSave")[1].split("auto runConfigPortal")[0]
        self.assertIn("armStaFirst", commit)
        self.assertIn("cardputerKeyboardClearConfigLatch", commit)
        self.assertIn("ESP.restart()", commit)
        self.assertNotIn("armForcePortal", commit)

        sta_path = init_fn.split("STA first:")[1].split("//Conectado a la red Wifi")[0]
        self.assertIn("NM_Connecting", sta_path)
        self.assertIn("setEnableConfigPortal(false)", sta_path)
        self.assertNotIn("drawSetupScreen()", sta_path.split("if (!wm.autoConnect")[0])
        self.assertIn("runConfigPortal", sta_path)
        self.assertLess(sta_path.find("autoConnect"), sta_path.find("runConfigPortal"))

        # Keyboard force-portal is skipped when /sta_first or wipe consumed first.
        self.assertIn("if (!wipePortal && !staFirst)", init_fn)
        self.assertIn("consumeForcePortal", wm)
        self.assertIn("/force_portal", nv)

        poll = kb.split("bool cardputerKeyboardPollConfigHeld()")[1].split("void cardputerKeyboardClearConfigLatch()")[0]
        self.assertIn("g_ignoreForcePortal", poll)
        self.assertIn("return false", poll)
        self.assertIn("return physical", poll)
        self.assertIn("peekStaFirst()", ino)
        self.assertLess(ino.find("drawLoadingScreen()"), ino.find("init_WifiManager()"))
        self.assertLess(ino.find("peekStaFirst()"), ino.find("init_WifiManager()"))

        # Host replica: /sta_first present → ignore keys; wipe still forces portal.
        def decide(sta_first, wipe, key_held):
            if wipe:
                return "portal"
            if sta_first:
                return "sta"
            if key_held:
                return "portal"
            return "sta"

        self.assertEqual(decide(True, False, True), "sta")
        self.assertEqual(decide(True, False, False), "sta")
        self.assertEqual(decide(False, True, False), "portal")
        self.assertEqual(decide(False, False, True), "portal")
        self.assertEqual(decide(True, True, True), "portal")  # wipe wins

        docs = read("docs/cardputer-adv-dual-screen.md")
        self.assertIn("/sta_first", docs)
        self.assertIn("autoConnect", docs)
        self.assertIn("/force_portal", docs)

    def test_orientation_locks_untouched(self) -> None:
        low = read("src/drivers/displays/ili9341Ext.cpp")
        self.assertIn("#define EXT_TFT_MADCTL (ILI9341_MADCTL_MV | ILI9341_MADCTL_BGR)", low)
        self.assertIn("#define EXT_TFT_SW_FLIP_Y 1", low)
        self.assertIn("#define EXT_TFT_SW_FLIP_X 1", low)
        driver = read("src/drivers/displays/ili9341ExtDriver.cpp")
        self.assertIn("blitLiveStock", driver)
        self.assertNotIn("Wifi 0kH", driver)
        self.assertNotIn("%luKH", driver)


if __name__ == "__main__":
    unittest.main()
