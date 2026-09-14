#include "nvMemory.h"

#ifdef NVMEM_SPIFFS

#include <SPIFFS.h>
#include <FS.h>
#include <ArduinoJson.h>

#include "../devices/device.h"
#include "storage.h"

#include <Preferences.h>
#include <esp_system.h>

nvMemory::nvMemory() : Initialized_(false){};

nvMemory::~nvMemory()
{
    if (Initialized_)
        SPIFFS.end();
};

/// @brief Save settings to config file on SPIFFS
/// @param TSettings* Settings to be saved.
/// @return true on success
bool nvMemory::saveConfig(TSettings* Settings)
{
    if (init())
    {
        // Save Config in JSON format
        Serial.println(F("SPIFS: Saving configuration."));

        // Create a JSON document
        StaticJsonDocument<512> json;
        json[JSON_SPIFFS_KEY_POOLURL] = Settings->PoolAddress;
        json[JSON_SPIFFS_KEY_POOLPORT] = Settings->PoolPort;
        json[JSON_SPIFFS_KEY_POOLPASS] = Settings->PoolPassword;
        json[JSON_SPIFFS_KEY_WALLETID] = Settings->BtcWallet;
        json[JSON_SPIFFS_KEY_TIMEZONE] = Settings->Timezone;
        json[JSON_SPIFFS_KEY_STATS2NV] = Settings->saveStats;
        json[JSON_SPIFFS_KEY_INVCOLOR] = Settings->invertColors;
        json[JSON_SPIFFS_KEY_BRIGHTNESS] = Settings->Brightness;

        // Open config file
        File configFile = SPIFFS.open(JSON_CONFIG_FILE, "w");
        if (!configFile)
        {
            // Error, file did not open
            Serial.println("SPIFS: Failed to open config file for writing");
            return false;
        }

        // Serialize JSON data to write to file
        serializeJsonPretty(json, Serial);
        Serial.print('\n');
        if (serializeJson(json, configFile) == 0)
        {
            // Error writing file
            Serial.println(F("SPIFS: Failed to write to file"));
            return false;
        }
        // Close file
        configFile.close();
        return true;
    };
    return false;
}

/// @brief Load settings from config file located in SPIFFS.
/// @param TSettings* Struct to update with new settings.
/// @return true on success
bool nvMemory::loadConfig(TSettings* Settings)
{
    // Uncomment if we need to format filesystem
    // SPIFFS.format();

    // Load existing configuration file
    // Read configuration from FS json

    if (init())
    {
        if (SPIFFS.exists(JSON_CONFIG_FILE))
        {
            // The file exists, reading and loading
            File configFile = SPIFFS.open(JSON_CONFIG_FILE, "r");
            if (configFile)
            {
                Serial.println("SPIFS: Loading config file");
                StaticJsonDocument<512> json;
                DeserializationError error = deserializeJson(json, configFile);
                configFile.close();
                serializeJsonPretty(json, Serial);
                Serial.print('\n');
                if (!error)
                {
                    Settings->PoolAddress = json[JSON_SPIFFS_KEY_POOLURL] | Settings->PoolAddress;
                    strcpy(Settings->PoolPassword, json[JSON_SPIFFS_KEY_POOLPASS] | Settings->PoolPassword);
                    strcpy(Settings->BtcWallet, json[JSON_SPIFFS_KEY_WALLETID] | Settings->BtcWallet);
                    if (json.containsKey(JSON_SPIFFS_KEY_POOLPORT))
                        Settings->PoolPort = json[JSON_SPIFFS_KEY_POOLPORT].as<int>();
                    if (json.containsKey(JSON_SPIFFS_KEY_TIMEZONE))
                        Settings->Timezone = json[JSON_SPIFFS_KEY_TIMEZONE].as<int>();
                    if (json.containsKey(JSON_SPIFFS_KEY_STATS2NV))
                        Settings->saveStats = json[JSON_SPIFFS_KEY_STATS2NV].as<bool>();
                    if (json.containsKey(JSON_SPIFFS_KEY_INVCOLOR)) {
                        Settings->invertColors = json[JSON_SPIFFS_KEY_INVCOLOR].as<bool>();
                    } else {
                        Settings->invertColors = false;
                    }
                    if (json.containsKey(JSON_SPIFFS_KEY_BRIGHTNESS)) {
                        Settings->Brightness = json[JSON_SPIFFS_KEY_BRIGHTNESS].as<int>();
                    } else {
                        Settings->Brightness = 250;
                    }
                    return true;
                }
                else
                {
                    // Error loading JSON data
                    Serial.println("SPIFS: Error parsing config file!");
                }
            }
            else
            {
                Serial.println("SPIFS: Error opening config file!");
            }
        }
        else
        {
            Serial.println("SPIFS: No config file available!");
        }
    }
    return false;
}

