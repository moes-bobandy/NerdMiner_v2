#define ESP_DRD_USE_SPIFFS true

// Include Libraries
//#include ".h"

#include <WiFi.h>

#include <WiFiManager.h>

#include "wManager.h"
#include "monitor.h"
#include "drivers/displays/display.h"
#include "drivers/storage/SDCard.h"
#include "drivers/storage/nvMemory.h"
#include "drivers/storage/storage.h"
#include "mining.h"
#include "timeconst.h"

#ifdef M5_CARDPUTER_ADV
#include "drivers/input/tca8418Keyboard.h"
#endif

#include <ArduinoJson.h>
#include <esp_flash.h>


// Flag for saving data
bool shouldSaveConfig = false;

// Variables to hold data from custom textboxes
TSettings Settings;

// Define WiFiManager Object
WiFiManager wm;
extern monitor_data mMonitor;

nvMemory nvMem;

extern SDCard SDCrd;

String readCustomAPName() {
    Serial.println("DEBUG: Attempting to read custom AP name from flash at 0x3F0000...");
    
    // Leer directamente desde flash
    const size_t DATA_SIZE = 128;
    uint8_t buffer[DATA_SIZE];
    memset(buffer, 0, DATA_SIZE); // Clear buffer
    
    // Leer desde 0x3F0000
    esp_err_t result = esp_flash_read(NULL, buffer, 0x3F0000, DATA_SIZE);
    if (result != ESP_OK) {
        Serial.printf("DEBUG: Flash read error: %s\n", esp_err_to_name(result));
        return "";
    }
    
    Serial.println("DEBUG: Successfully read from flash");
    String data = String((char*)buffer);
    
    // Debug: show raw data read
    Serial.printf("DEBUG: Raw flash data: '%s'\n", data.c_str());
    
    if (data.startsWith("WEBFLASHER_CONFIG:")) {
        Serial.println("DEBUG: Found WEBFLASHER_CONFIG marker");
        String jsonPart = data.substring(18); // Después del marcador "WEBFLASHER_CONFIG:"
        
        Serial.printf("DEBUG: JSON part: '%s'\n", jsonPart.c_str());
        
        DynamicJsonDocument doc(256);
        DeserializationError error = deserializeJson(doc, jsonPart);
        
        if (error == DeserializationError::Ok) {
            Serial.println("DEBUG: JSON parsed successfully");
            
            if (doc.containsKey("apname")) {
                String customAP = doc["apname"].as<String>();
                customAP.trim();
                
                if (customAP.length() > 0 && customAP.length() < 32) {
                    Serial.printf("✅ Custom AP name from webflasher: %s\n", customAP.c_str());
                    return customAP;
                } else {
                    Serial.printf("DEBUG: AP name invalid length: %d\n", customAP.length());
                }
            } else {
                Serial.println("DEBUG: 'apname' key not found in JSON");
            }
        } else {
            Serial.printf("DEBUG: JSON parse error: %s\n", error.c_str());
        }
    } else {
        Serial.println("DEBUG: WEBFLASHER_CONFIG marker not found - no custom config");
    }
    
    Serial.println("DEBUG: Using default AP name");
    return "";
}

void saveConfigCallback()
// Callback notifying us of the need to save configuration
{
    Serial.println("Should save config");
    shouldSaveConfig = true;    
    //wm.setConfigPortalBlocking(false);
}

/* void saveParamsCallback()
// Callback notifying us of the need to save configuration
{
    Serial.println("Should save config");
    shouldSaveConfig = true;
    nvMem.saveConfig(&Settings);
} */

void configModeCallback(WiFiManager* myWiFiManager)
// Called when config mode launched
{
    Serial.println("Entered Configuration Mode");
    drawSetupScreen();
    Serial.print("Config SSID: ");
    Serial.println(myWiFiManager->getConfigPortalSSID());

    Serial.print("Config IP Address: ");
    Serial.println(WiFi.softAPIP());
}

