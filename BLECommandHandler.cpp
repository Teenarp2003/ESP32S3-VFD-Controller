#include "BLECommandHandler.h"
#include "DisplayManager.h"
#include "Animation.h"
#include "TimeManager.h"
#include "LEDManager.h"
#include "MeetingManager.h"
#include <WiFi.h>
#include <Preferences.h>

#define NUS_SERVICE_UUID      "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_CHARACTERISTIC "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_TX_CHARACTERISTIC "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

BLECommandHandler* g_bleHandler = nullptr;
static String bleRxBuffer = "";

// ── BLE Callbacks ────────────────────────────────────────────────

class ServerCB : public BLEServerCallbacks {
  void onConnect(BLEServer* s) override {
    StateManager::bleConnected = true;
    Serial.println("[BLE] Client connected");
  }
  void onDisconnect(BLEServer* s) override {
    StateManager::bleConnected = false;
    Serial.println("[BLE] Client disconnected");
    BLEDevice::startAdvertising();
  }
};

class RxCB : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pChar) override {
    String value = pChar->getValue().c_str();
    for (size_t i = 0; i < value.length(); i++) {
      char c = value[i];
      if (c == '\n' || c == '\r') {
        if (bleRxBuffer.length() > 0 && g_bleHandler) {
          g_bleHandler->handleCommand(bleRxBuffer);
          bleRxBuffer = "";
        }
      } else {
        bleRxBuffer += c;
      }
    }
    // Buffer across BLE chunks -- only execute when \n arrives.
    // Make sure your BLE terminal app appends newline to each send.
  }
};

// ── Constructor / Init ───────────────────────────────────────────

BLECommandHandler::BLECommandHandler(DisplayManager* disp, Animation* anim,
                                     TimeManager* time, LEDManager* led,
                                     MeetingManager* meetings)
  : display(disp), animation(anim), timeManager(time),
    ledManager(led), meetingManager(meetings),
    pServer(nullptr), pTxCharacteristic(nullptr) {
  g_bleHandler = this;
}

