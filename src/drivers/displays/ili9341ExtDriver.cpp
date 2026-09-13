#include "displayDriver.h"

#ifdef NERDMINER_DUAL_SCREEN

#include "ili9341Ext.h"
#include "nerdMinerDual.h"
#include "monitor.h"
#include "version.h"
#include "drivers/devices/device.h"

// NerdMiner V1 palette (match INT theme, native 320x240 layout — not a blit).
#define C_BG      0x1082
#define C_PANEL   0x2104
#define C_ACCENT  0xDEDB
#define C_ORANGE  0xFD20
#define C_MUTED   0x9C92
#define C_LABEL   0x7BEF
#define C_WHITE   0xFFFF
#define C_BLACK   0x0000
#define C_LINE    0x3186
#define C_OK      0x07E0

#define W EXT_TFT_WIDTH
#define H EXT_TFT_HEIGHT

// runMonitor() calls drawCurrentScreen() at ~1 Hz. A full ili9341ExtFillScreen()
// on every tick is a visible top-down GRAM wipe (C_BG is near-black 0x1082).
// Clear the panel only when the cyclic view changes; widgets fill their own
// boxes on refresh. SD quiesce (EXT CS HIGH) does not blank ILI9341 GRAM.
enum {
    EXT_SCR_NONE = -1,
    EXT_SCR_MINER = 0,
    EXT_SCR_CLOCK = 1,
    EXT_SCR_GLOBAL = 2,
    EXT_SCR_PRICE = 3
};

static int s_extScreen = EXT_SCR_NONE;

static void enterExtScreen(int id)
{
    if (s_extScreen != id) {
        ili9341ExtFillScreen(C_BG);
        s_extScreen = id;
    }
}

static void fillContentBand(int16_t y, int16_t h)
{
    ili9341ExtFillRect(8, y, (int16_t)(W - 16), h, C_BG);
}

static void drawHeader(const char *title, const char *clock, const char *temp)
{
    ili9341ExtFillRect(0, 0, W, 28, C_PANEL);
    ili9341ExtDrawText(8, 7, title, C_ACCENT, C_PANEL, 2);
    if (temp && temp[0]) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%sC", temp);
        ili9341ExtDrawTextRight(250, 9, buf, C_MUTED, C_PANEL, 1);
    }
    if (clock && clock[0]) {
        ili9341ExtDrawTextRight(312, 9, clock, C_WHITE, C_PANEL, 1);
    }
    ili9341ExtHLine(0, 28, W, C_LINE);
}

static void drawFooter(const char *left, const char *right)
{
    ili9341ExtFillRect(0, 220, W, 20, C_PANEL);
    ili9341ExtHLine(0, 220, W, C_LINE);
    if (left) {
        ili9341ExtDrawText(8, 225, left, C_MUTED, C_PANEL, 1);
    }
    if (right) {
        ili9341ExtDrawTextRight(312, 225, right, C_MUTED, C_PANEL, 1);
    }
}

static void drawStatCell(int16_t x, int16_t y, int16_t w, int16_t h,
                         const char *label, const char *value, uint16_t valueColor)
{
    ili9341ExtFillRect(x, y, w, h, C_PANEL);
    ili9341ExtDrawRect(x, y, w, h, C_LINE);
    ili9341ExtDrawText((int16_t)(x + 8), (int16_t)(y + 6), label, C_LABEL, C_PANEL, 1);
    ili9341ExtDrawText((int16_t)(x + 8), (int16_t)(y + 20), value, valueColor, C_PANEL, 2);
}

static void extMinerScreen(unsigned long mElapsed)
{
    mining_data data = getMiningData(mElapsed);
    Serial.printf(">>> EXT miner %s KH/s shares=%s hashes=%sK\n",
                  data.currentHashRate.c_str(), data.completedShares.c_str(),
                  data.totalKHashes.c_str());

    enterExtScreen(EXT_SCR_MINER);
    drawHeader("MINING", data.currentTime.c_str(), data.temp.c_str());

    ili9341ExtDrawText(8, 38, "HASHRATE  KH/s", C_LABEL, C_BG, 1);
    fillContentBand(54, 52);
    ili9341ExtDraw7Seg(8, 54, data.currentHashRate.c_str(), C_ORANGE, C_BG, 28, 52, 6);

    drawStatCell(8, 118, 100, 46, "SHARES", data.completedShares.c_str(), C_ACCENT);
    drawStatCell(110, 118, 100, 46, "BEST DIFF", data.bestDiff.c_str(), C_ACCENT);
    drawStatCell(212, 118, 100, 46, "VALID", data.valids.c_str(), C_OK);

    drawStatCell(8, 168, 100, 46, "TEMPLATES", data.templates.c_str(), C_WHITE);
    drawStatCell(110, 168, 100, 46, "MHASHES", data.totalMHashes.c_str(), C_WHITE);
    drawStatCell(212, 168, 100, 46, "UPTIME", data.timeMining.c_str(), C_WHITE);

    drawFooter("EXT ILI9341 320x240", CURRENT_VERSION);
}