void reset_configuration()
{
    Serial.println("Erasing Config, restarting");
    nvMem.deleteConfig();
    nvMem.consumeStaFirst();
    nvMem.armForcePortal();
    resetStat();
    wm.resetSettings();
    WiFi.disconnect(true, true);
    delay(200);
    ESP.restart();
}

void init_WifiManager()
{
#ifdef MONITOR_SPEED
    Serial.begin(MONITOR_SPEED);
#else
    Serial.begin(115200);
#endif //MONITOR_SPEED
    //Serial.setTxTimeoutMs(10);
    
    // Check for custom AP name from flasher config, otherwise use default
    String customAPName = readCustomAPName();
    const char* apName = customAPName.length() > 0 ? customAPName.c_str() : DEFAULT_SSID;

    //Init pin 15 to eneble 5V external power (LilyGo bug)
#ifdef PIN_ENABLE5V
    pinMode(PIN_ENABLE5V, OUTPUT);
    digitalWrite(PIN_ENABLE5V, HIGH);
#endif

    // Change to true when testing to force configuration every time we run
    bool forceConfig = false;
    const bool wipePortal = nvMem.consumeForcePortal();
    const bool staFirst = nvMem.consumeStaFirst();

    if (wipePortal) {
        Serial.println(F("Config reset — start config portal"));
        forceConfig = true;
        wm.setBreakAfterConfig(true);
    }
#ifdef M5_CARDPUTER_ADV
    if (staFirst) {
        // Portal save one-shot: ignore leftover Enter/C/W/G0 and try STA first.
        cardputerKeyboardIgnoreForcePortal();
        Serial.println(F("STA-first boot — ignore Enter/C/W/G0 force-portal"));
    }
#endif
    if (!wipePortal && !staFirst) {
#if defined(PIN_BUTTON_2)
        // Check if button2 is pressed to enter configMode with actual configuration
        if (!digitalRead(PIN_BUTTON_2)) {
            Serial.println(F("Button pressed to force start config mode"));
            forceConfig = true;
            wm.setBreakAfterConfig(true); //Set to detect config edition and save
        }
#endif
#ifdef M5_CARDPUTER_ADV
        // Boot latch (begin / splash) OR keys still physically held.
        if (cardputerKeyboardWantsConfig() || cardputerKeyboardPollConfigHeld()) {
            Serial.println(F("Cardputer KB: Enter/C/W/G0 held — start config portal"));
            forceConfig = true;
            wm.setBreakAfterConfig(true);
        }
#endif
    }
    // Explicitly set WiFi mode
    WiFi.mode(WIFI_STA);

    if (!nvMem.loadConfig(&Settings))
    {
        //No config file on internal flash.
        // Boot-held portal / wipe flag must not be skipped by SD config.json.
        // STA-first after a portal save must not bounce to WAITING CONFIG just
        // because SPIFFS was reformatted (Launcher begin(true)); WiFi creds
        // live in NVS. SD fallback still runs when the user is not forcing
        // the portal and is not in the STA-first window.
        if (!forceConfig && !staFirst && SDCrd.loadConfigFile(&Settings))
        {
            //Config file on SD card.
            SDCrd.SD2nvMemory(&nvMem, &Settings); // reboot on success.          
        }
        else if (!forceConfig && staFirst && SDCrd.loadConfigFile(&Settings))
        {
            // Use SD pool/wallet in RAM; do not SD2nvMemory-reboot (re-latches keys).
            Serial.println(F("STA-first: using SD config in RAM, skip SPIFFS reboot"));
        }
        else if (!staFirst)
        {
            //No config file on SD card (or portal forced). Starting wifi config server.
            forceConfig = true;
        }
        else
        {
            Serial.println(F("STA-first: no SPIFFS/SD config — try STA anyway"));
        }
    };
    
    // Free the memory from SDCard class 
    SDCrd.terminate();
    
    // Reset settings (only for development)
    //wm.resetSettings();

    //Set dark theme
    //wm.setClass("invert"); // dark theme

    // Set config save notify callback
    wm.setSaveConfigCallback(saveConfigCallback);
    wm.setSaveParamsCallback(saveConfigCallback);

    // Set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
    wm.setAPCallback(configModeCallback);    

    //Advanced settings
    wm.setConfigPortalBlocking(false); //Hacemos que el portal no bloquee el firmware
    wm.setConnectTimeout(40); // how long to try to connect for before continuing
    wm.setConfigPortalTimeout(180); // auto close configportal after n seconds
    // wm.setCaptivePortalEnable(false); // disable captive portal redirection
    // wm.setAPClientCheck(true); // avoid timeout if client connected to softap
    //wm.setTimeout(120);
    //wm.setConfigPortalTimeout(120); //seconds

    // Custom elements

    // Text box (String) - 80 characters maximum
    WiFiManagerParameter pool_text_box("Poolurl", "Pool url", Settings.PoolAddress.c_str(), 80);

    // Need to convert numerical input to string to display the default value.
    char convertedValue[6];
    sprintf(convertedValue, "%d", Settings.PoolPort);

    // Text box (Number) - 7 characters maximum
    WiFiManagerParameter port_text_box_num("Poolport", "Pool port", convertedValue, 7);

    // Text box (String) - 80 characters maximum
    //WiFiManagerParameter password_text_box("Poolpassword", "Pool password (Optional)", Settings.PoolPassword, 80);

    // Text box (String) - 80 characters maximum
    WiFiManagerParameter addr_text_box("btcAddress", "Your BTC address", Settings.BtcWallet, 80);

  // Text box (Number) - 2 characters maximum
  char charZone[6];
  sprintf(charZone, "%d", Settings.Timezone);
  WiFiManagerParameter time_text_box_num("TimeZone", "TimeZone fromUTC (-12/+12)", charZone, 3);

  WiFiManagerParameter features_html("<hr><br><label style=\"font-weight: bold;margin-bottom: 25px;display: inline-block;\">Features</label>");

  char checkboxParams[24] = "type=\"checkbox\"";
  if (Settings.saveStats)
  {
    strcat(checkboxParams, " checked");
  }
  WiFiManagerParameter save_stats_to_nvs("SaveStatsToNVS", "Save mining statistics to flash memory.", "T", 2, checkboxParams, WFM_LABEL_AFTER);
  // Text box (String) - 80 characters maximum
  WiFiManagerParameter password_text_box("Poolpassword - Optional", "Pool password", Settings.PoolPassword, 80);

  // Add all defined parameters
  wm.addParameter(&pool_text_box);
  wm.addParameter(&port_text_box_num);
  wm.addParameter(&password_text_box);
  wm.addParameter(&addr_text_box);
  wm.addParameter(&time_text_box_num);
  wm.addParameter(&features_html);
  wm.addParameter(&save_stats_to_nvs);
  #if defined(ESP32_2432S028R) || defined(ESP32_2432S028_2USB)
  char checkboxParams2[24] = "type=\"checkbox\"";
  if (Settings.invertColors)
  {
    strcat(checkboxParams2, " checked");
  }
  WiFiManagerParameter invertColors("inverColors", "Invert Display Colors (if the colors looks weird)", "T", 2, checkboxParams2, WFM_LABEL_AFTER);
  wm.addParameter(&invertColors);
  #endif
  #if defined(ESP32_2432S028R) || defined(ESP32_2432S028_2USB)
    char brightnessConvValue[2];
    sprintf(brightnessConvValue, "%d", Settings.Brightness);
    // Text box (Number) - 3 characters maximum
    WiFiManagerParameter brightness_text_box_num("Brightness", "Screen backlight Duty Cycle (0-255)", brightnessConvValue, 3);
    wm.addParameter(&brightness_text_box_num);
  #endif

    auto saveFromPortal = [&]() {
        Settings.PoolAddress = pool_text_box.getValue();
        Settings.PoolPort = atoi(port_text_box_num.getValue());
        strncpy(Settings.PoolPassword, password_text_box.getValue(), sizeof(Settings.PoolPassword));
        strncpy(Settings.BtcWallet, addr_text_box.getValue(), sizeof(Settings.BtcWallet));
        Settings.Timezone = atoi(time_text_box_num.getValue());
        Settings.saveStats = (strncmp(save_stats_to_nvs.getValue(), "T", 1) == 0);
#if defined(ESP32_2432S028R) || defined(ESP32_2432S028_2USB)
        Settings.invertColors = (strncmp(invertColors.getValue(), "T", 1) == 0);
        Settings.Brightness = atoi(brightness_text_box_num.getValue());
#endif
        nvMem.saveConfig(&Settings);
    };

    // Contract v1.2: persist, clear sticky keys, arm STA-first (NVS+RTC+SPIFFS).
    // Do NOT ESP.restart() after Save — that is boot splash initScreen (Connecting
    // QR) then leftover keys / failed STA → setupModeScreen (WAITING CONFIG QR).
    // Dirt: those are TWO different QR screens. After Save never paint initScreen.
    // WL_CONNECTED → mining. Else quiet STA (status only); portal again only
    // after a real timeout, still on WAITING CONFIG — no Connecting QR flash.
    // /force_portal is wipe-only. Never arm it on connect fail.
    auto tryStaFirst = [&](bool paintConnectingQr) -> bool {
        mMonitor.NerdStatus = NM_Connecting;
        if (paintConnectingQr) {
            // Boot / first STA only. initScreen is a QR — banned after Save.
            drawLoadingScreen();
        }
        WiFi.mode(WIFI_STA);
        wm.setCaptivePortalEnable(true);
        wm.setConfigPortalBlocking(true);
        wm.setEnableConfigPortal(false);
        return wm.autoConnect(apName, DEFAULT_WIFIPW);
    };

    auto commitPortalSave = [&]() {
        Serial.println(F("Portal shouldSaveConfig — saving, no bounce restart"));
        saveFromPortal();
#ifdef M5_CARDPUTER_ADV
        cardputerKeyboardClearConfigLatch();
        cardputerKeyboardIgnoreForcePortal();
#endif
        nvMem.armStaFirst();
    };

    auto runConfigPortal = [&]() {
#ifdef M5_CARDPUTER_ADV
        cardputerKeyboardClearConfigLatch();
#endif
        wm.setConfigPortalBlocking(true);
        wm.setBreakAfterConfig(true);
        wm.setConfigPortalTimeout(0);
        for (;;) {
            shouldSaveConfig = false;
            mMonitor.NerdStatus = NM_waitingConfig;
            drawSetupScreen();
            const bool portalConnected = wm.startConfigPortal(apName, DEFAULT_WIFIPW);
            if (shouldSaveConfig) {
                commitPortalSave();
            }
            if (portalConnected || (WiFi.status() == WL_CONNECTED)) {
                Serial.println(F("Portal save — STA connected, skip both QRs, mine"));
                mMonitor.NerdStatus = NM_Connecting;
                WiFi.mode(WIFI_STA);
                return;
            }
            if (shouldSaveConfig) {
                Serial.println(F("Portal save — quiet STA (no initScreen QR), no instant bounce"));
#ifdef M5_CARDPUTER_ADV
                cardputerKeyboardIgnoreForcePortal();
#endif
                // paintConnectingQr=false: stay on setupModeScreen during STA.
                if (tryStaFirst(false)) {
                    return;
                }
                Serial.println(F("STA timeout after save — config portal again"));
                continue;
            }
            Serial.println(F("Config portal ended without save — re-enter WAITING CONFIG"));
        }
    };

    Serial.println("AllDone: ");
    if (forceConfig)
    {
        runConfigPortal();
    }
    else
    {
        // STA first: Connecting only. No setup QR until a real STA timeout.
        if (!tryStaFirst(true))
        {
            Serial.println("Failed to connect to configured WIFI, and hit timeout");
            if (shouldSaveConfig) {
                commitPortalSave();
            }
            runConfigPortal();
        }
    }
    
    //Conectado a la red Wifi
    if (WiFi.status() == WL_CONNECTED) {
        //tft.pushImage(0, 0, MinerWidth, MinerHeight, MinerScreen);
        Serial.println("");
        Serial.println("WiFi connected");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());


        // Lets deal with the user config values

        // Copy the string value
        Settings.PoolAddress = pool_text_box.getValue();
        //strncpy(Settings.PoolAddress, pool_text_box.getValue(), sizeof(Settings.PoolAddress));
        Serial.print("PoolString: ");
        Serial.println(Settings.PoolAddress);

        //Convert the number value
        Settings.PoolPort = atoi(port_text_box_num.getValue());
        Serial.print("portNumber: ");
        Serial.println(Settings.PoolPort);

        // Copy the string value
        strncpy(Settings.PoolPassword, password_text_box.getValue(), sizeof(Settings.PoolPassword));
        Serial.print("poolPassword: ");
        Serial.println(Settings.PoolPassword);

        // Copy the string value
        strncpy(Settings.BtcWallet, addr_text_box.getValue(), sizeof(Settings.BtcWallet));
        Serial.print("btcString: ");
        Serial.println(Settings.BtcWallet);

        //Convert the number value
        Settings.Timezone = atoi(time_text_box_num.getValue());
        Serial.print("TimeZone fromUTC: ");
        Serial.println(Settings.Timezone);

        #if defined(ESP32_2432S028R) || defined(ESP32_2432S028_2USB)
        Settings.invertColors = (strncmp(invertColors.getValue(), "T", 1) == 0);
        Serial.print("Invert Colors: ");
        Serial.println(Settings.invertColors);        
        #endif

        #if defined(ESP32_2432S028R) || defined(ESP32_2432S028_2USB)
        Settings.Brightness = atoi(brightness_text_box_num.getValue());
        Serial.print("Brightness: ");
        Serial.println(Settings.Brightness);
        #endif

    }

    // Lets deal with the user config values

    // Copy the string value
    Settings.PoolAddress = pool_text_box.getValue();
    //strncpy(Settings.PoolAddress, pool_text_box.getValue(), sizeof(Settings.PoolAddress));
    Serial.print("PoolString: ");
    Serial.println(Settings.PoolAddress);

    //Convert the number value
    Settings.PoolPort = atoi(port_text_box_num.getValue());
    Serial.print("portNumber: ");
    Serial.println(Settings.PoolPort);

    // Copy the string value
    strncpy(Settings.PoolPassword, password_text_box.getValue(), sizeof(Settings.PoolPassword));
    Serial.print("poolPassword: ");
    Serial.println(Settings.PoolPassword);

    // Copy the string value
    strncpy(Settings.BtcWallet, addr_text_box.getValue(), sizeof(Settings.BtcWallet));
    Serial.print("btcString: ");
    Serial.println(Settings.BtcWallet);

    //Convert the number value
    Settings.Timezone = atoi(time_text_box_num.getValue());
    Serial.print("TimeZone fromUTC: ");
    Serial.println(Settings.Timezone);

    #ifdef ESP32_2432S028R
    Settings.invertColors = (strncmp(invertColors.getValue(), "T", 1) == 0);
    Serial.print("Invert Colors: ");
    Serial.println(Settings.invertColors);
    #endif

    // Save the custom parameters to FS
    if (shouldSaveConfig)
    {
        nvMem.saveConfig(&Settings);
        #if defined(ESP32_2432S028R) || defined(ESP32_2432S028_2USB)
         if (Settings.invertColors) ESP.restart();                
        #endif
        #if defined(ESP32_2432S028R) || defined(ESP32_2432S028_2USB)
        if (Settings.Brightness != 250) ESP.restart();
        #endif
    }
}

//----------------- MAIN PROCESS WIFI MANAGER --------------
int oldStatus = 0;

void wifiManagerProcess() {

    wm.process(); // avoid delays() in loop when non-blocking and other long running code

    int newStatus = WiFi.status();
    if (newStatus != oldStatus) {
        if (newStatus == WL_CONNECTED) {
            Serial.println("CONNECTED - Current ip: " + WiFi.localIP().toString());
        } else {
            Serial.print("[Error] - current status: ");
            Serial.println(newStatus);
        }
        oldStatus = newStatus;
    }
}
