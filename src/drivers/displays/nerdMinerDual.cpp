#include "display.h"

#ifdef NERDMINER_DUAL_SCREEN

#include "nerdMinerDual.h"
#include "ili9341Ext.h"
#include <TFT_eSPI.h>
#include "monitor.h"
#include "version.h"
#include "drivers/devices/device.h"

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
    pinMode(EXT_TFT_CS, OUTPUT);
    digitalWrite(EXT_TFT_CS, HIGH);
    ili9341ExtQuiesce();
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

void nerd_draw_int_nav_hud(int screenIndex, unsigned long mElapsed)
{
    (void)mElapsed;
    static const char *names[] = {"MINING", "CLOCK", "NETWORK", "PRICE"};
    const int n = 4;
    if (screenIndex < 0 || screenIndex >= n) {
        screenIndex = 0;
    }

    background.fillSprite(TFT_BLACK);
    background.fillRect(0, 0, 240, 22, 0x2104);
    background.setTextDatum(TL_DATUM);
    background.setTextColor(0xDEDB, 0x2104);
    background.setTextFont(2);
    background.setTextSize(1);
    background.drawString("NAV  INT ST7789", 6, 4);
    background.setTextColor(0x7BEF, 0x2104);
    background.drawString(CURRENT_VERSION, 190, 4);

    const char *status = "WAIT";
    uint16_t statusColor = 0xFDA0;
    if (mMonitor.NerdStatus == NM_hashing) {
        status = "HASHING";
        statusColor = 0x07E0;
    } else if (mMonitor.NerdStatus == NM_Connecting) {
        status = "WIFI";
        statusColor = 0x07FF;
    } else if (mMonitor.NerdStatus == NM_waitingConfig) {
        status = "SETUP";
        statusColor = 0xFD20;
    }

    background.setTextColor(0x9C92, TFT_BLACK);
    background.drawString("View", 8, 28);
    for (int i = 0; i < n; ++i) {
        const int y = 46 + i * 16;
        if (i == screenIndex) {
            background.fillRect(6, y - 2, 228, 16, 0x3186);
            background.setTextColor(0xDEDB, 0x3186);
            background.drawString(">", 10, y);
            background.drawString(names[i], 24, y);
            background.drawString("EXT", 190, y);
        } else {
            background.setTextColor(0x7BEF, TFT_BLACK);
            background.drawString(names[i], 24, y);
        }
    }

    background.drawFastHLine(0, 112, 240, 0x3186);
    background.setTextColor(statusColor, TFT_BLACK);
    background.drawString(status, 8, 116);
    background.setTextColor(0x9C92, TFT_BLACK);
    background.drawString("; next  p prev", 70, 116);
    background.setTextColor(0x7BEF, TFT_BLACK);
    background.drawString("r rot  b bl  hold BKSP reset", 8, 128);

    background.pushSprite(0, 0);
}

#endif // NERDMINER_DUAL_SCREEN
