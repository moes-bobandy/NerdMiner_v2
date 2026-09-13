#include "displayDriver.h"

#ifdef NERDMINER_DUAL_SCREEN

#include "ili9341Ext.h"
#include "nerdMinerDual.h"
#include "monitor.h"
#include "version.h"
#include "drivers/devices/device.h"

#include <string.h>

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

// Locked wipe contract: a full-panel C_BG fill on every 1 Hz cyclic tick
// is Dirt's film-slide (full 320x240 top→bottom dark fill, then restore).
// Full clear ONLY when the cyclic screen index changes. Live stats are
// dirty-field updates — no mid-frame blank, no full-width content wipe.
enum {
    EXT_SCR_NONE = -1,
    EXT_SCR_MINER = 0,
    EXT_SCR_CLOCK = 1,
    EXT_SCR_GLOBAL = 2,
    EXT_SCR_PRICE = 3
};

static int s_extScreen = EXT_SCR_NONE;
static bool s_chrome = false;

static char s_hdrClock[16];
static char s_hdrTemp[16];
static char s_clock[16];
static char s_hash[16];
static char s_shares[16];
static char s_best[16];
static char s_valids[16];
static char s_templates[16];
static char s_mhashes[16];
static char s_uptime[16];
static char s_block[16];
static char s_price[24];
static char s_ghash[16];
static char s_diff[16];
static char s_fee[16];
static char s_remain[24];
static int s_pct = -1;

static void clearFieldCache()
{
    s_chrome = false;
    s_hdrClock[0] = s_hdrTemp[0] = 0;
    s_clock[0] = s_hash[0] = s_shares[0] = 0;
    s_best[0] = s_valids[0] = s_templates[0] = s_mhashes[0] = 0;
    s_uptime[0] = s_block[0] = s_price[0] = 0;
    s_ghash[0] = s_diff[0] = s_fee[0] = s_remain[0] = 0;
    s_pct = -1;
}

static bool takeField(char *slot, size_t cap, const char *now)
{
    if (!now) {
        now = "";
    }
    if (strncmp(slot, now, cap) == 0) {
        return false;
    }
    strncpy(slot, now, cap - 1);
    slot[cap - 1] = '\0';
    return true;
}

static void enterExtScreen(int id)
{
    if (s_extScreen != id) {
        ili9341ExtFillScreen(C_BG);
        s_extScreen = id;
        clearFieldCache();
    }
}

static void drawHeaderChrome(const char *title)
{
    ili9341ExtFillRect(0, 0, W, 28, C_PANEL);
    ili9341ExtDrawText(8, 7, title, C_ACCENT, C_PANEL, 2);
    ili9341ExtHLine(0, 28, W, C_LINE);
}

static void drawHeaderStats(const char *clock, const char *temp)
{
    const bool clockCh = takeField(s_hdrClock, sizeof(s_hdrClock), clock ? clock : "");
    const bool tempCh = takeField(s_hdrTemp, sizeof(s_hdrTemp), temp ? temp : "");
    if (!clockCh && !tempCh) {
        return;
    }
    ili9341ExtFillRect(168, 4, 152, 20, C_PANEL);
    if (temp && temp[0]) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%sC", temp);
        ili9341ExtDrawTextRight(250, 9, buf, C_MUTED, C_PANEL, 1);
    }
    if (clock && clock[0]) {
        ili9341ExtDrawTextRight(312, 9, clock, C_WHITE, C_PANEL, 1);
    }
}

static void drawFooterChrome(void)
{
    ili9341ExtFillRect(0, 220, W, 20, C_PANEL);
    ili9341ExtHLine(0, 220, W, C_LINE);
    ili9341ExtDrawText(8, 225, "EXT ILI9341 320x240", C_MUTED, C_PANEL, 1);
    ili9341ExtDrawTextRight(312, 225, CURRENT_VERSION, C_MUTED, C_PANEL, 1);
}

