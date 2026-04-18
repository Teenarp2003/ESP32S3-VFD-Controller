#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <PT6314.h>
#include "config.h"
#include "StateManager.h"

class DisplayManager {
public:
  DisplayManager();
  
  void begin();
  void clear();
  void setBrightness(int level);
  void resetAndApplyBrightness(int level);
  
  void setCursor(int col, int row);
  void print(const char* text);
  void printCentered(const char* text, int row);
  void printAt(const char* text, int col, int row);
  
  void showClock(int h, int m, int s, bool is24hr, bool showColon, char* lastPeriod, bool& forceClear, bool forceRedraw = false);
  void showDate(time_t epochTime, bool forceRedraw = false);
  void showStatus(const char* message, int row);
  void clearRow(int row);
  
  void powerOn();
  void powerOff();
  
  PT6314* getVFD() { return &vfd; }
  
private:
  PT6314 vfd;
  int lastAppliedBrightness;
  
  void setBrightnessFixed(int brt);
  void applyBrightness(int b);
};

#endif // DISPLAY_MANAGER_H
