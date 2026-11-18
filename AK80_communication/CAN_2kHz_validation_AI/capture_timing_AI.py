#!/usr/bin/env python3
"""Read-only timed capture of TIMER_ONLY firmware serial records.

This script never writes to the serial port. Opening a USB serial port can
reset some boards; use only after the approved physical isolation procedure.
"""

import argparse
import re
import sys
import time
from pathlib import Path

import serial

RECORD = re.compile(rb"(?:^|,)TIMER_BASE_US=\d+(?:,|$)")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="confirmed Teensy serial port")
    parser.add_argument("output", type=Path, help="destination text log")
    parser.add_argument("--duration", type=int, default=60,
                        help="capture duration in seconds (60 to 600; default: 60)")
    parser.add_argument("--startup-timeout", type=int, default=30,
                        help="seconds to wait for the first valid record")
    args = parser.parse_args()
    if not 60 <= args.duration <= 600:
        parser.error("--duration must be between 60 and 600 seconds")
    if args.startup_timeout < 1:
        parser.error("--startup-timeout must be positive")
    if args.output.exists():
        parser.error(f"refusing to overwrite existing log: {args.output}")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    deadline = time.monotonic() + args.startup_timeout
    start = None
    previous_elapsed = -1
    previous_values = None
    records = 0
    counters = ("TICK_COUNT", "VIRTUAL_EVENTS", "VIRTUAL_JITTER_VIOLATIONS",
                "MISSED_VIRTUAL_EST", "ISR_OVERRUNS", "MISSED_DEADLINES",
                "CAN_TX_COUNT")

    try:
        # Keep modem-control lines low to reduce the chance of resetting a board.
        device = serial.Serial(port=None, baudrate=115200, timeout=1)
        device.dtr = False
        device.rts = False
        device.port = args.port
        with device:
            with args.output.open("x", encoding="utf-8", newline="\n") as log:
                log.write(f"# port={args.port}\n# baud=115200\n# duration_seconds={args.duration}\n")
                log.write("# Host elapsed time is measured when each complete serial line is received.\n")
                log.flush()
                while True:
                    raw = device.readline()
                    received_at = time.monotonic()
                    if not raw:
                        if start is None and received_at >= deadline:
                            raise TimeoutError("no valid timing record before startup timeout")
                        if start is not None and received_at - start >= args.duration:
                            break
                        continue
                    raw = raw.strip()
                    if not RECORD.search(raw):
                        continue
                    try:
                        line = raw.decode("ascii")
                        values = dict(field.split("=", 1) for field in line.split(","))
                        current = {key: int(values[key], 0) for key in counters}
                        base = int(values["TIMER_BASE_US"], 0)
                    except (UnicodeDecodeError, KeyError, ValueError) as exc:
                        print(f"skipping malformed serial record: {exc}", file=sys.stderr)
                        continue
                    if base not in (50, 100):
                        print(f"skipping unexpected TIMER_BASE_US={base}", file=sys.stderr)
                        continue
                    if start is None:
                        start = received_at
                        elapsed_ms = 0
                    else:
                        elapsed_ms = round((received_at - start) * 1000)
                    if elapsed_ms < previous_elapsed:
                        raise RuntimeError("host elapsed time moved backwards")
                    if previous_values is not None:
                        for key in counters:
                            if current[key] < previous_values[key]:
                                raise RuntimeError(f"cumulative counter {key} decreased")
                    previous_elapsed = elapsed_ms
                    previous_values = current
                    log.write(f"{line},HOST_ELAPSED_MS={elapsed_ms}\n")
                    log.flush()
                    records += 1
                    if elapsed_ms >= args.duration * 1000:
                        break
    except (OSError, serial.SerialException, TimeoutError, RuntimeError) as exc:
        print(f"capture failed: {exc}", file=sys.stderr)
        return 2

    if records < 2:
        print("capture failed: fewer than two valid timing records", file=sys.stderr)
        return 2
    print(f"records={records} output={args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
