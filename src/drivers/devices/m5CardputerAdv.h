#ifndef _M5_CARDPUTER_ADV_H
#define _M5_CARDPUTER_ADV_H

#define PIN_BUTTON_1 0

#define V1_DISPLAY

#define SDSPI_CS    12
#define SDSPI_MOSI  14
#define SDSPI_CLK   40
#define SDSPI_MISO  39

// Porkchop EXT ILI9341 on the same HSPI bus as SD (MOSI14/CLK40/MISO39).
// Stock Adv firmware never talks to this panel but holds CS idle HIGH so a
// connected (or floating) EXT chip-select cannot steal SD traffic.
// Dual env (NERDMINER_DUAL_SCREEN): native 320x240 mining surface.
// Cap LoRa / Hydra Units use G5/G3/G6 — mutually exclusive with EXT.
#define EXT_TFT_CS    5
#define EXT_TFT_DC    6
#define EXT_TFT_RST   3
#define EXT_TFT_SCK   SDSPI_CLK
#define EXT_TFT_MOSI  SDSPI_MOSI
#define EXT_TFT_MISO  SDSPI_MISO
#define EXT_TFT_WIDTH  320
#define EXT_TFT_HEIGHT 240

// TCA8418 keyboard (Cardputer Adv only — not the original 74HC138 matrix)
#define TCA8418_I2C_ADDR  0x34
#define TCA8418_SDA_PIN   8
#define TCA8418_SCL_PIN   9
#define TCA8418_INT_PIN   11

#endif
