#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

class StateManager {
public:
  static void init();
  
  // Time variables
  static volatile int hours;
  static volatile int minutes;
  static volatile int seconds;
  static volatile bool ntpSynced;
  static volatile bool colonOn;
  
  // Display mode and content
  static bool customMode;
  static char customMessage[41];
  static char customLine1[21];
  static char customLine2[21];
  static char prevClockLine1[21];
  static char prevClockLine2[21];
  
  // Settings
  static int uiBrightness;
  static uint8_t vfdFunctionSetReg;
  static volatile bool clockEnabled;
  static volatile bool use24Hour;
  static volatile bool colonBlink;
  static volatile long tzOffsetSeconds;
  
  // Schedule settings
  static bool scheduleEnabled;
  static int scheduleOnHour;
  static int scheduleOnMinute;
  static int scheduleOffHour;
  static int scheduleOffMinute;
  static bool scheduleDays[7];
  
  // Status flags
  static volatile int syncDot;
  static volatile bool animationInProgress;
  static volatile bool gpio10State;
  static volatile int statusStage;
  static volatile bool clearedAfterConnect;
  static volatile bool screenClearedAfterConnect;
  static volatile bool forceClearTopRow;
  static volatile bool forceSeamlessUpdate;
  
  // LED settings
  static int ledMode;
  static uint8_t ledRed;
  static uint8_t ledGreen;
  static uint8_t ledBlue;
  static uint8_t ledBrightness;
  
  // Meeting reminder state
  static volatile bool meetingReminderActive;
  static volatile unsigned long meetingReminderStart;
  static volatile int meetingTargetHour;
  static volatile int meetingTargetMinute;
  static volatile int meetingReminderIndex;
  
  // OTA state
  static volatile bool otaInProgress;
  
  // BLE state
  static volatile bool bleConnected;
  
  // Task handles
  static TaskHandle_t wifiNtpTaskHandle;
  
  // Synchronization
  static SemaphoreHandle_t displayMutex;
};

#endif // STATE_MANAGER_H