#define FORCE_PORTAL_FILE "/force_portal"
#define STA_FIRST_FILE "/sta_first"
#define STA_FIRST_NVS_NS "nerdminer"
#define STA_FIRST_NVS_KEY "sta_first"
#define STA_FIRST_RTC_MAGIC 0x5A1F15A1u

// RTC_NOINIT survives ESP.restart() (the leftover-key window). NVS survives
// that plus power-cycle until consume. SPIFFS /sta_first is best-effort:
// Launcher may fail SPIFFS.begin(false) and begin(true) formats the partition.
static RTC_NOINIT_ATTR uint32_t s_staFirstRtc;

static bool staFirstResetKeepsRtc()
{
    switch (esp_reset_reason()) {
    case ESP_RST_SW:
    case ESP_RST_PANIC:
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
        return true;
    default:
        return false;
    }
}

static bool staFirstRtcPeek()
{
    return staFirstResetKeepsRtc() && (s_staFirstRtc == STA_FIRST_RTC_MAGIC);
}

static void staFirstRtcArm()
{
    s_staFirstRtc = STA_FIRST_RTC_MAGIC;
}

static void staFirstRtcClear()
{
    s_staFirstRtc = 0;
}

static bool nvsPeekStaFirst()
{
    Preferences prefs;
    if (!prefs.begin(STA_FIRST_NVS_NS, true)) {
        return false;
    }
    const bool armed = prefs.getBool(STA_FIRST_NVS_KEY, false);
    prefs.end();
    return armed;
}

static bool nvsArmStaFirst()
{
    Preferences prefs;
    if (!prefs.begin(STA_FIRST_NVS_NS, false)) {
        Serial.println("NVS: Failed to arm sta-first flag");
        return false;
    }
    const bool ok = prefs.putBool(STA_FIRST_NVS_KEY, true) > 0;
    prefs.end();
    if (ok) {
        Serial.println("NVS: STA-first on next boot");
    }
    return ok;
}

static bool nvsConsumeStaFirst()
{
    Preferences prefs;
    if (!prefs.begin(STA_FIRST_NVS_NS, false)) {
        return false;
    }
    const bool armed = prefs.getBool(STA_FIRST_NVS_KEY, false);
    if (armed) {
        prefs.remove(STA_FIRST_NVS_KEY);
        Serial.println("NVS: Consumed sta-first flag");
    }
    prefs.end();
    return armed;
}

/// @brief Delete config file from SPIFFS
/// @return true on successs
bool nvMemory::deleteConfig()
{
    if (!init()) {
        Serial.println("SPIFS: Erasing config file failed (not mounted)");
        return false;
    }
    Serial.println("SPIFS: Erasing config file..");
    if (!SPIFFS.exists(JSON_CONFIG_FILE)) {
        return true;
    }
    return SPIFFS.remove(JSON_CONFIG_FILE); //Borramos fichero
}