void BLECommandHandler::begin() {
  BLEDevice::init(BT_DEVICE_NAME);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCB());

  BLEService* svc = pServer->createService(NUS_SERVICE_UUID);

  pTxCharacteristic = svc->createCharacteristic(
    NUS_TX_CHARACTERISTIC,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic* pRx = svc->createCharacteristic(
    NUS_RX_CHARACTERISTIC,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );
  pRx->setCallbacks(new RxCB());

  svc->start();

  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(NUS_SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial.printf("[BLE] Started as \"%s\"\n", BT_DEVICE_NAME);
}

// ── Send helpers ─────────────────────────────────────────────────

void BLECommandHandler::send(const char* text) {
  if (!StateManager::bleConnected || !pTxCharacteristic) return;
  size_t len = strlen(text);
  for (size_t off = 0; off < len; off += 20) {
    size_t chunk = len - off;
    if (chunk > 20) chunk = 20;
    pTxCharacteristic->setValue((uint8_t*)(text + off), chunk);
    pTxCharacteristic->notify();
    delay(10);
  }
}

void BLECommandHandler::sendLine(const char* text) {
  send(text);
  send("\n");
}

// ── Command Dispatcher ───────────────────────────────────────────

void BLECommandHandler::handleCommand(String& input) {
  input.trim();
  if (input.length() == 0) return;

  Serial.printf("[BLE] Cmd: %s\n", input.c_str());

  if (input.equalsIgnoreCase("ping")) {
    sendLine("pong");
  } else if (input.equalsIgnoreCase("status")) {
    cmdStatus();
  } else if (input.startsWith("msg ") || input.startsWith("MSG ")) {
    cmdMsg(input.substring(4));
  } else if (input.startsWith("line1 ") || input.startsWith("LINE1 ")) {
    cmdLine(0, input.substring(6));
  } else if (input.startsWith("line2 ") || input.startsWith("LINE2 ")) {
    cmdLine(1, input.substring(6));
  } else if (input.equalsIgnoreCase("restore")) {
    cmdRestore();
  } else if (input.startsWith("bright ") || input.startsWith("BRIGHT ")) {
    cmdBright(input.substring(7));
  } else if (input.equalsIgnoreCase("24h on")) {
    cmd24h(true);
  } else if (input.equalsIgnoreCase("24h off")) {
    cmd24h(false);
  } else if (input.equalsIgnoreCase("blink on")) {
    cmdBlink(true);
  } else if (input.equalsIgnoreCase("blink off")) {
    cmdBlink(false);
  } else if (input.startsWith("tz ") || input.startsWith("TZ ")) {
    cmdTz(input.substring(3));
  } else if (input.startsWith("led ") || input.startsWith("LED ")) {
    cmdLed(input.substring(4));
  } else if (input.equalsIgnoreCase("sched on")) {
    cmdSchedToggle(true);
  } else if (input.equalsIgnoreCase("sched off")) {
    cmdSchedToggle(false);
  } else if (input.startsWith("sched set ") || input.startsWith("SCHED SET ")) {
    cmdSchedSet(input.substring(10));
  } else if (input.startsWith("meet sync ") || input.startsWith("MEET SYNC ")) {
    cmdMeetSync(input.substring(10));
  } else if (input.startsWith("meet add ") || input.startsWith("MEET ADD ")) {
    cmdMeetAdd(input.substring(9));
  } else if (input.startsWith("meet remove ") || input.startsWith("MEET REMOVE ")) {
    cmdMeetRemove(input.substring(12));
  } else if (input.equalsIgnoreCase("meet clear")) {
    cmdMeetClear();
  } else if (input.startsWith("meet lead ") || input.startsWith("MEET LEAD ")) {
    cmdMeetLead(input.substring(10));
  } else if (input.equalsIgnoreCase("meet list")) {
    cmdMeetList();
  } else if (input.equalsIgnoreCase("power on")) {
    cmdPower(true);
  } else if (input.equalsIgnoreCase("power off")) {
    cmdPower(false);
  } else if (input.equalsIgnoreCase("sync")) {
    cmdSync();
  } else if (input.equalsIgnoreCase("reboot")) {
    cmdReboot();
  } else if (input.equalsIgnoreCase("wifi")) {
    cmdWifi();
  } else if (input.equalsIgnoreCase("help")) {
    cmdHelp();
  } else {
    sendLine("Unknown command. Type 'help'.");
  }
}

// ── Command Implementations ──────────────────────────────────────

void BLECommandHandler::cmdStatus() {
  char buf[64];
  sendLine(StateManager::customMode ? "Mode: Custom" : "Mode: Clock");
  snprintf(buf, sizeof(buf), "Time: %02d:%02d:%02d",
           StateManager::hours, StateManager::minutes, StateManager::seconds);
  sendLine(buf);
  char dateBuf[21];
  timeManager->formatDate(dateBuf, sizeof(dateBuf));
  snprintf(buf, sizeof(buf), "Date: %s", dateBuf);
  sendLine(buf);
  snprintf(buf, sizeof(buf), "24h: %s  Blink: %s",
           StateManager::use24Hour ? "on" : "off",
           StateManager::colonBlink ? "on" : "off");
  sendLine(buf);
  snprintf(buf, sizeof(buf), "Bright: %d  TZ: %ld",
           StateManager::uiBrightness, StateManager::tzOffsetSeconds);
  sendLine(buf);
  snprintf(buf, sizeof(buf), "NTP: %s  Power: %s",
           StateManager::ntpSynced ? "synced" : "no",
           StateManager::gpio10State ? "on" : "off");
  sendLine(buf);
  snprintf(buf, sizeof(buf), "LED mode: %d  Sched: %s",
           StateManager::ledMode,
           StateManager::scheduleEnabled ? "on" : "off");
  sendLine(buf);
  snprintf(buf, sizeof(buf), "Meetings: %d", meetingManager->getCount());
  sendLine(buf);
}

void BLECommandHandler::cmdMsg(const String& text) {
  String trimmed = text;
  trimmed.trim();
  if (trimmed.length() > 40) trimmed = trimmed.substring(0, 40);

  if (xSemaphoreTake(StateManager::displayMutex, pdMS_TO_TICKS(MUTEX_TIMEOUT))) {
    char clockLine1[21], clockLine2[21], oldLine1[21], oldLine2[21];
    buildClockSnapshot(clockLine1, clockLine2);
    strncpy(StateManager::prevClockLine1, clockLine1, 21);
    strncpy(StateManager::prevClockLine2, clockLine2, 21);
    buildCurrentDisplay(oldLine1, oldLine2, clockLine1, clockLine2);

    trimmed.toCharArray(StateManager::customMessage, sizeof(StateManager::customMessage));
    splitMessageToLines(StateManager::customMessage);
    StateManager::customMode = true;

    animation->matrixCurtain(StateManager::customLine1, StateManager::customLine2,
                             oldLine1, oldLine2);
    xSemaphoreGive(StateManager::displayMutex);
    sendLine("OK");
  } else {
    sendLine("ERR: display busy");
  }
}

void BLECommandHandler::cmdLine(int row, const String& text) {
  String trimmed = text;
  trimmed.trim();
  if ((int)trimmed.length() > DISPLAY_COLS) trimmed = trimmed.substring(0, DISPLAY_COLS);

  if (xSemaphoreTake(StateManager::displayMutex, pdMS_TO_TICKS(MUTEX_TIMEOUT))) {
    if (row == 0) {
      strncpy(StateManager::customLine1, trimmed.c_str(), 21);
    } else {
      strncpy(StateManager::customLine2, trimmed.c_str(), 21);
    }
    StateManager::customMode = true;
    display->clearRow(row);
    display->printCentered(trimmed.c_str(), row);
    xSemaphoreGive(StateManager::displayMutex);
    sendLine("OK");
  } else {
    sendLine("ERR: display busy");
  }
}

void BLECommandHandler::cmdRestore() {
  if (xSemaphoreTake(StateManager::displayMutex, pdMS_TO_TICKS(MUTEX_TIMEOUT))) {
    char oldLine1[21], oldLine2[21];
    int l1len = strlen(StateManager::customLine1);
    int l2len = strlen(StateManager::customLine2);
    int l1start = (DISPLAY_COLS - l1len) / 2;
    int l2start = (DISPLAY_COLS - l2len) / 2;
    if (l1start < 0) l1start = 0;
    if (l2start < 0) l2start = 0;

    memset(oldLine1, ' ', DISPLAY_COLS);
    memset(oldLine2, ' ', DISPLAY_COLS);
    oldLine1[DISPLAY_COLS] = '\0';
    oldLine2[DISPLAY_COLS] = '\0';
    strncpy(oldLine1 + l1start, StateManager::customLine1, l1len);
    strncpy(oldLine2 + l2start, StateManager::customLine2, l2len);

    StateManager::animationInProgress = true;
    animation->matrixCurtain(StateManager::prevClockLine1, StateManager::prevClockLine2,
                             oldLine1, oldLine2);
    StateManager::animationInProgress = false;

    StateManager::customMode = false;
    StateManager::screenClearedAfterConnect = true;
    StateManager::clearedAfterConnect = true;
    StateManager::statusStage = 3;
    StateManager::forceSeamlessUpdate = true;

    xSemaphoreGive(StateManager::displayMutex);
    delay(100);
    sendLine("OK: clock restored");
  } else {
    sendLine("ERR: display busy");
  }
}

void BLECommandHandler::cmdBright(const String& arg) {
  int b = arg.toInt();
  if (b < 0) b = 0;
  if (b > 100) b = 100;
  if (xSemaphoreTake(StateManager::displayMutex, pdMS_TO_TICKS(MUTEX_TIMEOUT))) {
    StateManager::uiBrightness = b;
    xSemaphoreGive(StateManager::displayMutex);
  }
  char buf[32];
  snprintf(buf, sizeof(buf), "OK: brightness %d", b);
  sendLine(buf);
}

void BLECommandHandler::cmd24h(bool on) {
  if (xSemaphoreTake(StateManager::displayMutex, pdMS_TO_TICKS(MUTEX_TIMEOUT))) {
    if (on != StateManager::use24Hour) {
      StateManager::use24Hour = on;
      StateManager::forceClearTopRow = true;
    }
    xSemaphoreGive(StateManager::displayMutex);
  }
  sendLine(on ? "OK: 24h on" : "OK: 24h off");
}

void BLECommandHandler::cmdBlink(bool on) {
  StateManager::colonBlink = on;
  sendLine(on ? "OK: blink on" : "OK: blink off");
}

void BLECommandHandler::cmdTz(const String& arg) {
  long tz = arg.toInt();
  timeManager->setTimezone(tz);
  char buf[32];
  snprintf(buf, sizeof(buf), "OK: tz %ld", tz);
  sendLine(buf);
}

void BLECommandHandler::cmdLed(const String& args) {
  // led <mode> [r g b] [brightness]
  // mode: off solid breathing rainbow pulse status sync
  String a = args;
  a.trim();

  const char* modeNames[] = {"off", "solid", "breathing", "rainbow", "pulse", "status", "sync"};
  int mode = -1;

  // Try name first
  String firstWord = a;
  int spaceIdx = a.indexOf(' ');
  if (spaceIdx > 0) firstWord = a.substring(0, spaceIdx);
  firstWord.toLowerCase();

  for (int i = 0; i < 7; i++) {
    if (firstWord == modeNames[i]) { mode = i; break; }
  }
  if (mode == -1) {
    mode = firstWord.toInt();
    if (mode < 0 || mode > 6) {
      sendLine("ERR: mode 0-6 or name");
      return;
    }
  }

  StateManager::ledMode = mode;

  // Parse optional r g b brightness after mode word
  String rest = (spaceIdx > 0) ? a.substring(spaceIdx + 1) : "";
  rest.trim();

  if (rest.length() > 0) {
    int r = 0, g = 0, b = 0, br = -1;
    int n = sscanf(rest.c_str(), "%d %d %d %d", &r, &g, &b, &br);
    if (n >= 3) {
      StateManager::ledRed = constrain(r, 0, 255);
      StateManager::ledGreen = constrain(g, 0, 255);
      StateManager::ledBlue = constrain(b, 0, 255);
      ledManager->setColor(StateManager::ledRed, StateManager::ledGreen, StateManager::ledBlue);
    }
    if (n >= 4 && br >= 0) {
      StateManager::ledBrightness = constrain(br, 0, 255);
      ledManager->setBrightness(StateManager::ledBrightness);
    }
  }

  ledManager->setMode((LEDMode)mode);

  char buf[40];
  snprintf(buf, sizeof(buf), "OK: led %s", modeNames[mode]);
  sendLine(buf);
}

void BLECommandHandler::cmdSchedToggle(bool on) {
  StateManager::scheduleEnabled = on;

  if (!on) {
    if (xSemaphoreTake(StateManager::displayMutex, pdMS_TO_TICKS(MUTEX_TIMEOUT))) {
      if (!StateManager::gpio10State) {
        display->powerOn();
      }
      xSemaphoreGive(StateManager::displayMutex);
    }
  }

  saveScheduleSettings();
  sendLine(on ? "OK: sched on" : "OK: sched off");
}

void BLECommandHandler::cmdSchedSet(const String& args) {
  // sched set 8:00 22:00 0111110
  // Format: onH:onM offH:offM dayBits(SMTWTFS)
  int onH, onM, offH, offM;
  char dayStr[8] = "1111111";

  int n = sscanf(args.c_str(), "%d:%d %d:%d %7s", &onH, &onM, &offH, &offM, dayStr);
  if (n < 4) {
    sendLine("ERR: sched set H:M H:M [days]");
    return;
  }

  StateManager::scheduleOnHour = constrain(onH, 0, 23);
  StateManager::scheduleOnMinute = constrain(onM, 0, 59);
  StateManager::scheduleOffHour = constrain(offH, 0, 23);
  StateManager::scheduleOffMinute = constrain(offM, 0, 59);

  for (int i = 0; i < 7 && dayStr[i]; i++) {
    StateManager::scheduleDays[i] = (dayStr[i] == '1');
  }

  saveScheduleSettings();

  char buf[48];
  snprintf(buf, sizeof(buf), "OK: %02d:%02d-%02d:%02d days=%s",
           StateManager::scheduleOnHour, StateManager::scheduleOnMinute,
           StateManager::scheduleOffHour, StateManager::scheduleOffMinute, dayStr);
  sendLine(buf);
}

void BLECommandHandler::cmdMeetSync(const String& json) {
  if (meetingManager->syncFromJson(json)) {
    char buf[48];
    snprintf(buf, sizeof(buf), "OK: %d meetings (lead=%d min)",
             meetingManager->getCount(), meetingManager->getLeadMinutes());
    sendLine(buf);
  } else {
    sendLine("ERR: bad JSON");
  }
}

void BLECommandHandler::cmdMeetAdd(const String& args) {
  // Format: meet add <title> <when> <HH:MM>
  //   meet add Standup daily 09:30
  //   meet add Standup weekdays 09:30
  //   meet add Standup mon,tue,wed,thu,fri 09:30
  //   meet add Team Lunch 2026-04-17 12:00
  //   meet add Dentist today 15:30
  //   meet add Dentist tomorrow 15:30
  //   meet add Dentist day-after 15:30

  String a = args;
  a.trim();

  // Last token is time (HH:MM), second-to-last is when, rest is title
  int lastSpace = a.lastIndexOf(' ');
  if (lastSpace < 0) { sendLine("ERR: meet add <title> <when> HH:MM"); return; }

  String timeStr = a.substring(lastSpace + 1);
  String rest = a.substring(0, lastSpace);
  rest.trim();

  int secondLastSpace = rest.lastIndexOf(' ');
  if (secondLastSpace < 0) { sendLine("ERR: meet add <title> <when> HH:MM"); return; }

  String daysOrDate = rest.substring(secondLastSpace + 1);
  String title = rest.substring(0, secondLastSpace);
  title.trim();

  // Parse time
  int hour = 0, minute = 0;
  if (sscanf(timeStr.c_str(), "%d:%d", &hour, &minute) != 2) {
    sendLine("ERR: bad time (use HH:MM)");
    return;
  }
  if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
    sendLine("ERR: time out of range");
    return;
  }

  // Truncate title to 20 chars
  if (title.length() > 20) title = title.substring(0, 20);

  // Resolve relative date keywords to YYYY-MM-DD
  String dateLower = daysOrDate;
  dateLower.toLowerCase();

  if (dateLower == "today" || dateLower == "tomorrow" || dateLower == "day-after") {
    time_t epoch = (time_t)timeManager->getEpochTime();
    if (dateLower == "tomorrow") epoch += 86400;
    else if (dateLower == "day-after") epoch += 86400 * 2;
    struct tm* pt = gmtime(&epoch);
    if (pt) {
      char dateBuf[11];
      snprintf(dateBuf, sizeof(dateBuf), "%04d-%02d-%02d",
               pt->tm_year + 1900, pt->tm_mon + 1, pt->tm_mday);
      daysOrDate = String(dateBuf);
    }
  }

  // Determine type
  if (daysOrDate.indexOf('-') >= 0) {
    // Oneshot: parse date YYYY-MM-DD
    int y, mo, d;
    if (sscanf(daysOrDate.c_str(), "%d-%d-%d", &y, &mo, &d) != 3) {
      sendLine("ERR: bad date (YYYY-MM-DD)");
      return;
    }
    if (!meetingManager->addMeeting(title.c_str(), hour, minute, 1, 0, y, mo, d)) {
      sendLine("ERR: max meetings reached");
      return;
    }
    char buf[48];
    snprintf(buf, sizeof(buf), "OK: added \"%s\" %02d:%02d on %04d-%02d-%02d",
             title.c_str(), hour, minute, y, mo, d);
    sendLine(buf);
  } else {
    // Recurring: parse day names
    String daysLower = daysOrDate;
    daysLower.toLowerCase();

    uint8_t mask = 0;
    if (daysLower == "daily" || daysLower == "everyday") {
      mask = DAY_MON | DAY_TUE | DAY_WED | DAY_THU | DAY_FRI | DAY_SAT | DAY_SUN;
    } else if (daysLower == "weekdays") {
      mask = DAY_MON | DAY_TUE | DAY_WED | DAY_THU | DAY_FRI;
    } else if (daysLower == "weekends") {
      mask = DAY_SAT | DAY_SUN;
    } else {
      mask = meetingManager->parseDaysMask(daysLower);
    }

    if (mask == 0) {
      sendLine("ERR: no valid days");
      return;
    }

    if (!meetingManager->addMeeting(title.c_str(), hour, minute, 0, mask)) {
      sendLine("ERR: max meetings reached");
      return;
    }
    char buf[48];
    snprintf(buf, sizeof(buf), "OK: added \"%s\" %02d:%02d (%d total)",
             title.c_str(), hour, minute, meetingManager->getCount());
    sendLine(buf);
  }
}

