#ifndef BLE_COMMAND_HANDLER_H
#define BLE_COMMAND_HANDLER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "config.h"
#include "StateManager.h"

class DisplayManager;
class Animation;
class TimeManager;
class LEDManager;
class MeetingManager;

class BLECommandHandler {
public:
  BLECommandHandler(DisplayManager* display, Animation* anim,
                    TimeManager* time, LEDManager* led,
                    MeetingManager* meetings);

  void begin();
  void send(const char* text);
  void sendLine(const char* text);

  void handleCommand(String& input);

private:
  DisplayManager* display;
  Animation* animation;
  TimeManager* timeManager;
  LEDManager* ledManager;
  MeetingManager* meetingManager;

  BLEServer* pServer;
  BLECharacteristic* pTxCharacteristic;

  void cmdStatus();
  void cmdMsg(const String& text);
  void cmdLine(int row, const String& text);
  void cmdRestore();
  void cmdBright(const String& arg);
  void cmd24h(bool on);
  void cmdBlink(bool on);
  void cmdTz(const String& arg);
  void cmdLed(const String& args);
  void cmdSchedToggle(bool on);
  void cmdSchedSet(const String& args);
  void cmdMeetSync(const String& json);
  void cmdMeetAdd(const String& args);
  void cmdMeetRemove(const String& arg);
  void cmdMeetClear();
  void cmdMeetLead(const String& arg);
  void cmdMeetList();
  void cmdPower(bool on);
  void cmdSync();
  void cmdReboot();
  void cmdWifi();
  void cmdHelp();

  void splitMessageToLines(const char* msg);
  void buildClockSnapshot(char* line1, char* line2);
  void buildCurrentDisplay(char* old1, char* old2, const char* clock1, const char* clock2);
  void saveScheduleSettings();
};

extern BLECommandHandler* g_bleHandler;

#endif // BLE_COMMAND_HANDLER_H
