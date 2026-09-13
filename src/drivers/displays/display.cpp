#include "display.h"

#ifdef NO_DISPLAY
DisplayDriver *currentDisplayDriver = &noDisplayDriver;
#endif

#ifdef M5STACK_DISPLAY
DisplayDriver *currentDisplayDriver = &m5stackDisplayDriver;
#endif

#ifdef WT32_DISPLAY
DisplayDriver *currentDisplayDriver = &wt32DisplayDriver;
#endif

#ifdef LED_DISPLAY
DisplayDriver *currentDisplayDriver = &ledDisplayDriver;
#endif

#ifdef OLED_042_DISPLAY
DisplayDriver *currentDisplayDriver = &oled042DisplayDriver;
#endif

#ifdef T_DISPLAY
DisplayDriver *currentDisplayDriver = &tDisplayDriver;
#endif

#ifdef AMOLED_DISPLAY
DisplayDriver *currentDisplayDriver = &amoledDisplayDriver;
#endif

#ifdef DONGLE_DISPLAY
DisplayDriver *currentDisplayDriver = &dongleDisplayDriver;
#endif

#ifdef ESP32_2432S028R
DisplayDriver *currentDisplayDriver = &esp32_2432S028RDriver;
#endif

#ifdef ESP32_2432S028_2USB
DisplayDriver *currentDisplayDriver = &esp32_2432S028RDriver;
#endif

#ifdef T_QT_DISPLAY
DisplayDriver *currentDisplayDriver = &t_qtDisplayDriver;
#endif

#ifdef V1_DISPLAY
DisplayDriver *currentDisplayDriver = &tDisplayV1Driver;
#endif

#ifdef NERDMINER_DUAL_SCREEN
#include "nerdMinerDual.h"
#endif

#ifdef M5STICKC_DISPLAY
DisplayDriver *currentDisplayDriver = &m5stickCDriver;
#endif

#ifdef M5STICKCPLUS_DISPLAY
DisplayDriver *currentDisplayDriver = &m5stickCPlusDriver;
#endif

#ifdef T_HMI_DISPLAY
DisplayDriver *currentDisplayDriver = &t_hmiDisplayDriver;
#endif

#ifdef ST7735S_DISPLAY
DisplayDriver *currentDisplayDriver = &sp_kcDisplayDriver;
#endif

#ifdef OLED_SSD1306_128X64_DISPLAY
DisplayDriver *currentDisplayDriver = &ssd1306DisplayDriver;
#endif


// Initialize the display
void initDisplay()
{
#ifdef NERDMINER_DUAL_SCREEN
  nerd_dual_init();
#else
  currentDisplayDriver->initDisplay();
#endif
}

// Alternate screen state
void alternateScreenState()
{
  nerd_nav()->alternateScreenState();
}

// Alternate screen rotation
void alternateScreenRotation()
{
  nerd_nav()->alternateScreenRotation();
}

// Draw the loading screen
void drawLoadingScreen()
{
  nerd_nav()->loadingScreen();
}

// Draw the setup screen
void drawSetupScreen()
{
  nerd_nav()->setupScreen();
}

// Reset the current cyclic screen to the first one
void resetToFirstScreen()
{
  nerd_nav()->current_cyclic_screen = 0;
}

// Switches to the next cyclic screen without drawing it
void switchToNextScreen()
{
  DisplayDriver *nav = nerd_nav();
  nav->current_cyclic_screen = (nav->current_cyclic_screen + 1) % nav->num_cyclic_screens;
}

// Switches to the previous cyclic screen without drawing it
void switchToPrevScreen()
{
  DisplayDriver *nav = nerd_nav();
  if (nav->num_cyclic_screens <= 0) {
    return;
  }
  nav->current_cyclic_screen =
      (nav->current_cyclic_screen + nav->num_cyclic_screens - 1) %
      nav->num_cyclic_screens;
}

// Jump to a cyclic screen by index (0-based). Out-of-range values are ignored.
void switchToScreen(int index)
{
  DisplayDriver *nav = nerd_nav();
  if (index < 0 || index >= nav->num_cyclic_screens) {
    return;
  }
  nav->current_cyclic_screen = index;
}

// Draw the current cyclic screen
void drawCurrentScreen(unsigned long mElapsed)
{
  DisplayDriver *nav = nerd_nav();
  DisplayDriver *mining = nerd_mining();
  int idx = nav->current_cyclic_screen;
  if (mining->num_cyclic_screens <= 0) {
    return;
  }
  if (idx < 0 || idx >= mining->num_cyclic_screens) {
    idx = 0;
  }
#ifdef NERDMINER_DUAL_SCREEN
  nerd_ext_begin_frame();
#endif
  mining->cyclic_screens[idx](mElapsed);
#ifdef NERDMINER_DUAL_SCREEN
  nerd_ext_end_frame();
  if (nerd_ext_available()) {
    nerd_draw_int_nav_hud(idx, mElapsed);
  }
#endif
}

// Animate the current cyclic screen
void animateCurrentScreen(unsigned long frame)
{
  nerd_nav()->animateCurrentScreen(frame);
}

// Do LED stuff
void doLedStuff(unsigned long frame)
{
  nerd_nav()->doLedStuff(frame);
}
