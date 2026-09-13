#ifndef _M5_CARDPUTER_ADV_H
#define _M5_CARDPUTER_ADV_H

#define PIN_BUTTON_1 0

#define V1_DISPLAY

#define SDSPI_CS    12
#define SDSPI_MOSI  14
#define SDSPI_CLK   40
#define SDSPI_MISO  39

// TCA8418 keyboard (Cardputer Adv only — not the original 74HC138 matrix)
#define TCA8418_I2C_ADDR  0x34
#define TCA8418_SDA_PIN   8
#define TCA8418_SCL_PIN   9
#define TCA8418_INT_PIN   11

#endif
