#include "displayDriver.h"

#ifdef NERDMINER_DUAL_SCREEN

#include "ili9341Ext.h"
#include "nerdMinerDual.h"
#include "monitor.h"
#include "version.h"
#include "drivers/devices/device.h"

#include <string.h>

// Stock V1 palette (same family as tDisplayV1 MinerScreen).
#define C_BG      0x0000
#define C_CREAM   0xDEDB
#define C_PANEL   0x0000
#define C_MUTED   0x9C92
#define C_ORANGE  0xFD20
#define C_WHITE   0xFFFF
#define C_BLACK   0x0000
#define C_OK      0x07E0

#define W EXT_TFT_WIDTH
#define H EXT_TFT_HEIGHT

// Locked wipe contract: full-panel fill ONLY when the cyclic index changes.
enum {
    EXT_SCR_NONE = -1,
    EXT_SCR_MINER = 0,
    EXT_SCR_CLOCK = 1,
    EXT_SCR_GLOBAL = 2,
    EXT_SCR_PRICE = 3
};

static int s_extScreen = EXT_SCR_NONE;
static bool s_chrome = false;
static int16_t s_artH = 180;

static char s_hdrClock[16];
static char s_hdrTemp[16];
static char s_clock[16];
static char s_hash[16];
static char s_shares[16];
static char s_best[16];
static char s_valids[16];
static char s_templates[16];
static char s_mhashes[16];
static char s_khashes[16];
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
    s_khashes[0] = s_uptime[0] = s_block[0] = s_price[0] = 0;
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

static int16_t mapX(int srcX, uint16_t sw)
{
    if (sw == 0) {
        return (int16_t)srcX;
    }
    return (int16_t)((int32_t)srcX * W / sw);
}

static int16_t mapY(int srcY, uint16_t sh)
{
    if (sh == 0 || s_artH <= 0) {
        return (int16_t)srcY;
    }
    return (int16_t)((int32_t)srcY * s_artH / sh);
}

static void blitStockArt(int screenIndex)
{
    const uint16_t *bits = nullptr;
    uint16_t sw = 0, sh = 0;
    tDisplayV1StockFrame(screenIndex, &bits, &sw, &sh);
    if (!bits || sw == 0 || sh == 0) {
        s_artH = 180;
        return;
    }
    int16_t dh = (int16_t)((int32_t)sh * W / sw);
    if (dh > 200) {
        dh = 200;
    }
    s_artH = dh;
    ili9341ExtPushImageScaled(0, 0, W, dh, bits, (int16_t)sw, (int16_t)sh);
}

static void drawGoodsBand(void)
{
    ili9341ExtFillRect(0, s_artH, W, (int16_t)(H - s_artH), C_BG);
    ili9341ExtHLine(0, s_artH, W, C_CREAM);
}

static void dirtyText(char *slot, size_t cap, const char *text,
                      int16_t x, int16_t y, uint16_t fg, uint16_t bg,
                      uint8_t scale, int padChars)
{
    if (!takeField(slot, cap, text ? text : "")) {
        return;
    }
    char buf[24];
    if (padChars > 22) {
        padChars = 22;
    }
    snprintf(buf, sizeof(buf), "%-*s", padChars, text ? text : "");
    ili9341ExtDrawText(x, y, buf, fg, bg, scale);
}

static void dirtyTextRight(char *slot, size_t cap, const char *text,
                           int16_t right, int16_t y, uint16_t fg, uint16_t bg,
                           uint8_t scale)
{
    if (!takeField(slot, cap, text ? text : "")) {
        return;
    }
    const int16_t w = ili9341ExtTextWidth(text ? text : "", scale);
    ili9341ExtFillRect((int16_t)(right - w - 8), y, (int16_t)(w + 8), (int16_t)(8 * scale), bg);
    ili9341ExtDrawTextRight(right, y, text ? text : "", fg, bg, scale);
}

