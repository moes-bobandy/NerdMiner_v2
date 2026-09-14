#include "ili9341Ext.h"

#ifdef NERDMINER_DUAL_SCREEN

#include <SPI.h>
#include <pgmspace.h>
#include <string.h>
#include "drivers/devices/device.h"

#if EXT_TFT_CS != 5 || EXT_TFT_DC != 6 || EXT_TFT_RST != 3
#error "Dirt contract v1: EXT ILI9341 CS=5 DC=6 RST=3"
#endif
#if EXT_TFT_SCK != 40 || EXT_TFT_MOSI != 14 || EXT_TFT_MISO != 39
#error "Dirt contract v1: EXT ILI9341 SCK=40 MOSI=14 (MISO=39 shared with SD)"
#endif
#if SDSPI_CS != 12 || SDSPI_MOSI != 14 || SDSPI_CLK != 40 || SDSPI_MISO != 39
#error "Dirt contract v1: SD HSPI CS=12 MOSI=14 CLK=40 MISO=39"
#endif

#define ILI9341_SWRESET 0x01
#define ILI9341_SLPOUT  0x11
#define ILI9341_DISPON  0x29
#define ILI9341_CASET   0x2A
#define ILI9341_PASET   0x2B
#define ILI9341_RAMWR   0x2C
#define ILI9341_MADCTL  0x36
#define ILI9341_PIXFMT  0x3A
#define ILI9341_RDDID   0x04
#define ILI9341_RDID4   0xD3
#define ILI9341_PWCTR1  0xC0
#define ILI9341_PWCTR2  0xC1
#define ILI9341_VMCTR1  0xC5
#define ILI9341_VMCTR2  0xC7
#define ILI9341_FRMCTR1 0xB1
#define ILI9341_DFUNCTR 0xB6
#define ILI9341_GAMMASET 0x26
#define ILI9341_GMCTRP1 0xE0
#define ILI9341_GMCTRN1 0xE1

#define ILI9341_MADCTL_MY  0x80
#define ILI9341_MADCTL_MX  0x40
#define ILI9341_MADCTL_MV  0x20
#define ILI9341_MADCTL_ML  0x10
#define ILI9341_MADCTL_BGR 0x08
#define ILI9341_MADCTL_MH  0x04

// Contract v2.5: boot-only MADCTL MV|BGR = 0x28 (no MX, no MY, no MH, no ML).
// Field: 0x28 L/R-good + sharp, ONLY upside-down. 0xAC / 0x38 traded L/R vs Y.
// Keep BGR. Write once in sendInit — do not rewrite MADCTL mid-run.
// Fix Y in software: reverse ROWS only (vertical). Do NOT reverse columns / X.
// Do NOT set MY/MH/ML.
// A/B override: -DEXT_TFT_MADCTL=  (rollbacks: 0xE8, 0xA8, 0x68, 0xAC, 0x38).
#ifndef EXT_TFT_MADCTL
#define EXT_TFT_MADCTL (ILI9341_MADCTL_MV | ILI9341_MADCTL_BGR)
#endif

#ifndef EXT_TFT_SW_FLIP_Y
#define EXT_TFT_SW_FLIP_Y 1
#endif

static inline int16_t extFlipY(int16_t y, int16_t h)
{
#if EXT_TFT_SW_FLIP_Y
    return (int16_t)(EXT_TFT_HEIGHT - y - h);
#else
    (void)h;
    return y;
#endif
}

#ifndef EXT_TFT_SPI_HZ
#define EXT_TFT_SPI_HZ 20000000
#endif

static SPIClass extSpi(HSPI);
static bool g_ready = false;
static uint32_t g_id = 0;

