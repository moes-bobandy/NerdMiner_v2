#include "displayDriver.h"

#ifdef NERDMINER_DUAL_SCREEN

#include "ili9341Ext.h"
#include "nerdMinerDual.h"
#include "monitor.h"
#include "drivers/devices/device.h"

// Stock V1 palette only (tDisplayV1 / DigitalNumbers / 0xDEDB). Yellow custom HUD banned.
#define C_BG      0x0000

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

static void enterExtScreen(int id)
{
    if (s_extScreen != id) {
        ili9341ExtFillScreen(C_BG);
        s_extScreen = id;
    }
}

static void blitLiveStock(int screenIndex, unsigned long mElapsed)
{
    // Every 1 Hz monitor tick — not index-only. PR #12 blitStockOnce froze EXT
    // art on boot zeros. Do not re-compose with mElapsed==0 (poisons KH/s).
    if (mElapsed == 0) {
        mElapsed = 1000;
    }
    tDisplayV1ComposeCyclic(screenIndex, mElapsed, false);
    uint16_t sw = 0, sh = 0;
    const uint16_t *bits = tDisplayV1SpriteBits(&sw, &sh);
    if (!bits || sw == 0 || sh == 0) {
        return;
    }
    // Aspect-preserving scale (240x135 → 320x180). Center in 320x240 so no
    // goods-band / debug HUD is painted below the stock V1 chrome.
    int16_t dh = (int16_t)((int32_t)sh * W / sw);
    if (dh > H) {
        dh = H;
    }
    const int16_t y0 = (int16_t)((H - dh) / 2);
    ili9341ExtPushImageScaled(0, y0, W, dh, bits, (int16_t)sw, (int16_t)sh);
}

static void extMinerScreen(unsigned long mElapsed)
{
    mining_data data = getMiningData(mElapsed);
    Serial.printf(">>> EXT miner %s KH/s shares=%s hashes=%sK\n",
                  data.currentHashRate.c_str(), data.completedShares.c_str(),
                  data.totalKHashes.c_str());

    enterExtScreen(EXT_SCR_MINER);
    blitLiveStock(0, mElapsed);
}

static void extClockScreen(unsigned long mElapsed)
{
    clock_data data = getClockData(mElapsed);
    Serial.printf(">>> EXT clock %s rate=%s\n", data.currentTime.c_str(), data.currentHashRate.c_str());

    enterExtScreen(EXT_SCR_CLOCK);
    blitLiveStock(1, mElapsed);
}

static void extGlobalScreen(unsigned long mElapsed)
{
    coin_data data = getCoinData(mElapsed);
    Serial.printf(">>> EXT global %s height=%s\n", data.globalHashRate.c_str(), data.blockHeight.c_str());

    enterExtScreen(EXT_SCR_GLOBAL);
    blitLiveStock(2, mElapsed);
}

static void extPriceScreen(unsigned long mElapsed)
{
    clock_data data = getClockData(mElapsed);
    Serial.printf(">>> EXT price %s rate=%s\n", data.btcPrice.c_str(), data.currentHashRate.c_str());

    enterExtScreen(EXT_SCR_PRICE);
    blitLiveStock(3, mElapsed);
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
