#include "TimeManager.h"

TimeManager::TimeManager() {
  // Initialize with default timezone offset (will be updated in begin())
  timeClient = new NTPClient(ntpUDP, NTP_SERVER, StateManager::tzOffsetSeconds);
}

void TimeManager::begin() {
  timeClient->begin();
  timeClient->setUpdateInterval(NTP_UPDATE_INTERVAL);
  // Set timezone offset again in case it was changed before begin()
  timeClient->setTimeOffset(StateManager::tzOffsetSeconds);
}

bool TimeManager::syncNTP() {
  // Force an immediate update instead of waiting for the interval
  if (timeClient->forceUpdate()) {
    StateManager::ntpSynced = true;
    return true;
  }
  return false;
}

void TimeManager::update() {
  // Called from WiFi task - update NTP client
  timeClient->update();
}

void TimeManager::forceUpdate() {
  timeClient->forceUpdate();
}

void TimeManager::getTime(int& h, int& m, int& s) {
  h = StateManager::hours;
  m = StateManager::minutes;
  s = StateManager::seconds;
}

time_t TimeManager::getEpochTime() {
  return (time_t)timeClient->getEpochTime();
}

void TimeManager::formatTime(char* buffer, size_t bufSize, bool is24hr, bool showColon) {
  int displayHour = StateManager::hours;
  char period[4] = "";
  
  if (!is24hr) {
    if (StateManager::hours == 0) displayHour = 12;
    else if (StateManager::hours == 12) strncpy(period, "PM", sizeof(period));
    else if (StateManager::hours > 12) { 
      displayHour = StateManager::hours - 12; 
      strncpy(period, "PM", sizeof(period)); 
    } else {
      strncpy(period, "AM", sizeof(period));
    }
    
    snprintf(buffer, bufSize, "%02d%c%02d%c%02d %s",
             displayHour, showColon ? ':' : ' ',
             StateManager::minutes, showColon ? ':' : ' ',
             StateManager::seconds, period);
  } else {
    snprintf(buffer, bufSize, "%02d%c%02d%c%02d",
             displayHour, showColon ? ':' : ' ',
             StateManager::minutes, showColon ? ':' : ' ',
             StateManager::seconds);
  }
}

void TimeManager::formatDate(char* buffer, size_t bufSize) {
  time_t rawTime = (time_t)timeClient->getEpochTime();
  struct tm* ptm = gmtime(&rawTime);
  const char* daysOfWeek[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  const char* monthsOfYear[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                "Jul","Aug","Sep","Oct","Nov","Dec"};
  
  if (ptm) {
    snprintf(buffer, bufSize, "%s %02d %s %04d",
             daysOfWeek[ptm->tm_wday],
             ptm->tm_mday,
             monthsOfYear[ptm->tm_mon],
             ptm->tm_year + 1900);
  } else {
    snprintf(buffer, bufSize, "Waiting for NTP    ");
  }
}

void TimeManager::setTimezone(long offsetSeconds) {
  StateManager::tzOffsetSeconds = offsetSeconds;
  timeClient->setTimeOffset(offsetSeconds);
}

long TimeManager::getTimezone() {
  return StateManager::tzOffsetSeconds;
}

bool TimeManager::isSynced() {
  return StateManager::ntpSynced;
}

void TimeManager::incrementLocalTime() {
  StateManager::seconds++;
  if (StateManager::seconds >= 60) {
    StateManager::seconds = 0;
    StateManager::minutes++;
  }
  if (StateManager::minutes >= 60) {
    StateManager::minutes = 0;
    StateManager::hours++;
  }
  if (StateManager::hours >= 24) {
    StateManager::hours = 0;
  }
}
