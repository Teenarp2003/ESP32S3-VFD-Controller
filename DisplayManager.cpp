#include "DisplayManager.h"

DisplayManager::DisplayManager() : vfd(SCK, STB, SI), lastAppliedBrightness(-1) {
}

void DisplayManager::begin() {
  pinMode(GPIO_PIN_10, OUTPUT);
  digitalWrite(GPIO_PIN_10, HIGH);
  StateManager::gpio10State = true;
  
  vfd.begin(DISPLAY_COLS, DISPLAY_ROWS);
  vfd.clear();
}

void DisplayManager::clear() {
  vfd.clear();
}

void DisplayManager::setCursor(int col, int row) {
  vfd.setCursor(col, row);
}

void DisplayManager::print(const char* text) {
  vfd.print(text);
}

void DisplayManager::printCentered(const char* text, int row) {
  int len = strlen(text);
  int start = (DISPLAY_COLS - len) / 2;
  if (start < 0) start = 0;
  vfd.setCursor(start, row);
  vfd.print(text);
}

void DisplayManager::printAt(const char* text, int col, int row) {
  vfd.setCursor(col, row);
  vfd.print(text);
}

void DisplayManager::clearRow(int row) {
  vfd.setCursor(0, row);
  vfd.print("                    ");
}

void DisplayManager::showStatus(const char* message, int row) {
  vfd.setCursor(0, row);
  vfd.print(message);
}

void DisplayManager::showClock(int h, int m, int s, bool is24hr, bool showColon, char* lastPeriod, bool& forceClear, bool forceRedraw) {
  static int lastH = -1, lastM = -1, lastS = -1;
  static bool lastShowColon = true;
  static bool lastIs24hr = false;
  
  // Force redraw by resetting cache
  if (forceRedraw) {
    lastH = lastM = lastS = -1;
    forceClear = false; // Don't clear since animation already set the content
  }
  
  char timeStr[21] = "";
  char period[4] = "";
  int displayHour = h;
  
  if (!is24hr) {
    if (h == 0) {
      displayHour = 12;
      strncpy(period, "AM", sizeof(period));
    } else if (h == 12) {
      strncpy(period, "PM", sizeof(period));
    } else if (h > 12) {
      displayHour = h - 12;
      strncpy(period, "PM", sizeof(period));
    } else {
      strncpy(period, "AM", sizeof(period));
    }
    
    snprintf(timeStr, sizeof(timeStr), "%02d%c%02d%c%02d %s",
             displayHour, showColon ? ':' : ' ',
             m, showColon ? ':' : ' ',
             s, period);
  } else {
    snprintf(timeStr, sizeof(timeStr), "%02d%c%02d%c%02d",
             displayHour, showColon ? ':' : ' ',
             m, showColon ? ':' : ' ',
             s);
  }
   //(but not on forceRedraw from animation)
  if (!forceRedraw) {
    if (!is24hr) {
      if (strcmp(lastPeriod, period) != 0 || forceClear) {
        clearRow(0);
        strncpy(lastPeriod, period, 4);
        forceClear = false;
        lastH = lastM = lastS = -1; // Force full redraw
      }
    } else {
      if (strlen(lastPeriod) != 0 || forceClear) {
        clearRow(0);
        lastPeriod[0] = '\0';
        forceClear = false;
        lastH = lastM = lastS = -1; // Force full redraw
      }
    }
  } else {
    // On forceRedraw, just update lastPeriod without clearing
    if (!is24hr) {
      strncpy(lastPeriod, period, 4);
    } else {
      lastPeriod[0] = '\0';
      lastH = lastM = lastS = -1; // Force full redraw
    }
  }
  
  // Only update if time or colon changed
  if (h != lastH || m != lastM || s != lastS || showColon != lastShowColon || is24hr != lastIs24hr) {
    int timeStart = (DISPLAY_COLS - strlen(timeStr)) / 2;
    if (timeStart < 0) timeStart = 0;
    vfd.setCursor(timeStart, 0);
    vfd.print(timeStr);
    
    lastH = h;
    lastM = m;
    lastS = s;
    lastShowColon = showColon;
    lastIs24hr = is24hr;
  }
}

