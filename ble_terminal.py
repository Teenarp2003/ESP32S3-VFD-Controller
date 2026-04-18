#!/usr/bin/env python3
"""
vfd - BLE CLI for VFD-BLE-Clock

Interactive terminal with command history, arrow keys, and tab completion.
Connects to the ESP32 over BLE (Nordic UART Service).

Install:
    pip install bleak prompt_toolkit

Usage:
    vfd              # scan and connect
    vfd --name X     # connect to device named X
"""

import argparse
import asyncio
import os
import sys
import threading

from bleak import BleakClient, BleakScanner
from prompt_toolkit import PromptSession
from prompt_toolkit.history import FileHistory
from prompt_toolkit.completion import WordCompleter
from prompt_toolkit.patch_stdout import patch_stdout

DEFAULT_DEVICE = "VFD-BLE-Clock"

NUS_RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
NUS_TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

COMMANDS = [
    "ping", "status", "help", "reboot", "restore", "sync", "wifi",
    "msg", "line1", "line2", "clear",
    "bright", "24h on", "24h off", "blink on", "blink off", "tz",
    "led off", "led solid", "led breathing", "led rainbow", "led pulse",
    "led status", "led sync",
    "power on", "power off",
    "sched on", "sched off", "sched set",
    "meet add", "meet remove", "meet clear", "meet lead", "meet list",
    "meet sync",
    "today", "tomorrow", "day-after",
    "weekdays", "weekends", "daily",
]


def parse_args():
    p = argparse.ArgumentParser(prog="vfd", description="BLE CLI for VFD clock")
    p.add_argument("--name", "-n", default=DEFAULT_DEVICE,
                   help=f"BLE device name (default: {DEFAULT_DEVICE})")
    p.add_argument("--timeout", "-t", type=float, default=10.0,
                   help="Scan timeout in seconds (default: 10)")
    return p.parse_args()


HELP_TEXT = """\
=== VFD BLE Clock ===
ping             - pong
status           - show state
msg <text>       - custom msg
line1/2 <text>   - set line
restore          - back to clock
bright <0-100>   - brightness
24h on/off       - 12/24h mode
blink on/off     - colon blink
tz <secs>        - timezone
led <mode> [rgb] - LED ctrl
sched on/off     - schedule
sched set H:M H:M days
meet add <t> <when> H:M
meet remove <#>  - delete
meet clear       - wipe all
meet lead <min>  - reminder
meet list        - list all
meet sync <json> - bulk sync
power on/off     - VFD power
sync             - NTP resync
wifi             - WiFi info
reboot           - restart
help             - this list"""


def print_local_help():
    print(HELP_TEXT)


POLL_INTERVAL = 10


async def connection_watchdog(client, on_notify):
    while True:
        await asyncio.sleep(POLL_INTERVAL)
        if not client.is_connected:
            print("\nConnection lost. Attempting reconnect...")
            try:
                await client.connect()
                await client.start_notify(NUS_TX, on_notify)
                print("Reconnected.\n")
            except Exception:
                print("Reconnect failed. Exiting...")
                os._exit(1)


async def run(args):
    print(f"Scanning for '{args.name}'...")
    device = await BleakScanner.find_device_by_name(args.name, timeout=args.timeout)

    if device is None:
        print(f"Could not find '{args.name}'. Is the ESP32 powered on?")
        sys.exit(1)

    print(f"Found {device.name} ({device.address})")
    print("Connecting...\n")

    async with BleakClient(device) as client:
        def on_notify(_sender, data: bytearray):
            text = data.decode("utf-8", errors="replace")
            sys.stdout.write(text)
            sys.stdout.flush()

        await client.start_notify(NUS_TX, on_notify)

        history_path = os.path.expanduser("~/.vfd_history")

        session: PromptSession = PromptSession(
            history=FileHistory(history_path),
            completer=WordCompleter(COMMANDS, ignore_case=True, sentence=True),
        )

        watchdog = asyncio.create_task(connection_watchdog(client, on_notify))

        print(f"Connected to {device.name}. Type 'help' for commands. Ctrl+D to quit.\n")

        with patch_stdout():
            while True:
                try:
                    line = await asyncio.get_event_loop().run_in_executor(
                        None, lambda: session.prompt("vfd> ")
                    )
                except (EOFError, KeyboardInterrupt):
                    print("\nDisconnecting...")
                    break

                line = line.strip()
                if not line:
                    continue

                if line.lower() in ("exit", "quit"):
                    print("Disconnecting...")
                    break

                if line.lower() == "help":
                    print_local_help()
                    continue

                payload = (line + "\n").encode("utf-8")
                for i in range(0, len(payload), 20):
                    await client.write_gatt_char(NUS_RX, payload[i:i + 20])

                # Small delay to let response arrive before next prompt
                await asyncio.sleep(0.3)

        watchdog.cancel()


def main():
    args = parse_args()
    try:
        asyncio.run(run(args))
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
