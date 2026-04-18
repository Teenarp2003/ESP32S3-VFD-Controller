/*
 * ESP32 VFD BLE Clock
 * 20x2 PT6314 VFD + BLE control + NTP time + ArduinoOTA
 *
 * WiFi runs in background for NTP sync and OTA only.
 * All interactive control is via BLE (Nordic UART Service).
 * Use ble_terminal.py or any BLE serial app to send commands.
 *
 * Author: Teenarp2026
 */

#include "config.h"
#include "StateManager.h"
#include "DisplayManager.h"
#include "Animation.h"
#include "TimeManager.h"
#include "LEDManager.h"
#include "MeetingManager.h"
#include "BLECommandHandler.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Preferences.h>

DisplayManager display;
Animation animation(&display);
TimeManager timeManager;
LEDManager ledManager;
MeetingManager meetingManager;
BLECommandHandler bleCmd(&display, &animation, &timeManager, &ledManager, &meetingManager);

void wifiNtpTask(void* parameter);
void updateClockTask(void* parameter);

// ── Setup ────────────────────────────────────────────────────────

void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.println("\n\n=== VFD BLE Clock Starting ===");

  StateManager::init();

  // Load schedule settings from NVS
  Preferences prefs;
  prefs.begin("schedule", false);
  StateManager::scheduleEnabled = prefs.getBool("enabled", false);
  StateManager::scheduleOnHour = prefs.getInt("onHour", 8);
  StateManager::scheduleOnMinute = prefs.getInt("onMin", 0);
  StateManager::scheduleOffHour = prefs.getInt("offHour", 22);
  StateManager::scheduleOffMinute = prefs.getInt("offMin", 0);
  int daysBits = prefs.getInt("days", 0x7F);
  for (int i = 0; i < 7; i++) {
    StateManager::scheduleDays[i] = (daysBits & (1 << i)) != 0;
  }
  prefs.end();

  display.begin();
  display.setBrightness(DEFAULT_BRIGHTNESS);
  ledManager.begin();
  meetingManager.begin();

  Serial.println("Creating tasks...");

  xTaskCreatePinnedToCore(
    wifiNtpTask,
    "WiFiBLE",
    WIFI_TASK_STACK_SIZE,
    NULL,
    1,
    &StateManager::wifiNtpTaskHandle,
    0
  );

  xTaskCreatePinnedToCore(
    updateClockTask,
    "ClockUpdate",
    DISPLAY_TASK_STACK_SIZE,
    NULL,
    1,
    NULL,
    1
  );

  Serial.println("Setup complete");
}

void loop() {
  delay(1000);
}

// ── Core 0: WiFi + NTP + OTA + BLE + LED ────────────────────────

