#include "display.h"

#ifdef NERDMINER_DUAL_SCREEN

#include "nerdMinerDual.h"
#include "ili9341Ext.h"
#include <TFT_eSPI.h>
#include "monitor.h"
#include "version.h"
#include "drivers/devices/device.h"
#include "media/images_240_135.h"

#if TFT_MOSI != 35 || TFT_SCLK != 36 || TFT_CS != 37 || TFT_DC != 34 || TFT_RST != 33 || TFT_BL != 38
#error "Dirt contract v1: INT ST7789 must stay Setup215 MOSI35 SCLK36 CS37 DC34 RST33 BL38"
#endif

extern DisplayDriver tDisplayV1Driver;
extern DisplayDriver ili9341ExtDriver;
extern TFT_eSPI tft;
extern TFT_eSprite background;
extern monitor_data mMonitor;

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
static uint8_t s_intStatus = 0xFF;

static const char *intStatusLabel(uint16_t *color)
{
    if (mMonitor.NerdStatus == NM_hashing) {
        *color = 0x07E0;
        return "HASHING";
    }
    if (mMonitor.NerdStatus == NM_Connecting) {
        *color = 0x07FF;
        return "WIFI";
    }
    if (mMonitor.NerdStatus == NM_waitingConfig) {
        *color = 0xFD20;
        return "SETUP";
    }
    *color = 0xFDA0;
    return "WAIT";
}

static void pushStockCyclicChrome(int screenIndex)
{
    // Same 240x135 bitmaps as stock tDisplayV1 cyclic screens — INT is the
    // menu/chooser, not a dual-HUD list. Live mining goods stay on EXT.
    switch (screenIndex) {
    case 1:
        background.pushImage(0, 0, minerClockWidth, minerClockHeight, minerClockScreen);
        break;
    case 2:
        background.pushImage(0, 0, globalHashWidth, globalHashHeight, globalHashScreen);
        break;
    case 3:
        background.pushImage(0, 0, priceScreenWidth, priceScreenHeight, priceScreen);
        break;
    default:
        background.pushImage(0, 0, MinerWidth, MinerHeight, MinerScreen);
        break;
    }
}

static void drawStockMenuStrip(int screenIndex, const char *status, uint16_t statusColor)
{
    static const char *names[] = {"MINING", "CLOCK", "NETWORK", "PRICE"};
    background.fillRect(0, 109, 240, 26, 0x2104);
    background.drawFastHLine(0, 109, 240, 0xDEDB);
    background.setTextDatum(TL_DATUM);
    background.setTextFont(2);
    background.setTextSize(1);
    background.setTextColor(0xDEDB, 0x2104);
    background.drawString(names[screenIndex], 6, 111);
    for (int i = 0; i < 4; ++i) {
        background.fillCircle(88 + i * 12, 118, 3, (i == screenIndex) ? 0xFD20 : 0x4A49);
    }
    background.setTextDatum(TR_DATUM);
    background.setTextColor(statusColor, 0x2104);
    background.drawString(status, 234, 111);
    background.setTextDatum(TL_DATUM);
    background.setTextColor(0x9C92, 0x2104);
    background.drawString(";/dn next  p/, prev", 6, 123);
}

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
    (void)mElapsed;
    const int n = 4;
    if (screenIndex < 0 || screenIndex >= n) {
        screenIndex = 0;
    }

    uint16_t statusColor = 0xFDA0;
    const char *status = intStatusLabel(&statusColor);
    const uint8_t statusId = (uint8_t)mMonitor.NerdStatus;
    const bool viewChanged = (screenIndex != s_intDrawn);
    const bool statusChanged = (statusId != s_intStatus);

    // Throttle: skip identical 1 Hz ticks. Full stock bitmap only on view change.
    if (!s_intDirty && !viewChanged && !statusChanged) {
        return;
    }

    if (viewChanged || s_intDrawn < 0) {
        pushStockCyclicChrome(screenIndex);
        drawStockMenuStrip(screenIndex, status, statusColor);
    } else {
        drawStockMenuStrip(screenIndex, status, statusColor);
    }

    background.pushSprite(0, 0);
    s_intDrawn = screenIndex;
    s_intStatus = statusId;
    s_intDirty = false;
}

#endif // NERDMINER_DUAL_SCREEN
