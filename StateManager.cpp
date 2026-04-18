#include "StateManager.h"

volatile int StateManager::hours = 12;
volatile int StateManager::minutes = 0;
volatile int StateManager::seconds = 0;
volatile bool StateManager::ntpSynced = false;
volatile bool StateManager::colonOn = true;

bool StateManager::customMode = false;
char StateManager::customMessage[41] = "";
char StateManager::customLine1[21] = "";
char StateManager::customLine2[21] = "";
char StateManager::prevClockLine1[21] = "";
char StateManager::prevClockLine2[21] = "";

int StateManager::uiBrightness = 10;
uint8_t StateManager::vfdFunctionSetReg = 0;
volatile bool StateManager::clockEnabled = true;
volatile bool StateManager::use24Hour = false;
volatile bool StateManager::colonBlink = false;
volatile long StateManager::tzOffsetSeconds = 19800;

bool StateManager::scheduleEnabled = false;
int StateManager::scheduleOnHour = 8;
int StateManager::scheduleOnMinute = 0;
int StateManager::scheduleOffHour = 22;
int StateManager::scheduleOffMinute = 0;
bool StateManager::scheduleDays[7] = {true, true, true, true, true, true, true};

volatile int StateManager::syncDot = 1;
volatile bool StateManager::animationInProgress = false;
volatile bool StateManager::gpio10State = true;
volatile int StateManager::statusStage = 0;
volatile bool StateManager::clearedAfterConnect = false;
volatile bool StateManager::screenClearedAfterConnect = false;
volatile bool StateManager::forceClearTopRow = false;
volatile bool StateManager::forceSeamlessUpdate = false;

int StateManager::ledMode = 6;  // LED_SYNC_STATE
uint8_t StateManager::ledRed = 0;
uint8_t StateManager::ledGreen = 255;
uint8_t StateManager::ledBlue = 0;
uint8_t StateManager::ledBrightness = 128;

volatile bool StateManager::meetingReminderActive = false;
volatile unsigned long StateManager::meetingReminderStart = 0;
volatile int StateManager::meetingTargetHour = 0;
volatile int StateManager::meetingTargetMinute = 0;
volatile int StateManager::meetingReminderIndex = -1;
volatile bool StateManager::otaInProgress = false;
volatile bool StateManager::bleConnected = false;

TaskHandle_t StateManager::wifiNtpTaskHandle = NULL;
SemaphoreHandle_t StateManager::displayMutex = NULL;

void StateManager::init() {
  displayMutex = xSemaphoreCreateMutex();
  if (!displayMutex) {
    Serial.println("Failed to create display mutex");
  }
}