// Classic 5x7 ASCII 0x20-0x7F (column-major, LSB at top).
static const uint8_t kFont5x7[][5] PROGMEM = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // space
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
    {0x00, 0x00, 0x50, 0x30, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // @
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    {0x00, 0x7F, 0x41, 0x41, 0x00}, // [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // backslash
    {0x00, 0x41, 0x41, 0x7F, 0x00}, // ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // _
    {0x00, 0x01, 0x02, 0x04, 0x00}, // `
    {0x20, 0x54, 0x54, 0x54, 0x78}, // a
    {0x7F, 0x48, 0x44, 0x44, 0x38}, // b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // c
    {0x38, 0x44, 0x44, 0x48, 0x7F}, // d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // e
    {0x08, 0x7E, 0x09, 0x01, 0x02}, // f
    {0x08, 0x54, 0x54, 0x54, 0x3C}, // g
    {0x7F, 0x08, 0x04, 0x04, 0x78}, // h
    {0x00, 0x44, 0x7D, 0x40, 0x00}, // i
    {0x20, 0x40, 0x44, 0x3D, 0x00}, // j
    {0x7F, 0x10, 0x28, 0x44, 0x00}, // k
    {0x00, 0x41, 0x7F, 0x40, 0x00}, // l
    {0x7C, 0x04, 0x18, 0x04, 0x78}, // m
    {0x7C, 0x08, 0x04, 0x04, 0x78}, // n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // o
    {0x7C, 0x14, 0x14, 0x14, 0x08}, // p
    {0x08, 0x14, 0x14, 0x18, 0x7C}, // q
    {0x7C, 0x08, 0x04, 0x04, 0x08}, // r
    {0x48, 0x54, 0x54, 0x54, 0x20}, // s
    {0x04, 0x3F, 0x44, 0x40, 0x20}, // t
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, // u
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, // v
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, // w
    {0x44, 0x28, 0x10, 0x28, 0x44}, // x
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, // y
    {0x44, 0x64, 0x54, 0x4C, 0x44}, // z
    {0x00, 0x08, 0x36, 0x41, 0x00}, // {
    {0x00, 0x00, 0x7F, 0x00, 0x00}, // |
    {0x00, 0x41, 0x36, 0x08, 0x00}, // }
    {0x10, 0x08, 0x08, 0x10, 0x08}, // ~
    {0x00, 0x00, 0x00, 0x00, 0x00},
};

// Shared HSPI with SD: both chip-selects are GPIO-owned. Never attach EXT
// CS as the SPI hardware SS — that fights SD (GPIO12) on the same controller.
static inline void extCsIdle()
{
    pinMode(EXT_TFT_CS, OUTPUT);
    digitalWrite(EXT_TFT_CS, HIGH);
}

static inline void sdCsIdle()
{
    pinMode(SDSPI_CS, OUTPUT);
    digitalWrite(SDSPI_CS, HIGH);
}

static void beginTxn()
{
    sdCsIdle();
    extCsIdle();
    extSpi.beginTransaction(SPISettings(EXT_TFT_SPI_HZ, MSBFIRST, SPI_MODE0));
}

static void endTxn()
{
    extCsIdle();
    extSpi.endTransaction();
}

static void writeCommand(uint8_t cmd)
{
    digitalWrite(EXT_TFT_DC, LOW);
    digitalWrite(EXT_TFT_CS, LOW);
    extSpi.transfer(cmd);
    digitalWrite(EXT_TFT_CS, HIGH);
    digitalWrite(EXT_TFT_DC, HIGH);
}

static void writeData(uint8_t data)
{
    digitalWrite(EXT_TFT_CS, LOW);
    extSpi.transfer(data);
    digitalWrite(EXT_TFT_CS, HIGH);
}

static void setAddrWindow(int16_t x, int16_t y, int16_t w, int16_t h)
{
    // Logical compose space stays 320x240. Flip Y here so FillRect / text /
    // blit share one transform. Callers still pass unflipped y; row emitters
    // that write top-to-bottom must reverse source rows when SW_FLIP_Y=1.
    y = extFlipY(y, h);
    const int16_t x1 = (int16_t)(x + w - 1);
    const int16_t y1 = (int16_t)(y + h - 1);
    writeCommand(ILI9341_CASET);
    writeData((uint8_t)(x >> 8));
    writeData((uint8_t)(x & 0xFF));
    writeData((uint8_t)(x1 >> 8));
    writeData((uint8_t)(x1 & 0xFF));
    writeCommand(ILI9341_PASET);
    writeData((uint8_t)(y >> 8));
    writeData((uint8_t)(y & 0xFF));
    writeData((uint8_t)(y1 >> 8));
    writeData((uint8_t)(y1 & 0xFF));
    writeCommand(ILI9341_RAMWR);
}

