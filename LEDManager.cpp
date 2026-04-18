#include "LEDManager.h"
#include "StateManager.h"

LEDManager::LEDManager() 
  : pixel(1, LED_PIN, NEO_RGB + NEO_KHZ800),
    currentMode(LED_STATUS_INDICATOR),
    red(0), green(0), blue(0),
    brightness(LED_DEFAULT_BRIGHTNESS),
    lastUpdate(0),
    animationStep(0),
    flashActive(false),
    flashStartTime(0),
    flashR(0), flashG(0), flashB(0),
    preFlashMode(LED_STATUS_INDICATOR) {
}

void LEDManager::begin() {
  pixel.begin();
  pixel.setBrightness(brightness);
  pixel.show(); // Initialize to off
}

void LEDManager::update() {
  unsigned long now = millis();
  
  // Handle quick flash overrides
  if (flashActive) {
    if (now - flashStartTime < 25) {  // 50ms flash
      setPixelColor(flashR, flashG, flashB);
      return;
    } else {
      // Flash complete, restore previous mode
      flashActive = false;
      currentMode = preFlashMode;
      animationStep = 0;
    }
  }
  
  // Update based on mode
  switch (currentMode) {
    case LED_OFF:
      pixel.setPixelColor(0, 0, 0, 0);
      pixel.show();
      break;
      
    case LED_SOLID:
      setPixelColor(red, green, blue);
      break;
      
    case LED_BREATHING:
      if (now - lastUpdate > 15) {  // ~15ms per step for smooth breathing
        lastUpdate = now;
        animationStep = (animationStep + 1) % 512;
        uint8_t breath = breathingCurve(animationStep);
        setPixelColor((red * breath) / 255, (green * breath) / 255, (blue * breath) / 255);
      }
      break;
      
    case LED_RAINBOW:
      if (now - lastUpdate > 20) {  // ~20ms per step
        lastUpdate = now;
        animationStep = (animationStep + 1) % 256;
        uint32_t color = wheel(animationStep);
        pixel.setPixelColor(0, color);
        pixel.show();
      }
      break;
      
    case LED_PULSE:
      if (now - lastUpdate > 5) {  // Faster pulse
        lastUpdate = now;
        animationStep = (animationStep + 2) % 256;
        uint8_t pulse = (animationStep < 128) ? (animationStep * 2) : ((255 - animationStep) * 2);
        setPixelColor((red * pulse) / 255, (green * pulse) / 255, (blue * pulse) / 255);
      }
      break;
      
    case LED_STATUS_INDICATOR:
    case LED_SYNC_STATE:
      if (!StateManager::ntpSynced) {
        // Connecting/Syncing - Blue breathing
        if (now - lastUpdate > 15) {
          lastUpdate = now;
          animationStep = (animationStep + 1) % 512;
          uint8_t breath = breathingCurve(animationStep);
          setPixelColor(0, 0, (255 * breath) / 255);
        }
      } else {
        // Connected & synced - Green solid
        setPixelColor(0, 255, 0);
      }
      break;
  }
}

void LEDManager::setMode(LEDMode mode) {
  currentMode = mode;
  animationStep = 0;
  lastUpdate = millis();
}

void LEDManager::setColor(uint8_t r, uint8_t g, uint8_t b) {
  red = r;
  green = g;
  blue = b;
}

void LEDManager::setColorNow(uint8_t r, uint8_t g, uint8_t b) {
  red = r;
  green = g;
  blue = b;
  setPixelColor(r, g, b);  // Immediately show
}

void LEDManager::setBrightness(uint8_t b) {
  brightness = b;
  pixel.setBrightness(brightness);
  pixel.show();
}

void LEDManager::off() {
  currentMode = LED_OFF;
  pixel.setPixelColor(0, 0, 0, 0);
  pixel.show();
}

void LEDManager::forceUpdate() {
  update();  // Force an immediate update
}

void LEDManager::flashWiFiPing() {
  // Only flash in sync state mode
  if (!flashActive && currentMode == LED_SYNC_STATE) {
    preFlashMode = currentMode;
    flashActive = true;
    flashStartTime = millis();
    flashR = 255;
    flashG = 255;
    flashB = 255;  // White flash
  }
}

void LEDManager::flashNTPSync() {
  // Only flash in sync state mode
  if (!flashActive && currentMode == LED_SYNC_STATE) {
    preFlashMode = currentMode;
    flashActive = true;
    flashStartTime = millis();
    flashR = 0;
    flashG = 255;
    flashB = 0;  // Green flash
  }
}

void LEDManager::indicateAPMode() {
  setColor(255, 140, 0);  // Orange
  setMode(LED_PULSE);
}

void LEDManager::indicateConnecting() {
  setColor(0, 0, 255);  // Blue
  setMode(LED_BREATHING);
}

void LEDManager::indicateConnected() {
  setColor(0, 255, 0);  // Green
  setMode(LED_SOLID);
}

void LEDManager::indicateNTPSync() {
  setColor(0, 255, 255);  // Cyan
  setMode(LED_PULSE);
}

void LEDManager::indicateError() {
  setColor(255, 0, 0);  // Red
  setMode(LED_PULSE);
}

void LEDManager::setPixelColor(uint8_t r, uint8_t g, uint8_t b) {
  pixel.setPixelColor(0, pixel.Color(r, g, b));
  pixel.show();
}

uint32_t LEDManager::wheel(byte wheelPos) {
  wheelPos = 255 - wheelPos;
  if (wheelPos < 85) {
    return pixel.Color(255 - wheelPos * 3, 0, wheelPos * 3);
  }
  if (wheelPos < 170) {
    wheelPos -= 85;
    return pixel.Color(0, wheelPos * 3, 255 - wheelPos * 3);
  }
  wheelPos -= 170;
  return pixel.Color(wheelPos * 3, 255 - wheelPos * 3, 0);
}

uint8_t LEDManager::breathingCurve(uint16_t step) {
  // Smooth sine-wave breathing effect
  if (step < 256) {
    // Fade in
    return (step * step) / 256;
  } else {
    // Fade out
    uint16_t fadeStep = 511 - step;
    return (fadeStep * fadeStep) / 256;
  }
}
