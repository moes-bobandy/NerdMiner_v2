#ifndef DISPLAY_H
#define DISPLAY_H

#include "displayDriver.h"

extern DisplayDriver *currentDisplayDriver;

#ifdef NERDMINER_DUAL_SCREEN
#include "nerdMinerDual.h"
#else
inline DisplayDriver *nerd_nav() { return currentDisplayDriver; }
inline DisplayDriver *nerd_mining() { return currentDisplayDriver; }
#endif

void initDisplay();
void alternateScreenState();
void alternateScreenRotation();
void switchToNextScreen();
void switchToPrevScreen();
void switchToScreen(int index);
void resetToFirstScreen();
void drawLoadingScreen();
void drawSetupScreen();
void drawCurrentScreen(unsigned long mElapsed);
void animateCurrentScreen(unsigned long frame);
void doLedStuff(unsigned long frame);

#endif // DISPLAY_H