static void drawStatCell(int16_t x, int16_t y, int16_t w, int16_t h,
                         const char *label, const char *value, uint16_t valueColor)
{
    ili9341ExtFillRect(x, y, w, h, C_PANEL);
    ili9341ExtDrawRect(x, y, w, h, C_LINE);
    ili9341ExtDrawText((int16_t)(x + 8), (int16_t)(y + 6), label, C_LABEL, C_PANEL, 1);
    ili9341ExtDrawText((int16_t)(x + 8), (int16_t)(y + 20), value, valueColor, C_PANEL, 2);
}

static void dirtyStat(char *slot, size_t cap, const char *value,
                      int16_t x, int16_t y, int16_t w, int16_t h,
                      const char *label, uint16_t valueColor)
{
    if (takeField(slot, cap, value ? value : "")) {
        drawStatCell(x, y, w, h, label, value, valueColor);
    }
}

static void dirty7Seg(char *slot, size_t cap, const char *text,
                      int16_t x, int16_t y, uint16_t on,
                      int16_t w, int16_t h, int16_t gap, int padChars)
{
    if (!takeField(slot, cap, text ? text : "")) {
        return;
    }
    char buf[16];
    if (padChars > 14) {
        padChars = 14;
    }
    snprintf(buf, sizeof(buf), "%-*s", padChars, text ? text : "");
    ili9341ExtDraw7Seg(x, y, buf, on, C_BG, w, h, gap);
}

static void dirtyText(char *slot, size_t cap, const char *text,
                      int16_t x, int16_t y, uint16_t fg, uint8_t scale, int padChars)
{
    if (!takeField(slot, cap, text ? text : "")) {
        return;
    }
    char buf[24];
    if (padChars > 22) {
        padChars = 22;
    }
    snprintf(buf, sizeof(buf), "%-*s", padChars, text ? text : "");
    ili9341ExtDrawText(x, y, buf, fg, C_BG, scale);
}

static void extMinerScreen(unsigned long mElapsed)
{
    mining_data data = getMiningData(mElapsed);
    Serial.printf(">>> EXT miner %s KH/s shares=%s hashes=%sK\n",
                  data.currentHashRate.c_str(), data.completedShares.c_str(),
                  data.totalKHashes.c_str());

    enterExtScreen(EXT_SCR_MINER);
    if (!s_chrome) {
        drawHeaderChrome("MINING");
        ili9341ExtDrawText(8, 38, "HASHRATE  KH/s", C_LABEL, C_BG, 1);
        drawFooterChrome();
        s_chrome = true;
    }
    drawHeaderStats(data.currentTime.c_str(), data.temp.c_str());
    dirty7Seg(s_hash, sizeof(s_hash), data.currentHashRate.c_str(),
              8, 54, C_ORANGE, 28, 52, 6, 8);
    dirtyStat(s_shares, sizeof(s_shares), data.completedShares.c_str(),
              8, 118, 100, 46, "SHARES", C_ACCENT);
    dirtyStat(s_best, sizeof(s_best), data.bestDiff.c_str(),
              110, 118, 100, 46, "BEST DIFF", C_ACCENT);
    dirtyStat(s_valids, sizeof(s_valids), data.valids.c_str(),
              212, 118, 100, 46, "VALID", C_OK);
    dirtyStat(s_templates, sizeof(s_templates), data.templates.c_str(),
              8, 168, 100, 46, "TEMPLATES", C_WHITE);
    dirtyStat(s_mhashes, sizeof(s_mhashes), data.totalMHashes.c_str(),
              110, 168, 100, 46, "MHASHES", C_WHITE);
    dirtyStat(s_uptime, sizeof(s_uptime), data.timeMining.c_str(),
              212, 168, 100, 46, "UPTIME", C_WHITE);
}

