#ifdef M5_CARDPUTER_ADV

#include "tca8418Keyboard.h"

#include <Wire.h>

#include "drivers/devices/device.h"
#include "drivers/displays/display.h"
#include "wManager.h"

// TCA8418 registers (TI SLVSAL4)
#define TCA8418_REG_CFG          0x01
#define TCA8418_REG_INT_STAT     0x02
#define TCA8418_REG_KEY_LCK_EC   0x03
#define TCA8418_REG_KEY_EVENT_A  0x04
#define TCA8418_REG_KP_GPIO1     0x1D
#define TCA8418_REG_KP_GPIO2     0x1E
#define TCA8418_REG_KP_GPIO3     0x1F

#define TCA8418_CFG_KE_IEN  0x01
#define TCA8418_CFG_INT_CFG 0x10

#define KEY_BACKSPACE 0x2A
#define KEY_TAB       0x2B
#define KEY_ENTER     0x28
#define KEY_FN        0xFF
#define KEY_SHIFT     0x81
#define KEY_CTRL      0x80
#define KEY_ALT       0x82
#define KEY_OPT       0x00
// Arduino/HID-style arrows (uint8_t map — signed char would break 0xD9/0xFF compares).
#define KEY_RIGHT     0xD7
#define KEY_LEFT      0xD8
#define KEY_DOWN      0xD9
#define KEY_UP        0xDA

// Adv captured FIFO (kamrrillo / MultiMote): ↓=58 ←=54 ↑=57 →=64
#define ADV_RAW_UP    57
#define ADV_RAW_LEFT  54
#define ADV_RAW_DOWN  58
#define ADV_RAW_RIGHT 64

