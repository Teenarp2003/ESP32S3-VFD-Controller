#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "config.h"
#include "StateManager.h"

class TimeManager {
public:
  TimeManager();
  
  void begin();
  bool syncNTP();
  void update();
  void forceUpdate();
  
  void getTime(int& h, int& m, int& s);
  time_t getEpochTime();
  void formatTime(char* buffer, size_t bufSize, bool is24hr, bool showColon);
  void formatDate(char* buffer, size_t bufSize);
  
  void setTimezone(long offsetSeconds);
  long getTimezone();
  bool isSynced();
  
  void incrementLocalTime();
  
private:
  WiFiUDP ntpUDP;
  NTPClient* timeClient;
};

#endif // TIME_MANAGER_H