// Burst GRAM fill. Byte-at-a-time transfer() of 320x240 (~153k clocks plus
// call overhead) is a visible top-down wipe on the porkchop panel.
static void pushColor(uint16_t color, uint32_t count)
{
    uint8_t buf[128];
    const uint8_t hi = (uint8_t)(color >> 8);
    const uint8_t lo = (uint8_t)(color & 0xFF);
    for (size_t i = 0; i < sizeof(buf); i += 2) {
        buf[i] = hi;
        buf[i + 1] = lo;
    }
    digitalWrite(EXT_TFT_CS, LOW);
    while (count) {
        uint32_t n = count;
        if (n > (sizeof(buf) / 2)) {
            n = sizeof(buf) / 2;
        }
        extSpi.writeBytes(buf, n * 2);
        count -= n;
    }
    digitalWrite(EXT_TFT_CS, HIGH);
}

static uint32_t readIdInternal()
{
    // Shared-bus MISO39. Porkchop pin list is write-only; this is best-effort.
    beginTxn();
    digitalWrite(EXT_TFT_DC, LOW);
    digitalWrite(EXT_TFT_CS, LOW);
    extSpi.transfer(ILI9341_RDID4);
    digitalWrite(EXT_TFT_DC, HIGH);
    (void)extSpi.transfer(0x00);
    const uint8_t b1 = extSpi.transfer(0x00);
    const uint8_t b2 = extSpi.transfer(0x00);
    const uint8_t b3 = extSpi.transfer(0x00);
    digitalWrite(EXT_TFT_CS, HIGH);
    endTxn();
    return ((uint32_t)b1 << 16) | ((uint32_t)b2 << 8) | b3;
}

static bool idLooksLikeIli9341(uint32_t id)
{
    const uint16_t low = (uint16_t)(id & 0xFFFF);
    return low == 0x9341 || low == 0x9340 || low == 0x9342;
}

static void sendInit()
{
    beginTxn();
    writeCommand(ILI9341_SWRESET);
    endTxn();
    delay(150);

    beginTxn();
    writeCommand(0xCF);
    writeData(0x00);
    writeData(0xC1);
    writeData(0x30);
    writeCommand(0xED);
    writeData(0x64);
    writeData(0x03);
    writeData(0x12);
    writeData(0x81);
    writeCommand(0xE8);
    writeData(0x85);
    writeData(0x00);
    writeData(0x78);
    writeCommand(0xCB);
    writeData(0x39);
    writeData(0x2C);
    writeData(0x00);
    writeData(0x34);
    writeData(0x02);
    writeCommand(0xF7);
    writeData(0x20);
    writeCommand(0xEA);
    writeData(0x00);
    writeData(0x00);
    writeCommand(ILI9341_PWCTR1);
    writeData(0x23);
    writeCommand(ILI9341_PWCTR2);
    writeData(0x10);
    writeCommand(ILI9341_VMCTR1);
    writeData(0x3E);
    writeData(0x28);
    writeCommand(ILI9341_VMCTR2);
    writeData(0x86);
    // Landscape 320x240. MADCTL once at boot. Rotate stays on INT.
    writeCommand(ILI9341_MADCTL);
    writeData(EXT_TFT_MADCTL);
    writeCommand(ILI9341_PIXFMT);
    writeData(0x55);
    writeCommand(ILI9341_FRMCTR1);
    writeData(0x00);
    writeData(0x13);
    writeCommand(ILI9341_DFUNCTR);
    writeData(0x08);
    writeData(0x82);
    writeData(0x27);
    writeCommand(0xF2);
    writeData(0x00);
    writeCommand(ILI9341_GAMMASET);
    writeData(0x01);
    writeCommand(ILI9341_GMCTRP1);
    writeData(0x0F);
    writeData(0x31);
    writeData(0x2B);
    writeData(0x0C);
    writeData(0x0E);
    writeData(0x08);
    writeData(0x4E);
    writeData(0xF1);
    writeData(0x37);
    writeData(0x07);
    writeData(0x10);
    writeData(0x03);
    writeData(0x0E);
    writeData(0x09);
    writeData(0x00);
    writeCommand(ILI9341_GMCTRN1);
    writeData(0x00);
    writeData(0x0E);
    writeData(0x14);
    writeData(0x03);
    writeData(0x11);
    writeData(0x07);
    writeData(0x31);
    writeData(0xC1);
    writeData(0x48);
    writeData(0x08);
    writeData(0x0F);
    writeData(0x0C);
    writeData(0x31);
    writeData(0x36);
    writeData(0x0F);
    writeCommand(ILI9341_SLPOUT);
    endTxn();
    delay(120);
    beginTxn();
    writeCommand(ILI9341_DISPON);
    endTxn();
}

