#!/usr/bin/env python3
"""Collect read-only STATUS, EVENTS, and HISTORY after the v5 test is stopped."""

import argparse
import time
from pathlib import Path

import serial


def collect_quiet(device: serial.Serial, log, section: str, quiet_seconds: float = 1.0) -> list[str]:
    lines: list[str] = []
    last_data = time.monotonic()
    while time.monotonic() - last_data < quiet_seconds:
        raw = device.readline()
        if not raw:
            continue
        line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
        log.write(f"{time.monotonic():.6f},{section},{line}\n")
        log.flush()
        lines.append(line)
        last_data = time.monotonic()
    return lines


def send_read_only(device: serial.Serial, log, command: str) -> list[str]:
    if command not in {"STATUS", "EVENTS", "HISTORY"}:
        raise ValueError(f"not a read-only command: {command}")
    log.write(f"# HOST_COMMAND={command}\n")
    log.flush()
    device.write((command + "\n").encode("ascii"))
    device.flush()
    return collect_quiet(device, log, command)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="confirmed Teensy serial COM port")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if args.output.exists():
        parser.error(f"refusing to overwrite existing capture: {args.output}")
    args.output.parent.mkdir(parents=True, exist_ok=True)

    with args.output.open("x", encoding="utf-8", newline="\n") as log:
        log.write(f"# PORT={args.port}\n# BAUD=115200\n# READ_ONLY_COMMANDS=STATUS,EVENTS,HISTORY\n")
        device = serial.Serial(port=None, baudrate=115200, timeout=0.2, write_timeout=2)
        device.dtr = False
        device.rts = False
        device.port = args.port
        with device:
            status = send_read_only(device, log, "STATUS")
            if not any(line.startswith("STATE,IDLE,") for line in status):
                log.write("# ABORTED=Teensy did not report IDLE; EVENTS/HISTORY not queried\n")
                print("status did not confirm IDLE; no further command sent")
                return 2
            events = send_read_only(device, log, "EVENTS")
            history = send_read_only(device, log, "HISTORY")

    print(f"status_lines={len(status)} event_lines={len(events)} history_lines={len(history)}")
    print(f"output={args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