void BLECommandHandler::cmdMeetRemove(const String& arg) {
  int idx = arg.toInt();
  if (idx < 1 || idx > meetingManager->getCount()) {
    char buf[32];
    snprintf(buf, sizeof(buf), "ERR: pick 1-%d", meetingManager->getCount());
    sendLine(buf);
    return;
  }
  String title = meetingManager->meetings[idx - 1].title;
  meetingManager->removeMeeting(idx - 1);
  char buf[48];
  snprintf(buf, sizeof(buf), "OK: removed \"%s\" (%d left)",
           title.c_str(), meetingManager->getCount());
  sendLine(buf);
}

void BLECommandHandler::cmdMeetClear() {
  meetingManager->clearAll();
  sendLine("OK: all meetings cleared");
}

void BLECommandHandler::cmdMeetLead(const String& arg) {
  int mins = arg.toInt();
  meetingManager->setLeadMinutes(mins);
  char buf[32];
  snprintf(buf, sizeof(buf), "OK: lead time %d min", meetingManager->getLeadMinutes());
  sendLine(buf);
}

void BLECommandHandler::cmdMeetList() {
  char buf[64];
  snprintf(buf, sizeof(buf), "Meetings: %d (lead=%d min)",
           meetingManager->getCount(), meetingManager->getLeadMinutes());
  sendLine(buf);

  for (int i = 0; i < meetingManager->meetingCount; i++) {
    Meeting& m = meetingManager->meetings[i];
    if (!m.valid) continue;

    if (m.type == 1) {
      snprintf(buf, sizeof(buf), " %d. %s %02d:%02d %04d-%02d-%02d",
               i + 1, m.title, m.hour, m.minute, m.year, m.month, m.day);
    } else {
      char days[22] = "";
      if (m.daysMask & DAY_MON) strcat(days, "Mo");
      if (m.daysMask & DAY_TUE) strcat(days, "Tu");
      if (m.daysMask & DAY_WED) strcat(days, "We");
      if (m.daysMask & DAY_THU) strcat(days, "Th");
      if (m.daysMask & DAY_FRI) strcat(days, "Fr");
      if (m.daysMask & DAY_SAT) strcat(days, "Sa");
      if (m.daysMask & DAY_SUN) strcat(days, "Su");
      snprintf(buf, sizeof(buf), " %d. %s %02d:%02d %s",
               i + 1, m.title, m.hour, m.minute, days);
    }
    sendLine(buf);
  }
}