static volatile int s_frame = 0;

void ili9341ExtBeginFrame()
{
    s_frame++;
}

void ili9341ExtEndFrame()
{
    if (s_frame > 0) {
        s_frame--;
    }
    extCsIdle();
}

void ili9341ExtQuiesce()
{
    // Only between complete frames. Raising CS mid-RAMWR leaves a partial
    // top-down black scan over still-valid GRAM (Dirt film-slide).
    while (s_frame > 0) {
        delay(1);
    }
    extCsIdle();
}

bool ili9341ExtReady()
{
    return g_ready;
}

uint32_t ili9341ExtId()
{
    return g_id;
}

bool ili9341ExtBegin()
{
    g_ready = false;
    g_id = 0;

#ifdef NERDMINER_DUAL_FORCE_INT
    Serial.println(F("EXT ILI9341: NERDMINER_DUAL_FORCE_INT — INT-only fallback"));
    return false;
#endif

    pinMode(EXT_TFT_CS, OUTPUT);
    pinMode(EXT_TFT_DC, OUTPUT);
    pinMode(EXT_TFT_RST, OUTPUT);
    digitalWrite(EXT_TFT_CS, HIGH);
    digitalWrite(EXT_TFT_DC, HIGH);

    sdCsIdle();
    extSpi.begin(EXT_TFT_SCK, EXT_TFT_MISO, EXT_TFT_MOSI, -1);

    digitalWrite(EXT_TFT_RST, LOW);
    delay(20);
    digitalWrite(EXT_TFT_RST, HIGH);
    delay(150);

    sendInit();
    g_id = readIdInternal();
    Serial.printf("EXT ILI9341: RDID4=0x%06X\n", (unsigned)g_id);

    const bool idOk = idLooksLikeIli9341(g_id);
#if defined(NERDMINER_DUAL_ASSUME_EXT) && NERDMINER_DUAL_ASSUME_EXT
    if (!idOk) {
        Serial.println(F("EXT ILI9341: no ID (write-only porkchop) — assuming present"));
    }
    g_ready = true;
#else
    if (!idOk) {
        Serial.println(F("EXT ILI9341: probe failed — INT-only fallback"));
        ili9341ExtQuiesce();
        return false;
    }
    g_ready = true;
#endif
    Serial.printf("EXT ILI9341: 320x240 ready MADCTL=0x%02X SW_FLIP_Y=%d (HSPI)\n",
                  (unsigned)(EXT_TFT_MADCTL), (int)EXT_TFT_SW_FLIP_Y);
    return g_ready;
}

void ili9341ExtFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    if (!g_ready || w <= 0 || h <= 0) {
        return;
    }
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > EXT_TFT_WIDTH) {
        w = (int16_t)(EXT_TFT_WIDTH - x);
    }
    if (y + h > EXT_TFT_HEIGHT) {
        h = (int16_t)(EXT_TFT_HEIGHT - y);
    }
    if (w <= 0 || h <= 0) {
        return;
    }
    beginTxn();
    setAddrWindow(x, y, w, h);
    pushColor(color, (uint32_t)w * (uint32_t)h);
    endTxn();
}

void ili9341ExtFillScreen(uint16_t color)
{
    ili9341ExtFillRect(0, 0, EXT_TFT_WIDTH, EXT_TFT_HEIGHT, color);
}

static void emitRgb565Swapped(const uint16_t *src, uint32_t count)
{
    uint8_t buf[128];
    uint32_t i = 0;
    while (i < count) {
        uint32_t n = count - i;
        if (n > (sizeof(buf) / 2)) {
            n = sizeof(buf) / 2;
        }
        for (uint32_t k = 0; k < n; ++k) {
            const uint16_t c = pgm_read_word(&src[i + k]);
            // TFT_eSPI setSwapBytes(true): send stored low byte first.
            buf[k * 2] = (uint8_t)(c & 0xFF);
            buf[k * 2 + 1] = (uint8_t)(c >> 8);
        }
        extSpi.writeBytes(buf, n * 2);
        i += n;
    }
}

