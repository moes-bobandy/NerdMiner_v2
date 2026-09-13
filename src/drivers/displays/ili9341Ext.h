#ifndef ILI9341_EXT_H
#define ILI9341_EXT_H

#include <Arduino.h>
#include <stdint.h>

// Write-only-friendly ILI9341 on Cardputer Adv HSPI (porkchop).
// Pins: CS5 DC6 RST3 SCK40 MOSI14; optional MISO39 for ID probe.

bool ili9341ExtBegin();
void ili9341ExtQuiesce();
void ili9341ExtBeginFrame();
void ili9341ExtEndFrame();
bool ili9341ExtReady();
uint32_t ili9341ExtId();

void ili9341ExtFillScreen(uint16_t color);
void ili9341ExtFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void ili9341ExtDrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void ili9341ExtHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
void ili9341ExtVLine(int16_t x, int16_t y, int16_t h, uint16_t color);

// 5x7 GLCD, scale 1..6. Returns advance in pixels.
int16_t ili9341ExtDrawChar(int16_t x, int16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale);
int16_t ili9341ExtDrawText(int16_t x, int16_t y, const char *text, uint16_t fg, uint16_t bg, uint8_t scale);
int16_t ili9341ExtTextWidth(const char *text, uint8_t scale);

// Right-aligned text. Returns left edge x.
int16_t ili9341ExtDrawTextRight(int16_t right, int16_t y, const char *text, uint16_t fg, uint16_t bg, uint8_t scale);

// 7-segment style digits for hashrate (native 320x240, not a 240x135 blit).
void ili9341ExtDrawDigit7(int16_t x, int16_t y, char d, uint16_t on, uint16_t off, int16_t w, int16_t h);
void ili9341ExtDraw7Seg(int16_t x, int16_t y, const char *text, uint16_t on, uint16_t off, int16_t w, int16_t h, int16_t gap);

#endif // ILI9341_EXT_H
