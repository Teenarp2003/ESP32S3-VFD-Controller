#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

enum LEDMode {
  LED_OFF = 0,
  LED_SOLID = 1,
  LED_BREATHING = 2,
  LED_RAINBOW = 3,
  LED_PULSE = 4,
  LED_STATUS_INDICATOR = 5,  // Auto mode based on system state
  LED_SYNC_STATE = 6         // Status indicator + WiFi/NTP sync flashes
};

class LEDManager {
public:
  LEDManager();
  
  void begin();
  void update();  // Call this periodically to update animations
  
  // Manual control
  void setMode(LEDMode mode);
  void setColor(uint8_t r, uint8_t g, uint8_t b);
  void setColorNow(uint8_t r, uint8_t g, uint8_t b);  // Set color and show immediately
  void setBrightness(uint8_t brightness);
  void off();
  void forceUpdate();  // Force immediate LED update
  
  // Quick flash indicators
  void flashWiFiPing();     // Quick white flash for WiFi ping
  void flashNTPSync();      // Quick green flash for NTP sync success
  
  // Status indicator mode colors
  void indicateAPMode();
  void indicateConnecting();
  void indicateConnected();
  void indicateNTPSync();
  void indicateError();
  
  LEDMode getMode() { return currentMode; }
  void getColor(uint8_t& r, uint8_t& g, uint8_t& b) { r = red; g = green; b = blue; }
  uint8_t getBrightness() { return brightness; }
  
private:
  Adafruit_NeoPixel pixel;
  
  LEDMode currentMode;
  uint8_t red, green, blue;
  uint8_t brightness;
  
  unsigned long lastUpdate;
  uint16_t animationStep;
  
  // Flash state
  bool flashActive;
  unsigned long flashStartTime;
  uint8_t flashR, flashG, flashB;
  LEDMode preFlashMode;
  
  void setPixelColor(uint8_t r, uint8_t g, uint8_t b);
  uint32_t wheel(byte wheelPos);
  uint8_t breathingCurve(uint16_t step);
};

#endif // LED_MANAGER_H
