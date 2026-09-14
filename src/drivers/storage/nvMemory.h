#ifndef _NVMEMORY_H_
#define _NVMEMORY_H_

// we only have one implementation right now and nothing to choose from.
#define NVMEM_SPIFFS

#include "../devices/device.h"
#include "storage.h"

// Handles load and store of user settings, except wifi credentials. Those are managed by the wifimanager.
class nvMemory
{
public: 
    nvMemory();
    ~nvMemory();
    bool saveConfig(TSettings* Settings);
    bool loadConfig(TSettings* Settings);
    bool deleteConfig();
    /// Sticky SPIFFS flag so Backspace-5s wipe re-enters the portal next boot
    /// even if an SD config.json is present (SD fallback still works without it).
    bool armForcePortal();
    bool consumeForcePortal();
    /// One-shot after portal save: ignore Enter/C/W/G0 and try STA first.
    /// Armed in NVS + RTC + SPIFFS so a Launcher SPIFFS remount/format cannot
    /// drop the latch (v1.1 /sta_first-only still bounced on dirt).
    bool armStaFirst();
    bool peekStaFirst();
    bool consumeStaFirst();
private:
    bool init(bool formatIfNeeded = true);
    bool Initialized_;
};

#ifndef NVMEM_SPIFFS
#error We need some kind of permanent storage implementation!
#endif //NVMEM_TYPE

#endif // _NVMEMORY_H_
