#include "MeetingManager.h"
#include <ArduinoJson.h>

MeetingManager::MeetingManager() : meetingCount(0), leadMinutes(DEFAULT_LEAD_MINUTES) {
  memset(meetings, 0, sizeof(meetings));
}

void MeetingManager::begin() {
  prefs.begin("meetings", false);
  leadMinutes = prefs.getInt("lead_min", DEFAULT_LEAD_MINUTES);
  String stored = prefs.getString("mtg_json", "");
  prefs.end();

  if (stored.length() > 0) {
    parseJsonToMeetings(stored);
    Serial.printf("[Meetings] Loaded %d meetings from flash (lead=%d min)\n",
                  meetingCount, leadMinutes);
  } else {
    Serial.println("[Meetings] No stored meetings");
  }
}

bool MeetingManager::syncFromJson(const String& jsonPayload) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, jsonPayload);
  if (err) {
    Serial.printf("[Meetings] JSON parse error: %s\n", err.c_str());
    return false;
  }

  int newLead = doc["lead_minutes"] | DEFAULT_LEAD_MINUTES;
  leadMinutes = newLead;

  // Clear existing
  meetingCount = 0;
  memset(meetings, 0, sizeof(meetings));

  JsonArray arr = doc["meetings"].as<JsonArray>();
  for (JsonObject obj : arr) {
    if (meetingCount >= MAX_MEETINGS) break;

    Meeting& m = meetings[meetingCount];
    const char* title = obj["title"] | "";
    strncpy(m.title, title, 20);
    m.title[20] = '\0';

    const char* timeStr = obj["time"] | "00:00";
    sscanf(timeStr, "%hhu:%hhu", &m.hour, &m.minute);

    const char* typeStr = obj["type"] | "recurring";
    if (strcmp(typeStr, "oneshot") == 0) {
      m.type = 1;
      const char* dateStr = obj["date"] | "2026-01-01";
      int y, mo, d;
      sscanf(dateStr, "%d-%d-%d", &y, &mo, &d);
      m.year = (uint16_t)y;
      m.month = (uint8_t)mo;
      m.day = (uint8_t)d;
    } else {
      m.type = 0;
      const char* daysStr = obj["days"] | "";
      m.daysMask = parseDaysMask(String(daysStr));
    }

    m.valid = true;
    m.shownToday = false;
    meetingCount++;
  }

  // Persist to NVS
  prefs.begin("meetings", false);
  prefs.putInt("lead_min", leadMinutes);
  prefs.putString("mtg_json", jsonPayload);
  prefs.end();

  Serial.printf("[Meetings] Synced %d meetings (lead=%d min)\n",
                meetingCount, leadMinutes);
  return true;
}

String MeetingManager::getStoredJson() {
  prefs.begin("meetings", true);
  String json = prefs.getString("mtg_json", "{}");
  prefs.end();
  return json;
}

int MeetingManager::getCount() {
  return meetingCount;
}

int MeetingManager::getLeadMinutes() {
  return leadMinutes;
}

uint8_t MeetingManager::parseDaysMask(const String& daysStr) {
  uint8_t mask = 0;
  if (daysStr.indexOf("mon") >= 0) mask |= DAY_MON;
  if (daysStr.indexOf("tue") >= 0) mask |= DAY_TUE;
  if (daysStr.indexOf("wed") >= 0) mask |= DAY_WED;
  if (daysStr.indexOf("thu") >= 0) mask |= DAY_THU;
  if (daysStr.indexOf("fri") >= 0) mask |= DAY_FRI;
  if (daysStr.indexOf("sat") >= 0) mask |= DAY_SAT;
  if (daysStr.indexOf("sun") >= 0) mask |= DAY_SUN;
  return mask;
}

void MeetingManager::parseJsonToMeetings(const String& json) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    Serial.printf("[Meetings] Stored JSON parse error: %s\n", err.c_str());
    return;
  }

  meetingCount = 0;
  memset(meetings, 0, sizeof(meetings));

  JsonArray arr = doc["meetings"].as<JsonArray>();
  for (JsonObject obj : arr) {
    if (meetingCount >= MAX_MEETINGS) break;

    Meeting& m = meetings[meetingCount];
    const char* title = obj["title"] | "";
    strncpy(m.title, title, 20);
    m.title[20] = '\0';

    const char* timeStr = obj["time"] | "00:00";
    sscanf(timeStr, "%hhu:%hhu", &m.hour, &m.minute);

    const char* typeStr = obj["type"] | "recurring";
    if (strcmp(typeStr, "oneshot") == 0) {
      m.type = 1;
      const char* dateStr = obj["date"] | "2026-01-01";
      int y, mo, d;
      sscanf(dateStr, "%d-%d-%d", &y, &mo, &d);
      m.year = (uint16_t)y;
      m.month = (uint8_t)mo;
      m.day = (uint8_t)d;
    } else {
      m.type = 0;
      const char* daysStr = obj["days"] | "";
      m.daysMask = parseDaysMask(String(daysStr));
    }

    m.valid = true;
    m.shownToday = false;
    meetingCount++;
  }
}

