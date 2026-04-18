#ifndef CONFIG_H
#define CONFIG_H

// ── Pin Definitions ──────────────────────────────────────────────
#define SI 7
#define STB 8
#define SCK 9
#define GPIO_PIN_10 10
#define LED_PIN 21               // WS2812B LED pin

// ── VFD Hardware Commands (PT6314) ───────────────────────────────
#define VFD_COMMAND_MODE 0xF8
#define VFD_FUNCTIONSET 0x20
#define VFD_8BITMODE 0x10
#define VFD_2LINE 0x08
#define VFD_BRT_MASK 0xFC
#define VFD_BRT_100_VAL 0x00
#define VFD_BRT_75_VAL 0x01
#define VFD_BRT_50_VAL 0x02
#define VFD_BRT_25_VAL 0x03

// ── Display Configuration ────────────────────────────────────────
#define DISPLAY_COLS 20
#define DISPLAY_ROWS 2

// ── WiFi (background: NTP + OTA only) ────────────────────────────
#define WIFI_SSID "Hotspot"
#define WIFI_PASSWORD "elitehabitat@402"
#define WIFI_CONNECT_TIMEOUT 15000

// ── NTP Configuration ────────────────────────────────────────────
#define NTP_SERVER "pool.ntp.org"
#define NTP_UPDATE_INTERVAL 3600000

// ── OTA Configuration ────────────────────────────────────────────
#define OTA_HOSTNAME "vfd-ble-clock"
#define OTA_PASSWORD "vfdota"

// ── BLE Configuration ────────────────────────────────────────────
#define BT_DEVICE_NAME "VFD-BLE-Clock"

// ── Default Settings ─────────────────────────────────────────────
#define DEFAULT_BRIGHTNESS 10
#define DEFAULT_TIMEZONE 19800       // IST (+5:30) in seconds
#define DEFAULT_24HOUR false
#define DEFAULT_COLON_BLINK false

// ── Timeouts ─────────────────────────────────────────────────────
#define NTP_SYNC_TIMEOUT 10000
#define MUTEX_TIMEOUT 200

// ── Task Configuration ───────────────────────────────────────────
#define WIFI_TASK_STACK_SIZE 8192
#define DISPLAY_TASK_STACK_SIZE 8192
#define DISPLAY_UPDATE_RATE 20       // ms

// ── Animation Configuration ──────────────────────────────────────
#define DEFAULT_CURTAIN_WIDTH 5
#define DEFAULT_ANIMATION_SPEED 25
#define MATRIX_CHARS "!@#$%^&*()_+-=[]{}|;:,.<>?/~`"

// ── LED Configuration ────────────────────────────────────────────
#define LED_DEFAULT_BRIGHTNESS 128
#define LED_UPDATE_RATE 20

// ── Serial Configuration ─────────────────────────────────────────
#define SERIAL_BAUD 115200

#endif // CONFIG_H