void ili9341ExtPushImage(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *data)
{
    if (!g_ready || !data || w <= 0 || h <= 0) {
        return;
    }
    beginTxn();
    setAddrWindow(x, y, w, h);
    digitalWrite(EXT_TFT_CS, LOW);
#if EXT_TFT_SW_FLIP_Y
    for (int16_t row = (int16_t)(h - 1); row >= 0; --row) {
        emitRgb565Swapped(data + (int32_t)row * w, (uint32_t)w);
    }
#else
    emitRgb565Swapped(data, (uint32_t)w * (uint32_t)h);
#endif
    digitalWrite(EXT_TFT_CS, HIGH);
    endTxn();
}

void ili9341ExtPushImageScaled(int16_t x, int16_t y, int16_t dw, int16_t dh,
                               const uint16_t *data, int16_t sw, int16_t sh)
{
    if (!g_ready || !data || dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0) {
        return;
    }
    beginTxn();
    setAddrWindow(x, y, dw, dh);
    digitalWrite(EXT_TFT_CS, LOW);
    uint8_t buf[160];
    uint32_t bp = 0;
    // SW Y-flip: rows only (bottom→top). Columns stay left→right (dx 0..dw-1).
#if EXT_TFT_SW_FLIP_Y
    for (int16_t dy = (int16_t)(dh - 1); dy >= 0; --dy)
#else
    for (int16_t dy = 0; dy < dh; ++dy)
#endif
    {
        const int16_t sy = (int16_t)((int32_t)dy * sh / dh);
        const uint16_t *row = data + (int32_t)sy * sw;
        for (int16_t dx = 0; dx < dw; ++dx) {
            const int16_t sx = (int16_t)((int32_t)dx * sw / dw);
            const uint16_t c = pgm_read_word(&row[sx]);
            buf[bp++] = (uint8_t)(c & 0xFF);
            buf[bp++] = (uint8_t)(c >> 8);
            if (bp >= sizeof(buf)) {
                extSpi.writeBytes(buf, bp);
                bp = 0;
            }
        }
    }
    if (bp) {
        extSpi.writeBytes(buf, bp);
    }
    digitalWrite(EXT_TFT_CS, HIGH);
    endTxn();
}

void ili9341ExtHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
    ili9341ExtFillRect(x, y, w, 1, color);
}

void ili9341ExtVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
    ili9341ExtFillRect(x, y, 1, h, color);
}

void ili9341ExtDrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    ili9341ExtHLine(x, y, w, color);
    ili9341ExtHLine(x, (int16_t)(y + h - 1), w, color);
    ili9341ExtVLine(x, y, h, color);
    ili9341ExtVLine((int16_t)(x + w - 1), y, h, color);
}

int16_t ili9341ExtTextWidth(const char *text, uint8_t scale)
{
    if (!text || scale < 1) {
        return 0;
    }
    int16_t n = 0;
    while (*text++) {
        n++;
    }
    return (int16_t)(n * (5 + 1) * scale);
}

int16_t ili9341ExtDrawChar(int16_t x, int16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale)
{
    if (scale < 1) {
        scale = 1;
    }
    if (scale > 6) {
        scale = 6;
    }
    uint8_t idx = 0;
    if (c >= 0x20 && c <= 0x7F) {
        idx = (uint8_t)(c - 0x20);
    }
    uint8_t cols[5];
    memcpy_P(cols, kFont5x7[idx], 5);

    if (!g_ready) {
        return (int16_t)(6 * scale);
    }

    const int16_t gw = (int16_t)(6 * scale);
    const int16_t gh = (int16_t)(8 * scale);
    uint8_t line[6 * 6 * 2];
    beginTxn();
    setAddrWindow(x, y, gw, gh);
    digitalWrite(EXT_TFT_CS, LOW);
#if EXT_TFT_SW_FLIP_Y
    for (int row = 7; row >= 0; --row)
#else
    for (int row = 0; row < 8; ++row)
#endif
    {
        uint32_t p = 0;
        for (int col = 0; col < 6; ++col) {
            uint16_t color = bg;
            if (col < 5 && (cols[col] & (1 << row))) {
                color = fg;
            }
            for (int sx = 0; sx < scale; ++sx) {
                line[p++] = (uint8_t)(color >> 8);
                line[p++] = (uint8_t)(color & 0xFF);
            }
        }
        for (int sy = 0; sy < scale; ++sy) {
            extSpi.writeBytes(line, p);
        }
    }
    digitalWrite(EXT_TFT_CS, HIGH);
    endTxn();
    return gw;
}