bool MeetingManager::checkReminders(int currentHour, int currentMin,
                                     int currentDayOfWeek, int currentYear,
                                     int currentMonth, int currentDay,
                                     char* outTitle, char* outTime,
                                     int* outMeetHour, int* outMeetMinute,
                                     int* outIndex) {
  // tm_wday: 0=Sunday, 1=Monday, ..., 6=Saturday
  // Our daysMask: bit 0=Mon, 1=Tue, ..., 5=Sat, 6=Sun
  // Convert: Sunday(0)->bit6, Monday(1)->bit0, ..., Saturday(6)->bit5
  uint8_t todayBit;
  if (currentDayOfWeek == 0) {
    todayBit = DAY_SUN;
  } else {
    todayBit = (1 << (currentDayOfWeek - 1));
  }

  int currentMinutes = currentHour * 60 + currentMin;

  for (int i = 0; i < meetingCount; i++) {
    Meeting& m = meetings[i];
    if (!m.valid || m.shownToday) continue;

    int meetingMinutes = m.hour * 60 + m.minute;
    int reminderMinutes = meetingMinutes - leadMinutes;

    // Show reminder anytime within the lead window up to meeting time
    if (currentMinutes < reminderMinutes || currentMinutes >= meetingMinutes) {
      continue;
    }

    bool matches = false;

    if (m.type == 0) {
      // Recurring: check day mask
      matches = (m.daysMask & todayBit) != 0;
    } else {
      // Oneshot: check exact date
      matches = (currentYear == m.year &&
                 currentMonth == m.month &&
                 currentDay == m.day);
    }

    if (matches) {
      m.shownToday = true;

      strncpy(outTitle, m.title, 20);
      outTitle[20] = '\0';

      // Format time as "at H:MM AM/PM"
      int displayHour = m.hour;
      const char* ampm = "AM";
      if (displayHour == 0) {
        displayHour = 12;
      } else if (displayHour == 12) {
        ampm = "PM";
      } else if (displayHour > 12) {
        displayHour -= 12;
        ampm = "PM";
      }
      snprintf(outTime, 21, "at %d:%02d %s", displayHour, m.minute, ampm);

      if (outMeetHour) *outMeetHour = m.hour;
      if (outMeetMinute) *outMeetMinute = m.minute;
      if (outIndex) *outIndex = i;

      Serial.printf("[Meetings] Reminder: %s %s\n", outTitle, outTime);
      return true;
    }
  }

  return false;
}

void MeetingManager::resetShownFlags() {
  for (int i = 0; i < meetingCount; i++) {
    meetings[i].shownToday = false;
  }
}

bool MeetingManager::addMeeting(const char* title, uint8_t hour, uint8_t minute,
                                uint8_t type, uint8_t daysMask,
                                uint16_t year, uint8_t month, uint8_t day) {
  if (meetingCount >= MAX_MEETINGS) return false;

  Meeting& m = meetings[meetingCount];
  memset(&m, 0, sizeof(Meeting));
  strncpy(m.title, title, 20);
  m.title[20] = '\0';
  m.hour = hour;
  m.minute = minute;
  m.type = type;
  m.daysMask = daysMask;
  m.year = year;
  m.month = month;
  m.day = day;
  m.valid = true;
  m.shownToday = false;
  meetingCount++;

  persistToNvs();
  Serial.printf("[Meetings] Added: %s at %02d:%02d (%d total)\n", title, hour, minute, meetingCount);
  return true;
}

bool MeetingManager::removeMeeting(int index) {
  if (index < 0 || index >= meetingCount) return false;

  for (int i = index; i < meetingCount - 1; i++) {
    meetings[i] = meetings[i + 1];
  }
  memset(&meetings[meetingCount - 1], 0, sizeof(Meeting));
  meetingCount--;

  persistToNvs();
  Serial.printf("[Meetings] Removed #%d (%d remaining)\n", index + 1, meetingCount);
  return true;
}

void MeetingManager::clearAll() {
  meetingCount = 0;
  memset(meetings, 0, sizeof(meetings));

  prefs.begin("meetings", false);
  prefs.putString("mtg_json", "{}");
  prefs.end();

  Serial.println("[Meetings] Cleared all");
}

void MeetingManager::setLeadMinutes(int mins) {
  if (mins < 1) mins = 1;
  if (mins > 60) mins = 60;
  leadMinutes = mins;

  prefs.begin("meetings", false);
  prefs.putInt("lead_min", leadMinutes);
  prefs.end();

  Serial.printf("[Meetings] Lead time: %d min\n", leadMinutes);
}

void MeetingManager::persistToNvs() {
  // Rebuild JSON from in-memory meetings array and save
  String json = "{\"lead_minutes\":";
  json += String(leadMinutes);
  json += ",\"meetings\":[";

  for (int i = 0; i < meetingCount; i++) {
    Meeting& m = meetings[i];
    if (!m.valid) continue;
    if (i > 0) json += ",";

    json += "{\"title\":\"";
    json += m.title;
    json += "\",\"time\":\"";
    char timeBuf[6];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", m.hour, m.minute);
    json += timeBuf;

    if (m.type == 1) {
      json += "\",\"type\":\"oneshot\",\"date\":\"";
      char dateBuf[11];
      snprintf(dateBuf, sizeof(dateBuf), "%04d-%02d-%02d", m.year, m.month, m.day);
      json += dateBuf;
      json += "\"}";
    } else {
      json += "\",\"type\":\"recurring\",\"days\":\"";
      String days = "";
      if (m.daysMask & DAY_MON) { if (days.length()) days += ","; days += "mon"; }
      if (m.daysMask & DAY_TUE) { if (days.length()) days += ","; days += "tue"; }
      if (m.daysMask & DAY_WED) { if (days.length()) days += ","; days += "wed"; }
      if (m.daysMask & DAY_THU) { if (days.length()) days += ","; days += "thu"; }
      if (m.daysMask & DAY_FRI) { if (days.length()) days += ","; days += "fri"; }
      if (m.daysMask & DAY_SAT) { if (days.length()) days += ","; days += "sat"; }
      if (m.daysMask & DAY_SUN) { if (days.length()) days += ","; days += "sun"; }
      json += days;
      json += "\"}";
    }
  }

  json += "]}";

  prefs.begin("meetings", false);
  prefs.putInt("lead_min", leadMinutes);
  prefs.putString("mtg_json", json);
  prefs.end();
}