void DisplayManager::showDate(time_t epochTime, bool forceRedraw) {
  static int lastDay = -1, lastMonth = -1, lastYear = -1, lastWday = -1;
  
  // Force redraw by resetting cache
  if (forceRedraw) {
    lastDay = lastMonth = lastYear = lastWday = -1;
  }
  
  struct tm* ptm = gmtime(&epochTime);
  const char* daysOfWeek[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  const char* monthsOfYear[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                "Jul","Aug","Sep","Oct","Nov","Dec"};
  char bottomStr[21];
  
  if (ptm) {
    snprintf(bottomStr, sizeof(bottomStr), "%s %02d %s %04d",
             daysOfWeek[ptm->tm_wday],
             ptm->tm_mday,
             monthsOfYear[ptm->tm_mon],
             ptm->tm_year + 1900);
    
    // Only update if date changed
    if (ptm->tm_mday != lastDay || ptm->tm_mon != lastMonth || 
        ptm->tm_year != lastYear || ptm->tm_wday != lastWday) {
      int start = (DISPLAY_COLS - strlen(bottomStr)) / 2;
      if (start < 0) start = 0;
      vfd.setCursor(start, 1);
      vfd.print(bottomStr);
      
      lastDay = ptm->tm_mday;
      lastMonth = ptm->tm_mon;
      lastYear = ptm->tm_year;
      lastWday = ptm->tm_wday;
    }
  } else {
    snprintf(bottomStr, sizeof(bottomStr), "Waiting for NTP    ");
    int start = (DISPLAY_COLS - strlen(bottomStr)) / 2;
    if (start < 0) start = 0;
    vfd.setCursor(start, 1);
    vfd.print(bottomStr);
  }
}

void DisplayManager::setBrightnessFixed(int brt) {
  uint8_t brtRegVal;
  if (brt <= 12) {
    brtRegVal = VFD_BRT_25_VAL;
  } else if (brt <= 37) {
    brtRegVal = VFD_BRT_25_VAL;
  } else if (brt <= 62) {
    brtRegVal = VFD_BRT_50_VAL;
  } else if (brt <= 87) {
    brtRegVal = VFD_BRT_75_VAL;
  } else {
    brtRegVal = VFD_BRT_100_VAL;
  }
  
  uint8_t newRegVal = (VFD_FUNCTIONSET | VFD_8BITMODE | VFD_2LINE | brtRegVal);
  
  if (newRegVal != StateManager::vfdFunctionSetReg) {
    StateManager::vfdFunctionSetReg = newRegVal;
    
    uint16_t payload = ((uint16_t)VFD_COMMAND_MODE << 8) | newRegVal;
    
    digitalWrite(STB, 0);
    delayMicroseconds(1);
    digitalWrite(SCK, 0);
    
    for (int i = 15; i >= 0; i--) {
      digitalWrite(SI, bitRead(payload, i));
      delayMicroseconds(2);
      digitalWrite(SCK, 1);
      delayMicroseconds(2);
      digitalWrite(SCK, 0);
    }
    
    digitalWrite(SCK, 1);
    digitalWrite(STB, 1);
    delayMicroseconds(1);
  }
}

void DisplayManager::applyBrightness(int b) {
  if (b < 0) b = 0;
  if (b > 100) b = 100;
  StateManager::uiBrightness = b;
  
  if (b != lastAppliedBrightness) {
    lastAppliedBrightness = b;
    setBrightnessFixed(b);
  }
}

void DisplayManager::setBrightness(int level) {
  applyBrightness(level);
}

void DisplayManager::resetAndApplyBrightness(int b) {
  if (b < 0) b = 0;
  if (b > 100) b = 100;
  StateManager::uiBrightness = b;
  StateManager::vfdFunctionSetReg = 0;
  lastAppliedBrightness = -1;
  setBrightnessFixed(b);
}

void DisplayManager::powerOn() {
  StateManager::gpio10State = true;
  digitalWrite(GPIO_PIN_10, HIGH);
}

void DisplayManager::powerOff() {
  StateManager::gpio10State = false;
  digitalWrite(GPIO_PIN_10, LOW);
}