int16_t ili9341ExtDrawText(int16_t x, int16_t y, const char *text, uint16_t fg, uint16_t bg, uint8_t scale)
{
    if (!text) {
        return 0;
    }
    int16_t cx = x;
    while (*text) {
        cx = (int16_t)(cx + ili9341ExtDrawChar(cx, y, *text++, fg, bg, scale));
    }
    return (int16_t)(cx - x);
}

int16_t ili9341ExtDrawTextRight(int16_t right, int16_t y, const char *text, uint16_t fg, uint16_t bg, uint8_t scale)
{
    const int16_t w = ili9341ExtTextWidth(text, scale);
    const int16_t x = (int16_t)(right - w);
    ili9341ExtDrawText(x, y, text, fg, bg, scale);
    return x;
}

static void fillSegH(int16_t x, int16_t y, int16_t w, int16_t t, uint16_t color)
{
    ili9341ExtFillRect(x, y, w, t, color);
}

static void fillSegV(int16_t x, int16_t y, int16_t t, int16_t h, uint16_t color)
{
    ili9341ExtFillRect(x, y, t, h, color);
}

void ili9341ExtDrawDigit7(int16_t x, int16_t y, char d, uint16_t on, uint16_t off, int16_t w, int16_t h)
{
    // segments: 0A 1B 2C 3D 4E 5F 6G
    static const uint8_t map[10] = {
        0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
    };
    uint8_t bits = 0;
    if (d >= '0' && d <= '9') {
        bits = map[d - '0'];
    } else if (d == '-') {
        bits = 0x40;
    } else if (d == ' ') {
        bits = 0;
    } else {
        bits = 0x40;
    }
    const int16_t t = (w < 10) ? 2 : 3;
    const int16_t mid = (int16_t)(y + (h / 2) - (t / 2));
    const int16_t inner = (int16_t)(w - 2 * t);
    const int16_t halfH = (int16_t)((h / 2) - t);
    fillSegH((int16_t)(x + t), y, inner, t, (bits & 0x01) ? on : off);                         // A
    fillSegV((int16_t)(x + w - t), (int16_t)(y + t), t, halfH, (bits & 0x02) ? on : off);      // B
    fillSegV((int16_t)(x + w - t), (int16_t)(mid + t), t, halfH, (bits & 0x04) ? on : off);    // C
    fillSegH((int16_t)(x + t), (int16_t)(y + h - t), inner, t, (bits & 0x08) ? on : off);      // D
    fillSegV(x, (int16_t)(mid + t), t, halfH, (bits & 0x10) ? on : off);                       // E
    fillSegV(x, (int16_t)(y + t), t, halfH, (bits & 0x20) ? on : off);                          // F
    fillSegH((int16_t)(x + t), mid, inner, t, (bits & 0x40) ? on : off);                        // G
}

void ili9341ExtDraw7Seg(int16_t x, int16_t y, const char *text, uint16_t on, uint16_t off, int16_t w, int16_t h, int16_t gap)
{
    if (!text) {
        return;
    }
    int16_t cx = x;
    while (*text) {
        const char c = *text++;
        if (c == '.') {
            ili9341ExtFillRect(cx, (int16_t)(y + h - 6), 5, 5, on);
            cx = (int16_t)(cx + 8);
            continue;
        }
        if (c == ':') {
            ili9341ExtFillRect((int16_t)(cx + 2), (int16_t)(y + h / 3), 5, 5, on);
            ili9341ExtFillRect((int16_t)(cx + 2), (int16_t)(y + 2 * h / 3), 5, 5, on);
            cx = (int16_t)(cx + 12);
            continue;
        }
        ili9341ExtDrawDigit7(cx, y, c, on, off, w, h);
        cx = (int16_t)(cx + w + gap);
    }
}

#endif // NERDMINER_DUAL_SCREEN
