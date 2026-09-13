#ifdef M5_CARDPUTER_ADV

#include "tca8418Keyboard.h"

#include <Wire.h>

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

// M5Cardputer 4x14 base-layer map (value_first).
static const char kKeyMap[4][14] = {
    {'`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', (char)KEY_BACKSPACE},
    {(char)KEY_TAB, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\\'},
    {(char)KEY_FN, (char)KEY_SHIFT, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', (char)KEY_ENTER},
    {(char)KEY_CTRL, (char)KEY_OPT, (char)KEY_ALT, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', ' '},
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

// Cardputer Adv electrical 7x8 -> physical 4x14 (Bruce / M5Cardputer / rust crate).
// TCA8418 keycode: bits 0-6 = 10*row + col + 1
static bool mapRawToPhysical(uint8_t keycode, uint8_t *row, uint8_t *col)
{
    const uint8_t u = keycode % 10; // 1..8
    const uint8_t t = keycode / 10; // 0..6
    if (u < 1 || u > 8 || t > 6) {
        return false;
    }
    const uint8_t u0 = (uint8_t)(u - 1);
    *row = u0 & 0x03;
    *col = (uint8_t)((t << 1) | (u0 >> 2));
    return *row < 4 && *col < 14;
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

static void dispatchChar(char key)
{
    const uint32_t now = millis();
    if ((now - g_lastNavMs) < 180) {
        return;
    }
    g_lastNavMs = now;

    if (key == KEY_ENTER || key == ' ' || key == 'n' || key == '.' || key == '/') {
        Serial.println(F("Cardputer KB: next screen"));
        switchToNextScreen();
        return;
    }
    if (key == 'p' || key == ',' || key == ';' || key == KEY_BACKSPACE) {
        Serial.println(F("Cardputer KB: previous screen"));
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
    if (!mapRawToPhysical(keycode, &row, &col)) {
        return;
    }

    const char key = kKeyMap[row][col];

    if (key == KEY_FN) {
        g_fn = pressed;
        return;
    }

    if (key == 'x') {
        if (pressed) {
            if (!g_resetHeld) {
                g_resetHeld = true;
                g_resetHoldStart = millis();
            }
        } else {
            g_resetHeld = false;
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

    // Fn + arrows printed on ; , . / — same as next/prev.
    dispatchChar(key);
}

bool cardputerKeyboardBegin()
{
    g_available = false;
    g_wantsConfig = false;
    g_fn = false;
    g_resetHeld = false;

    // Keep EXT SPI CS idle so the onboard SD (HSPI CS=12) is not contested.
    pinMode(5, OUTPUT);
    digitalWrite(5, HIGH);
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
            const char key = kKeyMap[row][col];
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
        Serial.println(F("Cardputer KB: hold X — reset configuration"));
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