void BLECommandHandler::cmdPower(bool on) {
  if (xSemaphoreTake(StateManager::displayMutex, pdMS_TO_TICKS(MUTEX_TIMEOUT))) {
    if (StateManager::scheduleEnabled) {
      StateManager::scheduleEnabled = false;
      saveScheduleSettings();
    }
    if (on) display->powerOn();
    else     display->powerOff();
    xSemaphoreGive(StateManager::displayMutex);
  }
  sendLine(on ? "OK: power on" : "OK: power off");
}

void BLECommandHandler::cmdSync() {
  timeManager->forceUpdate();
  bool ok = timeManager->syncNTP();
  StateManager::ntpSynced = ok;
  sendLine(ok ? "OK: NTP synced" : "ERR: NTP failed");
}

void BLECommandHandler::cmdReboot() {
  sendLine("Rebooting...");
  delay(500);
  ESP.restart();
}

void BLECommandHandler::cmdWifi() {
  char buf[48];
  if (WiFi.status() == WL_CONNECTED) {
    snprintf(buf, sizeof(buf), "WiFi: %s", WiFi.localIP().toString().c_str());
    sendLine(buf);
    snprintf(buf, sizeof(buf), "RSSI: %d dBm", WiFi.RSSI());
    sendLine(buf);
  } else {
    sendLine("WiFi: disconnected");
  }
  snprintf(buf, sizeof(buf), "OTA: %s", OTA_HOSTNAME);
  sendLine(buf);
}

