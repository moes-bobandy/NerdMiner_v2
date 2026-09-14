#ifndef NERD_MINER_DUAL_H
#define NERD_MINER_DUAL_H

#include <stdint.h>
#include "displayDriver.h"

// Dirt-approved dual routing contract v1
//   nerd_nav()     — INT ST7789 (Setup215 / tDisplayV1Driver)
//   nerd_mining()  — EXT ILI9341 320x240 when present, else INT fallback
//   BL / rotate stay on INT. Loading / setup / portal stay on INT.

DisplayDriver *nerd_nav();
DisplayDriver *nerd_mining();
bool nerd_ext_available();

// Hold EXT CS HIGH (GPIO5). Safe before EXT init and from SD ctor.
// Must not run mid-frame (waits until ili9341ExtEndFrame).
void nerd_quiesce_ext();
void nerd_ext_begin_frame();
void nerd_ext_end_frame();

class TFT_eSprite;
void tDisplayV1PushStockChrome(TFT_eSprite *spr, int screenIndex);
void tDisplayV1StockFrame(int screenIndex, const uint16_t **bits, uint16_t *w, uint16_t *h);
// Compose stock V1 (MinerScreen + DigitalNumbers / 0xDEDB) into the INT sprite.
// pushToInt=false leaves ST7789 unchanged — EXT scale-blits the buffer.
void tDisplayV1ComposeCyclic(int screenIndex, unsigned long mElapsed, bool pushToInt);
const uint16_t *tDisplayV1SpriteBits(uint16_t *w, uint16_t *h);
// Tiny 1 Hz INT pulse (WIFI/CONN/HASH + uptime). Not a full V1 goods paint.
void tDisplayV1PaintLivePulse(unsigned long mElapsed);

void nerd_dual_init();
void nerd_draw_int_nav_hud(int screenIndex, unsigned long mElapsed);
void nerd_draw_int_live_pulse(unsigned long mElapsed);
// Keyboard / G0 mark dirty; 1 Hz drawCurrentScreen passes live mElapsed.
// Never force mElapsed==0 (frozen zeros / poisoned KH/s averager).
void nerd_mark_int_nav_dirty();
void nerd_poll_int_nav();

#if defined(NERDMINER_DUAL_SCREEN) && (defined(HAS_LORA) || defined(M5_MODULE_LORA) || defined(HYDRA) || defined(M5_MODULE_HYDRA))
#error "Cap LoRa/Hydra Units use G5/G3/G6 and are mutually exclusive with the EXT ILI9341 porkchop"
#endif

#endif // NERD_MINER_DUAL_H