// Adv 4x14 value_first. Arrows are Fn-layer (HWbot): Fn+. = Down, Fn+; = Up.
// Bare ';' / '.' stay v1.2 next. uint8_t avoids signed-char KEY_FN compares.
static const uint8_t kKeyMap[4][14] = {
    {'`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', KEY_BACKSPACE},
    {KEY_TAB, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\\'},
    {KEY_FN, KEY_SHIFT, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', KEY_ENTER},
    {KEY_CTRL, KEY_OPT, KEY_ALT, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', ' '},
};

static bool g_available = false;
static bool g_wantsConfig = false;
static bool g_fn = false;
static bool g_resetHeld = false;
static uint32_t g_resetHoldStart = 0;
static uint32_t g_lastNavMs = 0;

static bool writeReg(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(TCA8418_I2C_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

static bool readReg(uint8_t reg, uint8_t *value)
{
    Wire.beginTransmission(TCA8418_I2C_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }
    if (Wire.requestFrom((int)TCA8418_I2C_ADDR, 1) != 1) {
        return false;
    }
    *value = (uint8_t)Wire.read();
    return true;
}

// TCA8418: keycode = 10*row + col + 1 (TI SLVSAL4). Adv is 7x8.
// Remap is M5Cardputer-UserDemo / xiaozhi CardputerADV (7x8 -> 4x14).
static bool mapRawToPhysical(uint8_t keycode, uint8_t *row, uint8_t *col)
{
    if (keycode < 1) {
        return false;
    }
    const uint8_t raw_row = (uint8_t)((keycode - 1) / 10); // 0..6
    const uint8_t raw_col = (uint8_t)((keycode - 1) % 10); // 0..7
    if (raw_row > 6 || raw_col > 7) {
        return false;
    }
    *col = (uint8_t)((raw_row * 2) + ((raw_col > 3) ? 1 : 0));
    *row = (uint8_t)((raw_col + 4) % 4);
    return *row < 4 && *col < 14;
}

static uint8_t advArrowFromRaw(uint8_t keycode)
{
    switch (keycode) {
    case ADV_RAW_DOWN:
        return KEY_DOWN;
    case ADV_RAW_UP:
        return KEY_UP;
    case ADV_RAW_LEFT:
        return KEY_LEFT;
    case ADV_RAW_RIGHT:
        return KEY_RIGHT;
    default:
        return 0;
    }
}

static void flushFifo()
{
    uint8_t ev = 0;
    for (int i = 0; i < 16; ++i) {
        if (!readReg(TCA8418_REG_KEY_EVENT_A, &ev) || ev == 0) {
            break;
        }
    }
    writeReg(TCA8418_REG_INT_STAT, 0x03);
}

static void dispatchKey(uint8_t key)
{
    const uint32_t now = millis();
    if ((now - g_lastNavMs) < 180) {
        return;
    }
    g_lastNavMs = now;

    // Bare ';' / '.' stay v1.2 next. KEY_DOWN (Fn+.) is also next (+1).
    if (key == KEY_DOWN || key == KEY_RIGHT || key == KEY_ENTER ||
        key == ' ' || key == 'n' || key == '.' || key == '/' || key == ';') {
        Serial.println(key == KEY_DOWN ? F("Cardputer KB: down / next screen")
                                       : F("Cardputer KB: next screen"));
        switchToNextScreen();
        return;
    }
    // Up / left / p / comma = prev. Short KEY_BACKSPACE tap is handled on release.
    if (key == KEY_UP || key == KEY_LEFT || key == 'p' || key == ',') {
        Serial.println(key == KEY_UP ? F("Cardputer KB: up / previous screen")
                                     : F("Cardputer KB: previous screen"));
        switchToPrevScreen();
        return;
    }
    if (key == 'r') {
        Serial.println(F("Cardputer KB: rotate"));
        alternateScreenRotation();
        return;
    }
    if (key == 'b' || key == KEY_TAB) {
        Serial.println(F("Cardputer KB: backlight"));
        alternateScreenState();
        return;
    }
    if (key >= '1' && key <= '4') {
        Serial.printf("Cardputer KB: screen %d\n", key - '1');
        switchToScreen(key - '1');
    }
}

static void handleEvent(uint8_t raw)
{
    const bool pressed = (raw & 0x80) != 0;
    const uint8_t keycode = raw & 0x7F;
    uint8_t row = 0xFF, col = 0xFF;
    uint8_t key = 0;
    if (mapRawToPhysical(keycode, &row, &col)) {
        key = kKeyMap[row][col];
    } else {
        // Field-captured Adv FIFO (↓=58) if the 7x8 remap ever misses.
        key = advArrowFromRaw(keycode);
        if (key == 0) {
            return;
        }
    }

    if (key == KEY_FN) {
        g_fn = pressed;
        return;
    }

    // KEY_BACKSPACE (HID 0x2A): hold 5s = reset config; short tap = prev.
    if (key == KEY_BACKSPACE) {
        if (pressed) {
            if (!g_resetHeld) {
                g_resetHeld = true;
                g_resetHoldStart = millis();
            }
        } else if (g_resetHeld) {
            g_resetHeld = false;
            Serial.println(F("Cardputer KB: previous screen"));
            switchToPrevScreen();
        }
        return;
    }

    if (!pressed) {
        return;
    }

    // Fn + backtick (printed ESC) toggles the backlight.
    if (g_fn && key == '`') {
        Serial.println(F("Cardputer KB: Fn+` backlight"));
        alternateScreenState();
        return;
    }

    // HWbot: Adv arrows are Fn-layer. No extra GPIO.
    // Fn+. = Down → next; Fn+; = Up → prev. Fn+, left / Fn+/ right optional.
    if (g_fn) {
        if (key == '.' || key == KEY_DOWN) {
            key = KEY_DOWN;
        } else if (key == ';' || key == KEY_UP) {
            key = KEY_UP;
        } else if (key == ',') {
            key = KEY_LEFT;
        } else if (key == '/') {
            key = KEY_RIGHT;
        }
    }

    dispatchKey(key);
}

bool cardputerKeyboardBegin()
{
    g_available = false;
    g_wantsConfig = false;
    g_fn = false;
    g_resetHeld = false;

    // Keep EXT SPI CS idle so the onboard SD (HSPI CS=12) is not contested.
    pinMode(EXT_TFT_CS, OUTPUT);
    digitalWrite(EXT_TFT_CS, HIGH);
    pinMode(TCA8418_INT_PIN, INPUT_PULLUP);

    Wire.begin(TCA8418_SDA_PIN, TCA8418_SCL_PIN);
    Wire.setClock(400000);
    delay(20);

    Wire.beginTransmission(TCA8418_I2C_ADDR);
    if (Wire.endTransmission() != 0) {
        Serial.println(F("Cardputer KB: TCA8418 not found at 0x34"));
        return false;
    }

    // 7 rows x 8 cols keypad, key-event IRQ, INT stays asserted until cleared.
    if (!writeReg(TCA8418_REG_KP_GPIO1, 0x7F) ||
        !writeReg(TCA8418_REG_KP_GPIO2, 0xFF) ||
        !writeReg(TCA8418_REG_KP_GPIO3, 0x00) ||
        !writeReg(TCA8418_REG_CFG, TCA8418_CFG_INT_CFG | TCA8418_CFG_KE_IEN)) {
        Serial.println(F("Cardputer KB: TCA8418 init write failed"));
        return false;
    }

    flushFifo();
    delay(30);

    // Drain any keys already down (boot-time config combo).
    uint8_t ev = 0;
    while (readReg(TCA8418_REG_KEY_EVENT_A, &ev) && ev != 0) {
        const bool pressed = (ev & 0x80) != 0;
        uint8_t row = 0xFF, col = 0xFF;
        if (pressed && mapRawToPhysical((uint8_t)(ev & 0x7F), &row, &col)) {
            const uint8_t key = kKeyMap[row][col];
            if (key == KEY_ENTER || key == 'c' || key == 'w') {
                g_wantsConfig = true;
            }
        }
    }
    writeReg(TCA8418_REG_INT_STAT, 0x03);

    // Also treat G0 (PIN_BUTTON_1) held at boot as "open portal" when used
    // together with a keyboard key — Enter/C/W already set the flag.
    g_available = true;
    Serial.println(g_wantsConfig
                       ? F("Cardputer KB: TCA8418 ready (config key held)")
                       : F("Cardputer KB: TCA8418 ready"));
    return true;
}

bool cardputerKeyboardAvailable()
{
    return g_available;
}

bool cardputerKeyboardWantsConfig()
{
    return g_wantsConfig;
}

void cardputerKeyboardTick()
{
    if (!g_available) {
        return;
    }

    if (g_resetHeld && (millis() - g_resetHoldStart) >= CARDPUTER_RESET_HOLD_MS) {
        g_resetHeld = false;
        Serial.println(F("Cardputer KB: hold KEY_BACKSPACE — reset configuration"));
        reset_configuration();
        return;
    }

    uint8_t ev = 0;
    int drained = 0;
    while (drained < 10 && readReg(TCA8418_REG_KEY_EVENT_A, &ev) && ev != 0) {
        handleEvent(ev);
        drained++;
    }
    if (drained > 0) {
        writeReg(TCA8418_REG_INT_STAT, 0x03);
    }
}

#endif // M5_CARDPUTER_ADV