void BLECommandHandler::cmdHelp() {
  sendLine("=== VFD BLE Clock ===");
  sendLine("ping             - pong");
  sendLine("status           - show state");
  sendLine("msg <text>       - custom msg");
  sendLine("line1/2 <text>   - set line");
  sendLine("restore          - back to clock");
  sendLine("bright <0-100>   - brightness");
  sendLine("24h on/off       - 12/24h mode");
  sendLine("blink on/off     - colon blink");
  sendLine("tz <secs>        - timezone");
  sendLine("led <mode> [rgb] - LED ctrl");
  sendLine("sched on/off     - schedule");
  sendLine("sched set H:M H:M days");
  sendLine("meet add <t> <when> H:M");
  sendLine("meet remove <#>  - delete");
  sendLine("meet clear       - wipe all");
  sendLine("meet lead <min>  - reminder");
  sendLine("meet list        - list all");
  sendLine("meet sync <json> - bulk sync");
  sendLine("power on/off     - VFD power");
  sendLine("sync             - NTP resync");
  sendLine("wifi             - WiFi info");
  sendLine("reboot           - restart");
  sendLine("help             - this list");
}

// ── Helpers ──────────────────────────────────────────────────────

void BLECommandHandler::splitMessageToLines(const char* msg) {
  memset(StateManager::customLine1, 0, sizeof(StateManager::customLine1));
  memset(StateManager::customLine2, 0, sizeof(StateManager::customLine2));
  int len = strlen(msg);
  for (int i = 0; i < DISPLAY_COLS && i < len; i++)
    StateManager::customLine1[i] = msg[i];
  StateManager::customLine1[DISPLAY_COLS] = '\0';
  for (int i = DISPLAY_COLS; i < 40 && i < len; i++)
    StateManager::customLine2[i - DISPLAY_COLS] = msg[i];
  StateManager::customLine2[DISPLAY_COLS] = '\0';
}