static void extMinerScreen(unsigned long mElapsed)
{
    mining_data data = getMiningData(mElapsed);
    Serial.printf(">>> EXT miner %s KH/s shares=%s hashes=%sK\n",
                  data.currentHashRate.c_str(), data.completedShares.c_str(),
                  data.totalKHashes.c_str());

    enterExtScreen(EXT_SCR_MINER);
    const uint16_t *bits = nullptr;
    uint16_t sw = 240, sh = 135;
    tDisplayV1StockFrame(0, &bits, &sw, &sh);
    (void)bits;
    if (!s_chrome) {
        blitStockArt(0);
        drawGoodsBand();
        ili9341ExtDrawText(8, (int16_t)(s_artH + 4), "BLOCK TEMPLATES", C_MUTED, C_BG, 1);
        ili9341ExtDrawText(120, (int16_t)(s_artH + 4), "BEST DIFFICULTY", C_MUTED, C_BG, 1);
        ili9341ExtDrawText(8, (int16_t)(s_artH + 30), "32BITS SHARES", C_MUTED, C_BG, 1);
        ili9341ExtDrawText(160, (int16_t)(s_artH + 30), "VALID BLOCKS", C_MUTED, C_BG, 1);
        s_chrome = true;
    }

    if (takeField(s_templates, sizeof(s_templates), data.templates.c_str())) {
        ili9341ExtDrawText(mapX(140, sw), mapY(15, sh), data.templates.c_str(), C_CREAM, C_BG, 1);
        ili9341ExtDrawText(8, (int16_t)(s_artH + 14), data.templates.c_str(), C_CREAM, C_BG, 1);
    }
    if (takeField(s_best, sizeof(s_best), data.bestDiff.c_str())) {
        ili9341ExtDrawText(mapX(140, sw), mapY(38, sh), data.bestDiff.c_str(), C_CREAM, C_BG, 1);
        ili9341ExtDrawText(120, (int16_t)(s_artH + 14), data.bestDiff.c_str(), C_CREAM, C_BG, 1);
    }
    if (takeField(s_shares, sizeof(s_shares), data.completedShares.c_str())) {
        ili9341ExtDrawText(mapX(140, sw), mapY(60, sh), data.completedShares.c_str(), C_CREAM, C_BG, 1);
        ili9341ExtDrawText(8, (int16_t)(s_artH + 40), data.completedShares.c_str(), C_CREAM, C_BG, 1);
    }
    if (takeField(s_valids, sizeof(s_valids), data.valids.c_str())) {
        ili9341ExtDrawText(mapX(210, sw), mapY(45, sh), data.valids.c_str(), C_CREAM, C_BG, 1);
        ili9341ExtDrawText(160, (int16_t)(s_artH + 40), data.valids.c_str(), C_OK, C_BG, 1);
    }
    dirtyTextRight(s_hash, sizeof(s_hash), data.currentHashRate.c_str(),
                   mapX(96, sw), mapY(88, sh), C_BLACK, C_CREAM, 2);
    dirtyTextRight(s_mhashes, sizeof(s_mhashes), data.totalMHashes.c_str(),
                   mapX(200, sw), mapY(106, sh), C_BLACK, C_CREAM, 1);
    dirtyTextRight(s_uptime, sizeof(s_uptime), data.timeMining.c_str(),
                   mapX(226, sw), mapY(85, sh), C_CREAM, C_BG, 1);
    dirtyTextRight(s_hdrTemp, sizeof(s_hdrTemp), data.temp.c_str(),
                   mapX(180, sw), mapY(1, sh), C_BLACK, C_CREAM, 1);
    dirtyTextRight(s_hdrClock, sizeof(s_hdrClock), data.currentTime.c_str(),
                   mapX(215, sw), mapY(1, sh), C_BLACK, C_CREAM, 1);
    dirtyText(s_khashes, sizeof(s_khashes), data.totalKHashes.c_str(),
              240, (int16_t)(s_artH + 14), C_WHITE, C_BG, 1, 8);
}

static void extClockScreen(unsigned long mElapsed)
{
    clock_data data = getClockData(mElapsed);
    Serial.printf(">>> EXT clock %s rate=%s\n", data.currentTime.c_str(), data.currentHashRate.c_str());

    enterExtScreen(EXT_SCR_CLOCK);
    const uint16_t *bits = nullptr;
    uint16_t sw = 240, sh = 135;
    tDisplayV1StockFrame(1, &bits, &sw, &sh);
    if (!s_chrome) {
        blitStockArt(1);
        drawGoodsBand();
        ili9341ExtDrawText(8, (int16_t)(s_artH + 8), "HASHRATE KH/s", C_MUTED, C_BG, 1);
        ili9341ExtDrawText(160, (int16_t)(s_artH + 8), "BLOCK HEIGHT", C_MUTED, C_BG, 1);
        s_chrome = true;
    }

    dirtyText(s_clock, sizeof(s_clock), data.currentTime.c_str(),
              mapX(70, sw), mapY(25, sh), C_CREAM, C_BG, 2, 8);
    dirtyText(s_price, sizeof(s_price), data.btcPrice.c_str(),
              mapX(148, sw), mapY(1, sh), C_BLACK, C_CREAM, 1, 10);
    if (takeField(s_hash, sizeof(s_hash), data.currentHashRate.c_str())) {
        ili9341ExtDrawTextRight(mapX(70, sw), mapY(103, sh), data.currentHashRate.c_str(), C_BLACK, C_CREAM, 2);
        ili9341ExtDrawText(8, (int16_t)(s_artH + 20), data.currentHashRate.c_str(), C_ORANGE, C_BG, 2);
    }
    if (takeField(s_block, sizeof(s_block), data.blockHeight.c_str())) {
        ili9341ExtDrawTextRight(mapX(190, sw), mapY(110, sh), data.blockHeight.c_str(), C_BLACK, C_CREAM, 1);
        ili9341ExtDrawText(160, (int16_t)(s_artH + 20), data.blockHeight.c_str(), C_WHITE, C_BG, 2);
    }
}

