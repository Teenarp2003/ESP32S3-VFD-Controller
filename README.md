# ESP32S3-VFD-Controller

An ESP32-powered clock using a 20x2 character [PT6314](https://newhavendisplay.com/content/app_notes/PT6314.pdf) Vacuum Fluorescent Display([ M0220MD-202MDAR1-3 ](https://newhavendisplay.com/2x20-character-vfd-module-parallel-interface-dot-matrix-with-controller/)), controlled over BLE. WiFi runs in the background for NTP time sync and OTA firmware updates.

## Features

- **Accurate timekeeping** via NTP with configurable timezone
- **BLE command interface** using the Nordic UART Service (NUS)
- **Meeting reminders** with animated transitions and brightness alerts
- **Display power scheduling** -- auto on/off at set times on set days
- **WS2812B LED indicator** with multiple animation modes
- **Over-the-air updates** via ArduinoOTA
- **12/24-hour mode**, colon blink toggle, adjustable brightness
- **Custom messages** on the display with matrix curtain animation
- **Persistent settings** -- all config survives reboots (stored in NVS flash)

## Hardware

| Component | Description |
|---|---|
| MCU | ESP32 (dual-core, WiFi + BLE) |
| Display | 20x2 VFD driven by PT6314 |
| LED | WS2812B NeoPixel (single) |
| Interface | BLE (Nordic UART Service) |

### Pin Wiring

| Pin | Function |
|---|---|
| GPIO 7 | PT6314 SI (serial data) |
| GPIO 8 | PT6314 STB (strobe/latch) |
| GPIO 9 | PT6314 SCK (serial clock) |
| GPIO 10 | VFD power control (HIGH = on) |
| GPIO 21 | WS2812B LED data |

## Firmware Setup

### Prerequisites

Install the following in the Arduino IDE (or `arduino-cli`):

**Board package:**
- ESP32 by Espressif (`esp32` in Board Manager)

**Libraries (via Library Manager):**
- `PT6314` -- VFD display driver
- `NTPClient` -- NTP time sync
- `ArduinoJson` -- meeting JSON parsing
- `Adafruit NeoPixel` -- WS2812B LED control

The following are built into the ESP32 Arduino core (no install needed):
- `WiFi`, `ESPmDNS`, `ArduinoOTA`, `Preferences`, `BLE`

### Configuration

Edit `config.h` before flashing:

```cpp
// WiFi credentials (used for NTP + OTA only, not BLE)
#define WIFI_SSID "YourNetwork"
#define WIFI_PASSWORD "YourPassword"

// Timezone offset in seconds (default: IST +5:30 = 19800)
#define DEFAULT_TIMEZONE 19800

// OTA update password
#define OTA_PASSWORD "vfdota"

// BLE advertised name
#define BT_DEVICE_NAME "VFD-BLE-Clock"
```

### Flashing

**USB:**
```bash
arduino-cli compile --fqbn esp32:esp32:esp32 VFD_BLE_Clock
arduino-cli upload --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0 VFD_BLE_Clock
```

**OTA (after initial USB flash):**
```bash
arduino-cli upload --fqbn esp32:esp32:esp32 -p vfd-ble-clock.local VFD_BLE_Clock
```

## BLE CLI (`vfd`)

A Python command-line tool for controlling the clock over BLE.

### Install

Requires Python 3.8+.

```bash
cd VFD_BLE_Clock
pip install -e .
```

This installs the `vfd` command globally. Alternatively, run directly:

```bash
pip install bleak prompt_toolkit
python ble_terminal.py
```

### Usage

```bash
vfd                  # scan and connect to "VFD-BLE-Clock"
vfd --name MyClock   # connect to a device with a different name
vfd --timeout 15     # increase scan timeout (default: 10s)
```

The CLI provides:
- **Tab completion** for all commands
- **Command history** (persisted in `~/.vfd_history`)
- **Arrow key navigation**
- **Connection watchdog** -- polls every 10 seconds, attempts one reconnect on drop

Type `help` at the prompt for the full command list. Exit with `Ctrl+D`, `exit`, or `quit`.

## Command Reference

### General

| Command | Description |
|---|---|
| `ping` | Returns `pong` -- connection test |
| `status` | Show current clock state (mode, time, settings, etc.) |
| `help` | Print command list (handled locally, no BLE traffic) |
| `reboot` | Restart the ESP32 |

### Display

| Command | Description |
|---|---|
| `msg <text>` | Display a custom message (up to 40 chars, split across 2 lines) with matrix curtain animation |
| `line1 <text>` | Set line 1 directly (up to 20 chars) |
| `line2 <text>` | Set line 2 directly (up to 20 chars) |
| `restore` | Return to clock display with animation |

### Clock Settings

| Command | Description |
|---|---|
| `bright <0-100>` | Set VFD brightness (0 = dimmest, 100 = full) |
| `24h on` | Switch to 24-hour time format |
| `24h off` | Switch to 12-hour AM/PM format |
| `blink on` | Enable colon blinking (toggles every second) |
| `blink off` | Disable colon blinking (colons always visible) |
| `tz <seconds>` | Set timezone as UTC offset in seconds (e.g. `19800` for IST +5:30, `-18000` for EST -5:00) |
| `sync` | Force an immediate NTP resync |

### Display Power

| Command | Description |
|---|---|
| `power on` | Turn VFD on (disables schedule) |
| `power off` | Turn VFD off (disables schedule) |

### Display Schedule

Automatically turn the VFD on and off at set times. Settings persist across reboots.

| Command | Description |
|---|---|
| `sched on` | Enable the schedule |
| `sched off` | Disable the schedule (turns display back on if it was off) |
| `sched set <ON> <OFF> [days]` | Set schedule times and active days |

**`sched set` format:**

```
sched set HH:MM HH:MM [SMTWTFS]
```

- First time = display ON time (24h format)
- Second time = display OFF time (24h format)
- Days = 7-character string of `1`/`0` for Sun, Mon, Tue, Wed, Thu, Fri, Sat (default: `1111111`)

**Examples:**

```
sched set 8:00 22:00              # 8am-10pm every day
sched set 9:00 18:00 0111110      # 9am-6pm weekdays only
sched set 22:00 6:00              # overnight: 10pm to 6am (wraps past midnight)
```

### LED Control

Controls the WS2812B indicator LED.

```
led <mode> [R G B] [brightness]
```

| Mode | Name | Description |
|---|---|---|
| 0 | `off` | LED off |
| 1 | `solid` | Solid color |
| 2 | `breathing` | Smooth fade in/out |
| 3 | `rainbow` | Cycle through spectrum |
| 4 | `pulse` | Fast pulse effect |
| 5 | `status` | Auto color based on system state |
| 6 | `sync` | Status indicator with sync flashes |

**Examples:**

```
led solid 255 0 0           # solid red
led breathing 0 0 255       # blue breathing
led rainbow                 # rainbow cycle
led solid 255 255 0 64      # yellow at brightness 64
led off                     # turn off
```

### Meeting Reminders

The clock can display meeting reminders with animated transitions. When a meeting is within the configured lead time, the title and time replace the clock display. One minute before the meeting, the VFD brightness flashes between 100% and 50% every second as an alert. Once the meeting time arrives, the clock restores and the meeting is automatically removed from memory.

| Command | Description |
|---|---|
| `meet add <title> <when> HH:MM` | Add a meeting |
| `meet remove <#>` | Remove a meeting by its list number |
| `meet clear` | Remove all meetings |
| `meet lead <minutes>` | Set reminder lead time (default: 5 min) |
| `meet list` | List all stored meetings |
| `meet sync <json>` | Bulk sync meetings via JSON |

**`meet add` scheduling options:**

| `<when>` value | Meaning |
|---|---|
| `daily` | Every day |
| `weekdays` | Monday through Friday |
| `weekends` | Saturday and Sunday |
| `mon,wed,fri` | Specific days (comma-separated) |
| `today` | Today's date (oneshot) |
| `tomorrow` | Tomorrow's date (oneshot) |
| `day-after` | Day after tomorrow (oneshot) |
| `2026-04-17` | Specific date as YYYY-MM-DD (oneshot) |

**Examples:**

```
meet add Standup weekdays 09:30
meet add Team Lunch daily 12:00
meet add Dentist tomorrow 15:30
meet add Sprint Review 2026-04-18 14:00
meet lead 10                            # remind 10 min before
meet list                               # see all meetings
meet remove 2                           # remove #2 from list
```

**Bulk sync via JSON:**

```
meet sync {"lead_minutes":5,"meetings":[{"title":"Standup","time":"09:30","type":"recurring","days":"mon,tue,wed,thu,fri"},{"title":"Dentist","time":"15:30","type":"oneshot","date":"2026-04-20"}]}
```

### Network Info

| Command | Description |
|---|---|
| `wifi` | Show WiFi IP, RSSI, and OTA hostname |

## Architecture

The firmware uses FreeRTOS with two pinned tasks across both ESP32 cores:

```
Core 0: wifiNtpTask          Core 1: updateClockTask
├── WiFi connection           ├── Boot status display
├── NTP time sync             ├── Clock rendering (time + date)
├── ArduinoOTA handler        ├── Display schedule (on/off)
├── BLE command server        ├── Meeting reminder logic
├── LED animation updates     ├── Brightness flash alerts
└── WiFi auto-reconnect       └── Custom message display
```

A FreeRTOS mutex (`displayMutex`) protects the VFD from concurrent writes between the two cores.

### Source Files

| File | Purpose |
|---|---|
| `VFD_BLE_Clock.ino` | Entry point, task setup, main loops for both cores |
| `config.h` | All compile-time configuration (pins, WiFi, timeouts, etc.) |
| `StateManager.h/cpp` | Global shared state (volatile flags, settings, mutex) |
| `DisplayManager.h/cpp` | VFD driver wrapper (print, brightness, power control) |
| `Animation.h/cpp` | Matrix curtain transition animation |
| `TimeManager.h/cpp` | NTP client wrapper, time/date formatting |
| `LEDManager.h/cpp` | WS2812B LED modes and animations |
| `MeetingManager.h/cpp` | Meeting storage, reminder checking, NVS persistence |
| `BLECommandHandler.h/cpp` | BLE Nordic UART Service, command parsing and dispatch |
| `ble_terminal.py` | Python BLE CLI client |
| `setup.py` | Python package installer for the CLI |

## License

Author: Teenarp2026