void wifiNtpTask(void* parameter) {
  Serial.println("[Core0] Task started");

  ledManager.setColorNow(128, 0, 255);
  ledManager.setMode(LED_BREATHING);

  // Connect WiFi
  display.printCentered("Connecting WiFi", 0);
  StateManager::statusStage = 0;

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT) {
    vTaskDelay(pdMS_TO_TICKS(250));
    Serial.print(".");
  }
  Serial.println();

  bool wifiOk = (WiFi.status() == WL_CONNECTED);

  if (wifiOk) {
    WiFi.setSleep(false);
    Serial.printf("[WiFi] Connected - IP: %s\n", WiFi.localIP().toString().c_str());

    ledManager.setColorNow(0, 255, 255);
    ledManager.setMode(LED_SOLID);

    // Show IP
    StateManager::statusStage = 10;
    if (xSemaphoreTake(StateManager::displayMutex, pdMS_TO_TICKS(MUTEX_TIMEOUT))) {
      display.clear();
      char ipStr[21];
      snprintf(ipStr, sizeof(ipStr), "%s", WiFi.localIP().toString().c_str());
      display.printCentered(ipStr, 0);
      xSemaphoreGive(StateManager::displayMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(1500));

    // NTP sync
    StateManager::statusStage = 1;
    StateManager::syncDot = 1;
    ledManager.setColorNow(255, 255, 0);
    ledManager.setMode(LED_PULSE);

    timeManager.begin();
    unsigned long ntpStart = millis();
    bool ntpOk = false;

    while (!StateManager::ntpSynced && (millis() - ntpStart) < NTP_SYNC_TIMEOUT) {
      if (timeManager.syncNTP()) { ntpOk = true; break; }
      timeManager.forceUpdate();
      vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (ntpOk) {
      Serial.println("[NTP] Synced");
      StateManager::statusStage = 11;
      if (xSemaphoreTake(StateManager::displayMutex, pdMS_TO_TICKS(MUTEX_TIMEOUT))) {
        display.clear();
        display.printCentered("NTP Connected!", 0);
        xSemaphoreGive(StateManager::displayMutex);
      }
      vTaskDelay(pdMS_TO_TICKS(1000));
    } else {
      Serial.println("[NTP] Timeout");
    }

    // OTA
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    if (strlen(OTA_PASSWORD) > 0) {
      ArduinoOTA.setPassword(OTA_PASSWORD);
    }

    ArduinoOTA.onStart([]() {
      Serial.println("[OTA] Update starting...");
      StateManager::otaInProgress = true;
      vTaskDelay(pdMS_TO_TICKS(50));
      if (xSemaphoreTake(StateManager::displayMutex, pdMS_TO_TICKS(MUTEX_TIMEOUT))) {
        display.clear();
        display.printCentered("OTA Updating...", 0);
        display.printAt("[..............]  0%", 0, 1);
        xSemaphoreGive(StateManager::displayMutex);
      }
    });

    ArduinoOTA.onEnd([]() {
      Serial.println("[OTA] Complete!");
      if (xSemaphoreTake(StateManager::displayMutex, pdMS_TO_TICKS(MUTEX_TIMEOUT))) {
        display.clear();
        display.printCentered("OTA Complete!", 0);
        display.printCentered("Rebooting...", 1);
        xSemaphoreGive(StateManager::displayMutex);
      }
      StateManager::otaInProgress = false;
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      int pct = progress * 100 / total;
      int bars = pct * 14 / 100;
      char bar[21];
      bar[0] = '[';
      for (int i = 0; i < 14; i++) bar[i + 1] = (i < bars) ? '#' : '.';
      bar[15] = ']';
      snprintf(bar + 16, 5, "%3d%%", pct);
      bar[20] = '\0';
      if (xSemaphoreTake(StateManager::displayMutex, pdMS_TO_TICKS(50))) {
        display.printAt(bar, 0, 1);
        xSemaphoreGive(StateManager::displayMutex);
      }
      Serial.printf("[OTA] Progress: %u%%\r", pct);
    });

    ArduinoOTA.onError([](ota_error_t error) {
      const char* errMsg = "Unknown Error";
      if (error == OTA_AUTH_ERROR)         errMsg = "Auth Failed";
      else if (error == OTA_BEGIN_ERROR)   errMsg = "Begin Failed";
      else if (error == OTA_CONNECT_ERROR) errMsg = "Connect Failed";
      else if (error == OTA_RECEIVE_ERROR) errMsg = "Receive Failed";
      else if (error == OTA_END_ERROR)     errMsg = "End Failed";
      Serial.printf("[OTA] Error: %s\n", errMsg);
      if (xSemaphoreTake(StateManager::displayMutex, pdMS_TO_TICKS(MUTEX_TIMEOUT))) {
        display.clear();
        display.printCentered("OTA Error!", 0);
        display.printCentered(errMsg, 1);
        xSemaphoreGive(StateManager::displayMutex);
      }
      vTaskDelay(pdMS_TO_TICKS(3000));
      StateManager::otaInProgress = false;
    });

    ArduinoOTA.begin();
    MDNS.begin(OTA_HOSTNAME);
    Serial.println("[OTA] Ready");
  } else {
    Serial.println("[WiFi] Failed - OTA unavailable");
    if (StateManager::scheduleEnabled) {
      StateManager::scheduleEnabled = false;
      Serial.println("[Schedule] Disabled (no WiFi/NTP)");
    }
  }

  // BLE init (always, regardless of WiFi)
  bleCmd.begin();

  // Transition to clock display
  ledManager.setMode((LEDMode)StateManager::ledMode);
  StateManager::statusStage = 3;
  StateManager::screenClearedAfterConnect = false;

  if (xSemaphoreTake(StateManager::displayMutex, pdMS_TO_TICKS(MUTEX_TIMEOUT))) {
    display.clear();
    xSemaphoreGive(StateManager::displayMutex);
  }

  Serial.println("[Core0] Entering main loop");

  // Main loop
  static bool wifiServicesInitialized = wifiOk;
  static bool wasConnected = wifiOk;

  for (;;) {
    bool nowConnected = (WiFi.status() == WL_CONNECTED);

    if (nowConnected) {
      // First-time WiFi services init (covers late connect after boot failure)
      if (!wifiServicesInitialized) {
        wifiServicesInitialized = true;
        WiFi.setSleep(false);
        Serial.printf("[WiFi] Late connect - IP: %s\n", WiFi.localIP().toString().c_str());

        timeManager.begin();
        if (timeManager.syncNTP()) {
          Serial.println("[NTP] Synced (late)");
        }

        ArduinoOTA.setHostname(OTA_HOSTNAME);
        if (strlen(OTA_PASSWORD) > 0) ArduinoOTA.setPassword(OTA_PASSWORD);
        ArduinoOTA.begin();
        MDNS.begin(OTA_HOSTNAME);
        Serial.println("[OTA] Ready (late)");
      }

      ArduinoOTA.handle();
    }

    static unsigned long lastLedUpdate = 0;
    if (millis() - lastLedUpdate >= LED_UPDATE_RATE) {
      lastLedUpdate = millis();
      ledManager.update();
    }

    static unsigned long lastNtpUpdate = 0;
    if (wifiServicesInitialized && nowConnected && millis() - lastNtpUpdate >= 1000) {
      lastNtpUpdate = millis();
      timeManager.update();
    }

    // Reconnect WiFi if lost (or keep retrying after boot failure)
    if (!nowConnected) {
      static unsigned long lastReconnect = 0;
      if (millis() - lastReconnect > 30000) {
        lastReconnect = millis();
        Serial.println("[WiFi] Reconnecting...");
        WiFi.reconnect();
      }
    }
    if (nowConnected && !wasConnected) {
      Serial.printf("[WiFi] Reconnected - IP: %s\n", WiFi.localIP().toString().c_str());
      MDNS.begin(OTA_HOSTNAME);
    }
    wasConnected = nowConnected;

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ── Core 1: Display & Clock Update ──────────────────────────────

void updateClockTask(void* parameter) {
  Serial.println("[Display] Task started on Core 1");

  unsigned long previousMillis = 0;
  unsigned long previousDotMillis = 0;
  unsigned long dotStaticMillis = 0;
  bool dotVisible = false;
  char lastPeriod[4] = "";
  int lastAppliedBrightness = -1;
  bool timeChanged = false;
  bool lastGpio10State = true;
  bool lastClockEnabled = false;
  bool needsRedraw = false;
  bool meetingFlashActive = false;
  bool meetingFlashHigh = false;
  int meetingFlashSavedBright = -1;

  for (;;) {
    if (StateManager::otaInProgress) {
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }

    unsigned long currentMillis = millis();

    if (StateManager::statusStage == 0) {
      display.showStatus("Connecting WiFi    ", 1);
    } else if (StateManager::statusStage == 10) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    } else if (StateManager::statusStage == 1) {
      display.showStatus("Trying NTP server  ", 1);
    } else if (StateManager::statusStage == 11) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    } else if (StateManager::statusStage == 3) {
      if (!StateManager::screenClearedAfterConnect) {
        display.clear();
        StateManager::screenClearedAfterConnect = true;
        needsRedraw = true;
      }
    }

    timeChanged = false;
    if (currentMillis - previousMillis >= 1000) {
      previousMillis = currentMillis;
      timeChanged = true;

      time_t rawTime = (time_t)timeManager.getEpochTime();
      struct tm* ptm = gmtime(&rawTime);
      if (ptm) {
        StateManager::hours = ptm->tm_hour;
        StateManager::minutes = ptm->tm_min;
        StateManager::seconds = ptm->tm_sec;
      }

      if (StateManager::colonBlink) {
        StateManager::colonOn = !StateManager::colonOn;
      } else {
        StateManager::colonOn = true;
      }

      // Schedule check
      if (StateManager::scheduleEnabled) {
        time_t rt = (time_t)timeManager.getEpochTime();
        struct tm* pt = localtime(&rt);
        if (pt) {
          int dow = pt->tm_wday;
          int curMin = pt->tm_hour * 60 + pt->tm_min;
          int onMin = StateManager::scheduleOnHour * 60 + StateManager::scheduleOnMinute;
          int offMin = StateManager::scheduleOffHour * 60 + StateManager::scheduleOffMinute;
          bool todayActive = StateManager::scheduleDays[dow];

          bool shouldBeOn = false;
          if (todayActive) {
            if (onMin < offMin) {
              shouldBeOn = (curMin >= onMin && curMin < offMin);
            } else {
              shouldBeOn = (curMin >= onMin || curMin < offMin);
            }
          }

          if (shouldBeOn && !StateManager::gpio10State) {
            display.powerOn();
          } else if (!shouldBeOn && StateManager::gpio10State) {
            display.powerOff();
          }
        }
      }

      // Meeting reminders
      if (StateManager::seconds == 0 && !StateManager::meetingReminderActive
          && meetingManager.getCount() > 0 && StateManager::statusStage == 3) {
        time_t rt = (time_t)timeManager.getEpochTime();
        struct tm* pt = localtime(&rt);
        if (pt) {
          char mtgTitle[21], mtgTime[21];
          int mtgH, mtgM, mtgIdx = -1;
          if (meetingManager.checkReminders(
                pt->tm_hour, pt->tm_min, pt->tm_wday,
                pt->tm_year + 1900, pt->tm_mon + 1, pt->tm_mday,
                mtgTitle, mtgTime, &mtgH, &mtgM, &mtgIdx)) {

            // Snapshot current clock display for animation
            char oldLine1[21], oldLine2[21];
            char timeStr[21], period[4] = "";
            int dh = StateManager::hours;
            if (!StateManager::use24Hour) {
              if (dh == 0) { dh = 12; strncpy(period, "AM", 4); }
              else if (dh == 12) { strncpy(period, "PM", 4); }
              else if (dh > 12) { dh -= 12; strncpy(period, "PM", 4); }
              else { strncpy(period, "AM", 4); }
              snprintf(timeStr, 21, "%02d:%02d:%02d %s", dh, StateManager::minutes, StateManager::seconds, period);
            } else {
              snprintf(timeStr, 21, "%02d:%02d:%02d", dh, StateManager::minutes, StateManager::seconds);
            }
            int ts = (DISPLAY_COLS - strlen(timeStr)) / 2;
            if (ts < 0) ts = 0;
            memset(oldLine1, ' ', DISPLAY_COLS); oldLine1[DISPLAY_COLS] = '\0';
            strncpy(oldLine1 + ts, timeStr, strlen(timeStr));

            char dateStr[21];
            timeManager.formatDate(dateStr, sizeof(dateStr));
            int ds = (DISPLAY_COLS - strlen(dateStr)) / 2;
            if (ds < 0) ds = 0;
            memset(oldLine2, ' ', DISPLAY_COLS); oldLine2[DISPLAY_COLS] = '\0';
            strncpy(oldLine2 + ds, dateStr, strlen(dateStr));

            strncpy(StateManager::prevClockLine1, oldLine1, 21);
            strncpy(StateManager::prevClockLine2, oldLine2, 21);

            strncpy(StateManager::customLine1, mtgTitle, 21);
            strncpy(StateManager::customLine2, mtgTime, 21);
            StateManager::customMode = true;
            StateManager::meetingReminderActive = true;
            StateManager::meetingReminderStart = currentMillis;
            StateManager::meetingTargetHour = mtgH;
            StateManager::meetingTargetMinute = mtgM;
            StateManager::meetingReminderIndex = mtgIdx;

            animation.matrixCurtain(mtgTitle, mtgTime, oldLine1, oldLine2);
          }

          if (pt->tm_hour == 0 && pt->tm_min == 0) {
            meetingManager.resetShownFlags();
          }
        }
      }

      // Auto-restore clock once the meeting time has passed
      bool meetingTimePassed = false;
      if (StateManager::meetingReminderActive) {
        int curTotalMin = StateManager::hours * 60 + StateManager::minutes;
        int mtgTotalMin = StateManager::meetingTargetHour * 60 + StateManager::meetingTargetMinute;
        meetingTimePassed = (curTotalMin >= mtgTotalMin);
      }
      if (StateManager::meetingReminderActive && meetingTimePassed) {

        char oldLine1[21], oldLine2[21];
        int l1len = strlen(StateManager::customLine1);
        int l2len = strlen(StateManager::customLine2);
        int l1s = (DISPLAY_COLS - l1len) / 2;
        int l2s = (DISPLAY_COLS - l2len) / 2;
        if (l1s < 0) l1s = 0;
        if (l2s < 0) l2s = 0;
        memset(oldLine1, ' ', DISPLAY_COLS); oldLine1[DISPLAY_COLS] = '\0';
        memset(oldLine2, ' ', DISPLAY_COLS); oldLine2[DISPLAY_COLS] = '\0';
        strncpy(oldLine1 + l1s, StateManager::customLine1, l1len);
        strncpy(oldLine2 + l2s, StateManager::customLine2, l2len);

        StateManager::animationInProgress = true;
        animation.matrixCurtain(StateManager::prevClockLine1, StateManager::prevClockLine2,
                                oldLine1, oldLine2);
        StateManager::animationInProgress = false;

        StateManager::customMode = false;
        StateManager::meetingReminderActive = false;
        StateManager::forceSeamlessUpdate = true;

        int expiredIdx = StateManager::meetingReminderIndex;
        if (expiredIdx >= 0 && expiredIdx < meetingManager.getCount()) {
          Serial.printf("[Meetings] Removing expired meeting #%d\n", expiredIdx + 1);
          meetingManager.removeMeeting(expiredIdx);
        }
        StateManager::meetingReminderIndex = -1;
        Serial.println("[Meetings] Reminder expired, restoring clock");

        // Stop brightness flash and restore
        if (meetingFlashActive) {
          meetingFlashActive = false;
          if (meetingFlashSavedBright >= 0) {
            StateManager::uiBrightness = meetingFlashSavedBright;
            meetingFlashSavedBright = -1;
          }
        }
      }

      // Brightness flash during the last minute before meeting
      if (StateManager::meetingReminderActive) {
        time_t flashRt = (time_t)timeManager.getEpochTime();
        struct tm* flashTm = localtime(&flashRt);
        if (flashTm) {
          int curTotalMin = flashTm->tm_hour * 60 + flashTm->tm_min;
          int mtgTotalMin = StateManager::meetingTargetHour * 60 + StateManager::meetingTargetMinute;

          if (curTotalMin == mtgTotalMin - 1) {
            if (!meetingFlashActive) {
              meetingFlashActive = true;
              meetingFlashSavedBright = StateManager::uiBrightness;
              meetingFlashHigh = false;
            }
            meetingFlashHigh = !meetingFlashHigh;
            int flashBright = meetingFlashHigh ? 100 : 50;
            StateManager::uiBrightness = flashBright;
          } else if (meetingFlashActive && curTotalMin >= mtgTotalMin) {
            meetingFlashActive = false;
            if (meetingFlashSavedBright >= 0) {
              StateManager::uiBrightness = meetingFlashSavedBright;
              meetingFlashSavedBright = -1;
            }
          }
        }
      }
    }

    bool localCustomMode;
    char localL1[21], localL2[21];
    bool localClockEnabled;
    bool localUse24;
    int localBright;

    if (xSemaphoreTake(StateManager::displayMutex, pdMS_TO_TICKS(MUTEX_TIMEOUT))) {
      localCustomMode = StateManager::customMode;
      strncpy(localL1, StateManager::customLine1, sizeof(localL1));
      strncpy(localL2, StateManager::customLine2, sizeof(localL2));
      localClockEnabled = StateManager::clockEnabled;
      localUse24 = StateManager::use24Hour;
      localBright = StateManager::uiBrightness;
      xSemaphoreGive(StateManager::displayMutex);
    } else {
      localCustomMode = StateManager::customMode;
      strncpy(localL1, StateManager::customLine1, sizeof(localL1));
      strncpy(localL2, StateManager::customLine2, sizeof(localL2));
      localClockEnabled = StateManager::clockEnabled;
      localUse24 = StateManager::use24Hour;
      localBright = StateManager::uiBrightness;
    }

    bool currentGpio10 = StateManager::gpio10State;
    if (!lastGpio10State && currentGpio10) {
      vTaskDelay(pdMS_TO_TICKS(200));
      display.getVFD()->begin(DISPLAY_COLS, DISPLAY_ROWS);
      taskYIELD();
      vTaskDelay(pdMS_TO_TICKS(50));
      display.getVFD()->clear();
      taskYIELD();
      vTaskDelay(pdMS_TO_TICKS(50));
      display.resetAndApplyBrightness(StateManager::uiBrightness);
      taskYIELD();
      needsRedraw = true;
    }
    lastGpio10State = currentGpio10;

    if (localClockEnabled != lastClockEnabled) {
      if (localClockEnabled) needsRedraw = true;
      lastClockEnabled = localClockEnabled;
    }

    if (!currentGpio10) {
      vTaskDelay(pdMS_TO_TICKS(DISPLAY_UPDATE_RATE));
      continue;
    }

    if (localBright != lastAppliedBrightness) {
      lastAppliedBrightness = localBright;
      display.setBrightness(localBright);
    }

    if (localCustomMode) {
      if (StateManager::animationInProgress) {
        vTaskDelay(pdMS_TO_TICKS(20));
        continue;
      }
      if (timeChanged) {
        display.printCentered(localL1, 0);
        display.printCentered(localL2, 1);
      }
    } else {
      if (localClockEnabled && StateManager::statusStage == 3) {
        int h, m, s;
        timeManager.getTime(h, m, s);
        bool localForceClear = StateManager::forceClearTopRow;
        bool forceUpdate = needsRedraw || StateManager::forceSeamlessUpdate;

        display.showClock(h, m, s, localUse24, StateManager::colonOn,
                         lastPeriod, localForceClear, forceUpdate);
        StateManager::forceClearTopRow = localForceClear;
        display.showDate(timeManager.getEpochTime(), forceUpdate);

        if (needsRedraw) needsRedraw = false;
        if (StateManager::forceSeamlessUpdate) StateManager::forceSeamlessUpdate = false;
      } else {
        display.clearRow(0);
        display.clearRow(1);
      }
    }

    // Status dot for pre-clock stages
    if (!localCustomMode && StateManager::statusStage < 3) {
      if (StateManager::syncDot == 1) {
        if (currentMillis - previousDotMillis >= 500) {
          previousDotMillis = currentMillis;
          dotVisible = !dotVisible;
          display.printAt(dotVisible ? "." : " ", 19, 1);
        }
      } else if (StateManager::syncDot == 2) {
        display.printAt(".", 19, 1);
        if (dotStaticMillis == 0) dotStaticMillis = currentMillis;
        if (currentMillis - dotStaticMillis >= 2000) {
          StateManager::syncDot = 0;
          display.printAt(" ", 19, 1);
        }
      }
    } else {
      if (!localCustomMode) {
        display.printAt(" ", 19, 1);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(DISPLAY_UPDATE_RATE));
  }
}