static void extGlobalScreen(unsigned long mElapsed)
{
    coin_data data = getCoinData(mElapsed);
    Serial.printf(">>> EXT global %s height=%s\n", data.globalHashRate.c_str(), data.blockHeight.c_str());

    enterExtScreen(EXT_SCR_GLOBAL);
    const uint16_t *bits = nullptr;
    uint16_t sw = 240, sh = 135;
    tDisplayV1StockFrame(2, &bits, &sw, &sh);
    if (!s_chrome) {
        blitStockArt(2);
        drawGoodsBand();
        ili9341ExtDrawText(8, (int16_t)(s_artH + 6), "GLOBAL HASH", C_MUTED, C_BG, 1);
        ili9341ExtDrawText(160, (int16_t)(s_artH + 6), "DIFFICULTY", C_MUTED, C_BG, 1);
        s_chrome = true;
    }

    dirtyText(s_price, sizeof(s_price), data.btcPrice.c_str(),
              mapX(148, sw), mapY(1, sh), C_BLACK, C_CREAM, 1, 10);
    dirtyText(s_hdrClock, sizeof(s_hdrClock), data.currentTime.c_str(),
              mapX(195, sw), mapY(1, sh), C_BLACK, C_CREAM, 1, 8);
    dirtyTextRight(s_fee, sizeof(s_fee), data.halfHourFee.c_str(),
                   mapX(230, sw), mapY(40, sh), C_MUTED, C_BG, 1);
    dirtyTextRight(s_block, sizeof(s_block), data.blockHeight.c_str(),
                   mapX(105, sw), mapY(80, sh), C_CREAM, C_BG, 2);
    if (takeField(s_ghash, sizeof(s_ghash), data.globalHashRate.c_str())) {
        ili9341ExtDrawTextRight(mapX(205, sw), mapY(115, sh), data.globalHashRate.c_str(), C_BLACK, C_CREAM, 1);
        ili9341ExtDrawText(8, (int16_t)(s_artH + 16), data.globalHashRate.c_str(), C_WHITE, C_BG, 1);
    }
    if (takeField(s_diff, sizeof(s_diff), data.netwrokDifficulty.c_str())) {
        ili9341ExtDrawTextRight(mapX(230, sw), mapY(68, sh), data.netwrokDifficulty.c_str(), C_MUTED, C_BG, 1);
        ili9341ExtDrawText(160, (int16_t)(s_artH + 16), data.netwrokDifficulty.c_str(), C_WHITE, C_BG, 1);
    }

    const int pct = (int)data.progressPercent;
    if (pct != s_pct || takeField(s_remain, sizeof(s_remain), data.remainingBlocks.c_str())) {
        s_pct = pct;
        const int16_t y = (int16_t)(s_artH + 36);
        ili9341ExtFillRect(8, y, 304, 16, C_PANEL);
        ili9341ExtDrawRect(8, y, 304, 16, C_CREAM);
        const int filled = 2 + (300 * pct / 100);
        ili9341ExtFillRect(10, (int16_t)(y + 2), (int16_t)filled, 12, C_CREAM);
        ili9341ExtDrawText(12, (int16_t)(y + 3), data.remainingBlocks.c_str(), C_BLACK, C_CREAM, 1);
    }
}

static void extPriceScreen(unsigned long mElapsed)
{
    clock_data data = getClockData(mElapsed);
    Serial.printf(">>> EXT price %s rate=%s\n", data.btcPrice.c_str(), data.currentHashRate.c_str());

    enterExtScreen(EXT_SCR_PRICE);
    const uint16_t *bits = nullptr;
    uint16_t sw = 240, sh = 128;
    tDisplayV1StockFrame(3, &bits, &sw, &sh);
    if (!s_chrome) {
        blitStockArt(3);
        drawGoodsBand();
        ili9341ExtDrawText(8, (int16_t)(s_artH + 8), "HASHRATE KH/s", C_MUTED, C_BG, 1);
        ili9341ExtDrawText(160, (int16_t)(s_artH + 8), "32BITS SHARES", C_MUTED, C_BG, 1);
        s_chrome = true;
    }

    dirtyText(s_hdrClock, sizeof(s_hdrClock), data.currentTime.c_str(),
              mapX(148, sw), mapY(1, sh), C_BLACK, C_CREAM, 1, 8);
    dirtyText(s_price, sizeof(s_price), data.btcPrice.c_str(),
              mapX(82, sw), mapY(50, sh), C_CREAM, C_BG, 2, 12);
    if (takeField(s_hash, sizeof(s_hash), data.currentHashRate.c_str())) {
        ili9341ExtDrawTextRight(mapX(70, sw), mapY(103, sh), data.currentHashRate.c_str(), C_BLACK, C_CREAM, 2);
        ili9341ExtDrawText(8, (int16_t)(s_artH + 20), data.currentHashRate.c_str(), C_ORANGE, C_BG, 2);
    }
    dirtyTextRight(s_block, sizeof(s_block), data.blockHeight.c_str(),
                   mapX(190, sw), mapY(110, sh), C_WHITE, C_BG, 1);
    dirtyText(s_shares, sizeof(s_shares), data.completedShares.c_str(),
              160, (int16_t)(s_artH + 20), C_CREAM, C_BG, 2, 8);
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