static void extClockScreen(unsigned long mElapsed)
{
    clock_data data = getClockData(mElapsed);
    Serial.printf(">>> EXT clock %s rate=%s\n", data.currentTime.c_str(), data.currentHashRate.c_str());

    enterExtScreen(EXT_SCR_CLOCK);
    drawHeader("CLOCK", data.currentDate.c_str(), nullptr);

    ili9341ExtDrawText(8, 40, "LOCAL TIME", C_LABEL, C_BG, 1);
    fillContentBand(58, 56);
    ili9341ExtDraw7Seg(8, 58, data.currentTime.c_str(), C_ACCENT, C_BG, 26, 56, 5);

    drawStatCell(8, 132, 152, 46, "HASHRATE KH/s", data.currentHashRate.c_str(), C_ORANGE);
    drawStatCell(164, 132, 148, 46, "BLOCK", data.blockHeight.c_str(), C_WHITE);
    drawStatCell(8, 182, 304, 32, "BTC PRICE", data.btcPrice.c_str(), C_ACCENT);

    drawFooter("EXT ILI9341 320x240", CURRENT_VERSION);
}

static void extGlobalScreen(unsigned long mElapsed)
{
    coin_data data = getCoinData(mElapsed);
    Serial.printf(">>> EXT global %s height=%s\n", data.globalHashRate.c_str(), data.blockHeight.c_str());

    enterExtScreen(EXT_SCR_GLOBAL);
    drawHeader("NETWORK", data.currentTime.c_str(), nullptr);

    ili9341ExtDrawText(8, 38, "BLOCK HEIGHT", C_LABEL, C_BG, 1);
    fillContentBand(54, 28);
    ili9341ExtDrawText(8, 54, data.blockHeight.c_str(), C_ACCENT, C_BG, 3);

    drawStatCell(8, 92, 152, 46, "GLOBAL HASH", data.globalHashRate.c_str(), C_WHITE);
    drawStatCell(164, 92, 148, 46, "DIFFICULTY", data.netwrokDifficulty.c_str(), C_WHITE);
    drawStatCell(8, 142, 152, 46, "FEE", data.halfHourFee.c_str(), C_ORANGE);
    drawStatCell(164, 142, 148, 46, "MINER KH/s", data.currentHashRate.c_str(), C_ORANGE);

    const int pct = (int)data.progressPercent;
    ili9341ExtFillRect(8, 196, 304, 16, C_PANEL);
    const int filled = 2 + (300 * pct / 100);
    ili9341ExtFillRect(10, 198, (int16_t)filled, 12, C_ACCENT);
    char remain[24];
    snprintf(remain, sizeof(remain), "%s left", data.remainingBlocks.c_str());
    ili9341ExtDrawText(12, 199, remain, C_BLACK, C_ACCENT, 1);

    drawFooter("EXT ILI9341 320x240", CURRENT_VERSION);
}

static void extPriceScreen(unsigned long mElapsed)
{
    clock_data data = getClockData(mElapsed);
    Serial.printf(">>> EXT price %s rate=%s\n", data.btcPrice.c_str(), data.currentHashRate.c_str());

    enterExtScreen(EXT_SCR_PRICE);
    drawHeader("BTC PRICE", data.currentTime.c_str(), nullptr);

    ili9341ExtDrawText(8, 42, "USD", C_LABEL, C_BG, 1);
    fillContentBand(62, 36);
    ili9341ExtDrawText(8, 62, data.btcPrice.c_str(), C_ACCENT, C_BG, 3);

    drawStatCell(8, 120, 152, 46, "HASHRATE KH/s", data.currentHashRate.c_str(), C_ORANGE);
    drawStatCell(164, 120, 148, 46, "BLOCK", data.blockHeight.c_str(), C_WHITE);
    drawStatCell(8, 170, 304, 42, "SHARES", data.completedShares.c_str(), C_ACCENT);

    drawFooter("EXT ILI9341 320x240", CURRENT_VERSION);
}

static void extInit(void)
{
    // Hardware init is owned by nerd_dual_init() so SD can quiesce first.
}

static void extAltState(void)
{
    // Backlight / rotate stay on INT.
}

static void extAltRotation(void)
{
}

static void extLoading(void)
{
}

static void extSetup(void)
{
}

static void extAnimate(unsigned long)
{
}

static void extLed(unsigned long)
{
}

static CyclicScreenFunction extCyclicScreens[] = {
    extMinerScreen, extClockScreen, extGlobalScreen, extPriceScreen
};

DisplayDriver ili9341ExtDriver = {
    extInit,
    extAltState,
    extAltRotation,
    extLoading,
    extSetup,
    extCyclicScreens,
    extAnimate,
    extLed,
    SCREENS_ARRAY_SIZE(extCyclicScreens),
    0,
    EXT_TFT_WIDTH,
    EXT_TFT_HEIGHT
};

#endif // NERDMINER_DUAL_SCREEN
