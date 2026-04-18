#ifndef MEETING_MANAGER_H
#define MEETING_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

#define MAX_MEETINGS 16
#define DEFAULT_LEAD_MINUTES 5
#define REMINDER_DISPLAY_SECONDS 60

// Day mask bits: bit 0=Mon, 1=Tue, 2=Wed, 3=Thu, 4=Fri, 5=Sat, 6=Sun
#define DAY_MON (1 << 0)
#define DAY_TUE (1 << 1)
#define DAY_WED (1 << 2)
#define DAY_THU (1 << 3)
#define DAY_FRI (1 << 4)
#define DAY_SAT (1 << 5)
#define DAY_SUN (1 << 6)

struct Meeting {
  char title[21];
  uint8_t hour;
  uint8_t minute;
  uint8_t type;       // 0=recurring, 1=oneshot
  uint8_t daysMask;   // bitmask for recurring
  uint16_t year;      // oneshot only
  uint8_t month;      // oneshot only
  uint8_t day;        // oneshot only
  bool valid;
  bool shownToday;    // prevents repeat firing same day
};

class MeetingManager {
public:
  MeetingManager();

  void begin();

  bool syncFromJson(const String& jsonPayload);
  String getStoredJson();
  int getCount();
  int getLeadMinutes();

  bool addMeeting(const char* title, uint8_t hour, uint8_t minute,
                  uint8_t type, uint8_t daysMask,
                  uint16_t year = 0, uint8_t month = 0, uint8_t day = 0);
  bool removeMeeting(int index);
  void clearAll();
  void setLeadMinutes(int mins);

  bool checkReminders(int currentHour, int currentMin,
                      int currentDayOfWeek, int currentYear,
                      int currentMonth, int currentDay,
                      char* outTitle, char* outTime,
                      int* outMeetHour = nullptr, int* outMeetMinute = nullptr,
                      int* outIndex = nullptr);

  void resetShownFlags();

  Meeting meetings[MAX_MEETINGS];
  int meetingCount;
  int leadMinutes;

  uint8_t parseDaysMask(const String& daysStr);

private:
  Preferences prefs;
  void parseJsonToMeetings(const String& json);
  void persistToNvs();
};

#endif // MEETING_MANAGER_H
