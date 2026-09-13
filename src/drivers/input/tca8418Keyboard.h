#ifndef TCA8418_KEYBOARD_H
#define TCA8418_KEYBOARD_H

#include <Arduino.h>

#ifdef M5_CARDPUTER_ADV

// Cardputer Adv TCA8418 (I2C keyboard controller)
#ifndef TCA8418_I2C_ADDR
#define TCA8418_I2C_ADDR 0x34
#endif
#ifndef TCA8418_SDA_PIN
#define TCA8418_SDA_PIN 8
#endif
#ifndef TCA8418_SCL_PIN
#define TCA8418_SCL_PIN 9
#endif
#ifndef TCA8418_INT_PIN
#define TCA8418_INT_PIN 11
#endif

// Hold X (or Fn+X) this long to wipe config, matching the GPIO long-press.
#ifndef CARDPUTER_RESET_HOLD_MS
#define CARDPUTER_RESET_HOLD_MS 5000
#endif

/// Init I2C + TCA8418 matrix. Safe to call once from setup().
bool cardputerKeyboardBegin();

/// True after a successful begin() (chip ACK at 0x34).
bool cardputerKeyboardAvailable();

/// True if Enter or C was held during begin() — open WiFi config portal.
bool cardputerKeyboardWantsConfig();

/// Poll FIFO and dispatch UI actions (next/prev screen, rotate, backlight, reset).
void cardputerKeyboardTick();

#endif // M5_CARDPUTER_ADV

#endif // TCA8418_KEYBOARD_H