void BLECommandHandler::buildClockSnapshot(char* line1, char* line2) {
  char timeStr[21], period[4] = "";
  int displayHour = StateManager::hours;

  if (!StateManager::use24Hour) {
    if (StateManager::hours == 0) {
      displayHour = 12;
      strncpy(period, "AM", sizeof(period));
    } else if (StateManager::hours == 12) {
      strncpy(period, "PM", sizeof(period));
    } else if (StateManager::hours > 12) {
      displayHour = StateManager::hours - 12;
      strncpy(period, "PM", sizeof(period));
    } else {
      strncpy(period, "AM", sizeof(period));
    }
    snprintf(timeStr, sizeof(timeStr), "%02d%c%02d%c%02d %s",
             displayHour, StateManager::colonOn ? ':' : ' ',
             StateManager::minutes, StateManager::colonOn ? ':' : ' ',
             StateManager::seconds, period);
  } else {
    snprintf(timeStr, sizeof(timeStr), "%02d%c%02d%c%02d",
             displayHour, StateManager::colonOn ? ':' : ' ',
             StateManager::minutes, StateManager::colonOn ? ':' : ' ',
             StateManager::seconds);
  }

  int timeStart = (DISPLAY_COLS - strlen(timeStr)) / 2;
  if (timeStart < 0) timeStart = 0;
  memset(line1, ' ', DISPLAY_COLS);
  line1[DISPLAY_COLS] = '\0';
  strncpy(line1 + timeStart, timeStr, strlen(timeStr));

  char dateStr[21];
  timeManager->formatDate(dateStr, sizeof(dateStr));
  int dateStart = (DISPLAY_COLS - strlen(dateStr)) / 2;
  if (dateStart < 0) dateStart = 0;
  memset(line2, ' ', DISPLAY_COLS);
  line2[DISPLAY_COLS] = '\0';
  strncpy(line2 + dateStart, dateStr, strlen(dateStr));
}