bool nvMemory::armForcePortal()
{
    if (!init()) {
        return false;
    }
    File f = SPIFFS.open(FORCE_PORTAL_FILE, "w");
    if (!f) {
        Serial.println("SPIFS: Failed to arm force-portal flag");
        return false;
    }
    f.print("1");
    f.flush();
    f.close();
    Serial.println("SPIFS: Force portal on next boot");
    return true;
}

bool nvMemory::consumeForcePortal()
{
    if (!init()) {
        return false;
    }
    if (!SPIFFS.exists(FORCE_PORTAL_FILE)) {
        return false;
    }
    SPIFFS.remove(FORCE_PORTAL_FILE);
    Serial.println("SPIFS: Consumed force-portal flag");
    return true;
}

bool nvMemory::armStaFirst()
{
    // NVS + RTC first: these survive a SPIFFS remount/format on the next boot.
    staFirstRtcArm();
    bool ok = nvsArmStaFirst();
    if (init(true)) {
        File f = SPIFFS.open(STA_FIRST_FILE, "w");
        if (!f) {
            Serial.println("SPIFS: Failed to arm sta-first flag");
        } else {
            f.print("1");
            f.flush();
            f.close();
            Serial.println("SPIFS: STA-first on next boot");
            ok = true;
        }
    }
    Serial.println(ok ? "STA-first armed (NVS/RTC/SPIFFS)"
                      : "STA-first: NVS/SPIFFS write failed; RTC latch still armed");
    return true;
}

bool nvMemory::peekStaFirst()
{
    if (staFirstRtcPeek()) {
        Serial.println("STA-first peek: RTC");
        return true;
    }
    if (nvsPeekStaFirst()) {
        Serial.println("STA-first peek: NVS");
        return true;
    }
    // Do not format SPIFFS just to look for a flag — begin(true) would wipe it.
    if (!init(false)) {
        return false;
    }
    return SPIFFS.exists(STA_FIRST_FILE);
}

bool nvMemory::consumeStaFirst()
{
    const bool rtc = staFirstRtcPeek() || (s_staFirstRtc == STA_FIRST_RTC_MAGIC);
    staFirstRtcClear();
    const bool nvs = nvsConsumeStaFirst();
    bool spiffs = false;
    if (init(false) && SPIFFS.exists(STA_FIRST_FILE)) {
        SPIFFS.remove(STA_FIRST_FILE);
        Serial.println("SPIFS: Consumed sta-first flag");
        spiffs = true;
    }
    return rtc || nvs || spiffs;
}

/// @brief Prepare and mount SPIFFS
/// @param formatIfNeeded If mount fails, format (wipes /sta_first and config).
/// @return true on success
bool nvMemory::init(bool formatIfNeeded)
{
    if (!Initialized_)
    {
        Serial.println("SPIFS: Mounting File System...");
        Initialized_ = SPIFFS.begin(false);
        if (!Initialized_ && formatIfNeeded) {
            // First-boot / Launcher LittleFS leftover. Formatting drops flags
            // that were only on SPIFFS — NVS/RTC sta-first must still win.
            Serial.println("SPIFS: Mount failed, formatting");
            Initialized_ = SPIFFS.begin(true);
        }
        Initialized_ ? Serial.println("SPIFS: Mounted") : Serial.println("SPIFS: Mounting failed.");
    }
    else
    {
        Serial.println("SPIFS: Already Mounted");
    }
    return Initialized_;
};

#else

nvMemory::nvMemory() {}
nvMemory::~nvMemory() {}
bool nvMemory::saveConfig(TSettings* Settings) { return false; }
bool nvMemory::loadConfig(TSettings* Settings) { return false; }
bool nvMemory::deleteConfig() { return false; }
bool nvMemory::armForcePortal() { return false; }
bool nvMemory::consumeForcePortal() { return false; }
bool nvMemory::armStaFirst() { return false; }
bool nvMemory::peekStaFirst() { return false; }
bool nvMemory::consumeStaFirst() { return false; }
bool nvMemory::init(bool formatIfNeeded) { (void)formatIfNeeded; return false; }


#endif //NVMEM_TYPE