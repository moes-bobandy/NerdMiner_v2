#ifndef _WMANAGER_H
#define _WMANAGER_H

#include "drivers/storage/nvMemory.h"

extern nvMemory nvMem;

void init_WifiManager();
void wifiManagerProcess();
void reset_configuration();

#endif // _WMANAGER_H