void BLECommandHandler::buildCurrentDisplay(char* old1, char* old2,
                                            const char* clock1, const char* clock2) {
  if (StateManager::customMode) {
    int l1len = strlen(StateManager::customLine1);
    int l2len = strlen(StateManager::customLine2);
    int l1start = (DISPLAY_COLS - l1len) / 2;
    int l2start = (DISPLAY_COLS - l2len) / 2;
    if (l1start < 0) l1start = 0;
    if (l2start < 0) l2start = 0;
    memset(old1, ' ', DISPLAY_COLS);
    old1[DISPLAY_COLS] = '\0';
    memset(old2, ' ', DISPLAY_COLS);
    old2[DISPLAY_COLS] = '\0';
    strncpy(old1 + l1start, StateManager::customLine1, l1len);
    strncpy(old2 + l2start, StateManager::customLine2, l2len);
  } else {
    strncpy(old1, clock1, 21);
    strncpy(old2, clock2, 21);
  }
}

void BLECommandHandler::saveScheduleSettings() {
  Preferences prefs;
  prefs.begin("schedule", false);
  prefs.putBool("enabled", StateManager::scheduleEnabled);
  prefs.putInt("onHour", StateManager::scheduleOnHour);
  prefs.putInt("onMin", StateManager::scheduleOnMinute);
  prefs.putInt("offHour", StateManager::scheduleOffHour);
  prefs.putInt("offMin", StateManager::scheduleOffMinute);
  int daysBits = 0;
  for (int i = 0; i < 7; i++) {
    if (StateManager::scheduleDays[i]) daysBits |= (1 << i);
  }
  prefs.putInt("days", daysBits);
  prefs.end();
}