static void extClockScreen(unsigned long mElapsed)
{
    clock_data data = getClockData(mElapsed);
    Serial.printf(">>> EXT clock %s rate=%s\n", data.currentTime.c_str(), data.currentHashRate.c_str());

    enterExtScreen(EXT_SCR_CLOCK);
    if (!s_chrome) {
        drawHeaderChrome("CLOCK");
        ili9341ExtDrawText(8, 40, "LOCAL TIME", C_LABEL, C_BG, 1);
        drawFooterChrome();
        s_chrome = true;
    }
    drawHeaderStats(data.currentDate.c_str(), nullptr);
    dirty7Seg(s_clock, sizeof(s_clock), data.currentTime.c_str(),
              8, 58, C_ACCENT, 26, 56, 5, 8);
    dirtyStat(s_hash, sizeof(s_hash), data.currentHashRate.c_str(),
              8, 132, 152, 46, "HASHRATE KH/s", C_ORANGE);
    dirtyStat(s_block, sizeof(s_block), data.blockHeight.c_str(),
              164, 132, 148, 46, "BLOCK", C_WHITE);
    dirtyStat(s_price, sizeof(s_price), data.btcPrice.c_str(),
              8, 182, 304, 32, "BTC PRICE", C_ACCENT);
}

static void extGlobalScreen(unsigned long mElapsed)
{
    coin_data data = getCoinData(mElapsed);
    Serial.printf(">>> EXT global %s height=%s\n", data.globalHashRate.c_str(), data.blockHeight.c_str());

    enterExtScreen(EXT_SCR_GLOBAL);
    if (!s_chrome) {
        drawHeaderChrome("NETWORK");
        ili9341ExtDrawText(8, 38, "BLOCK HEIGHT", C_LABEL, C_BG, 1);
        drawFooterChrome();
        s_chrome = true;
    }
    drawHeaderStats(data.currentTime.c_str(), nullptr);
    dirtyText(s_block, sizeof(s_block), data.blockHeight.c_str(),
              8, 54, C_ACCENT, 3, 10);
    dirtyStat(s_ghash, sizeof(s_ghash), data.globalHashRate.c_str(),
              8, 92, 152, 46, "GLOBAL HASH", C_WHITE);
    dirtyStat(s_diff, sizeof(s_diff), data.netwrokDifficulty.c_str(),
              164, 92, 148, 46, "DIFFICULTY", C_WHITE);
    dirtyStat(s_fee, sizeof(s_fee), data.halfHourFee.c_str(),
              8, 142, 152, 46, "FEE", C_ORANGE);
    dirtyStat(s_hash, sizeof(s_hash), data.currentHashRate.c_str(),
              164, 142, 148, 46, "MINER KH/s", C_ORANGE);

    const int pct = (int)data.progressPercent;
    if (pct != s_pct || takeField(s_remain, sizeof(s_remain), data.remainingBlocks.c_str())) {
        s_pct = pct;
        ili9341ExtFillRect(8, 196, 304, 16, C_PANEL);
        const int filled = 2 + (300 * pct / 100);
        ili9341ExtFillRect(10, 198, (int16_t)filled, 12, C_ACCENT);
        char remain[24];
        snprintf(remain, sizeof(remain), "%s left", data.remainingBlocks.c_str());
        ili9341ExtDrawText(12, 199, remain, C_BLACK, C_ACCENT, 1);
    }
}

static void extPriceScreen(unsigned long mElapsed)
{
    clock_data data = getClockData(mElapsed);
    Serial.printf(">>> EXT price %s rate=%s\n", data.btcPrice.c_str(), data.currentHashRate.c_str());

    enterExtScreen(EXT_SCR_PRICE);
    if (!s_chrome) {
        drawHeaderChrome("BTC PRICE");
        ili9341ExtDrawText(8, 42, "USD", C_LABEL, C_BG, 1);
        drawFooterChrome();
        s_chrome = true;
    }
    drawHeaderStats(data.currentTime.c_str(), nullptr);
    dirtyText(s_price, sizeof(s_price), data.btcPrice.c_str(),
              8, 62, C_ACCENT, 3, 12);
    dirtyStat(s_hash, sizeof(s_hash), data.currentHashRate.c_str(),
              8, 120, 152, 46, "HASHRATE KH/s", C_ORANGE);
    dirtyStat(s_block, sizeof(s_block), data.blockHeight.c_str(),
              164, 120, 148, 46, "BLOCK", C_WHITE);
    dirtyStat(s_shares, sizeof(s_shares), data.completedShares.c_str(),
              8, 170, 304, 42, "SHARES", C_ACCENT);
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
