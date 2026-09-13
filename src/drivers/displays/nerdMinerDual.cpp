#include "display.h"

#ifdef NERDMINER_DUAL_SCREEN

#include "nerdMinerDual.h"
#include "ili9341Ext.h"
#include <TFT_eSPI.h>
#include "drivers/devices/device.h"

#if TFT_MOSI != 35 || TFT_SCLK != 36 || TFT_CS != 37 || TFT_DC != 34 || TFT_RST != 33 || TFT_BL != 38
#error "Dirt contract v1: INT ST7789 must stay Setup215 MOSI35 SCLK36 CS37 DC34 RST33 BL38"
#endif

extern DisplayDriver tDisplayV1Driver;
extern DisplayDriver ili9341ExtDriver;

static bool g_ext = false;

void nerd_quiesce_ext()
{
    // GPIO5 HIGH only, and only between complete EXT frames.
    // ILI9341 GRAM is retained; this must not blank the panel.
    ili9341ExtQuiesce();
    pinMode(EXT_TFT_CS, OUTPUT);
    digitalWrite(EXT_TFT_CS, HIGH);
}

void nerd_ext_begin_frame()
{
    ili9341ExtBeginFrame();
}

void nerd_ext_end_frame()
{
    ili9341ExtEndFrame();
}

DisplayDriver *nerd_nav()
{
    return &tDisplayV1Driver;
}

DisplayDriver *nerd_mining()
{
    return g_ext ? &ili9341ExtDriver : &tDisplayV1Driver;
}

bool nerd_ext_available()
{
    return g_ext;
}

void nerd_dual_init()
{
    nerd_quiesce_ext();
    Serial.println(F("Dual screen: INT=nav ST7789  Setup215 / tDisplayV1"));
    tDisplayV1Driver.initDisplay();
    currentDisplayDriver = &tDisplayV1Driver;

    g_ext = ili9341ExtBegin();
    if (g_ext) {
        Serial.println(F("Dual screen: EXT=mining ILI9341 320x240"));
    } else {
        Serial.println(F("Dual screen: EXT fail — INT-only fallback"));
    }
    nerd_quiesce_ext();
}

static volatile bool s_intDirty = true;
static int s_intDrawn = -1;

void nerd_mark_int_nav_dirty()
{
    s_intDirty = true;
}

void nerd_poll_int_nav()
{
    if (!g_ext || !s_intDirty) {
        return;
    }
    nerd_draw_int_nav_hud(nerd_nav()->current_cyclic_screen, 0);
}

void nerd_draw_int_nav_hud(int screenIndex, unsigned long mElapsed)
{
    DisplayDriver *nav = nerd_nav();
    if (!nav || nav->num_cyclic_screens <= 0) {
        return;
    }
    if (screenIndex < 0 || screenIndex >= nav->num_cyclic_screens) {
        screenIndex = 0;
    }

    // ARCH v2.1b addendum: same stock V1 scheme, info split.
    // INT = nav/status only. EXT = mining goods.
    const bool viewChanged = (screenIndex != s_intDrawn);
    if (!s_intDirty && !viewChanged && mElapsed == 0) {
        return;
    }

    nav->cyclic_screens[screenIndex](mElapsed);
    s_intDrawn = screenIndex;
    s_intDirty = false;
}

#endif // NERDMINER_DUAL_SCREEN
