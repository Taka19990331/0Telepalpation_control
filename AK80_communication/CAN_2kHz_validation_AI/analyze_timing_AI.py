#!/usr/bin/env python3
"""Summarize CSV serial records emitted by CAN_2kHz_validation_AI.ino."""

import argparse
import sys
from pathlib import Path

REQUIRED = {
    "TIMER_BASE_US", "TICK_COUNT", "TICK_INTERVAL_US", "VIRTUAL_EVENTS",
    "VIRTUAL_INTERVAL_US", "VIRTUAL_JITTER_US", "VIRTUAL_JITTER_VIOLATIONS",
    "MISSED_VIRTUAL_EST", "JITTER_US", "ISR_DURATION_US",
    "ISR_OVERRUNS", "MISSED_DEADLINES", "RX_POLL_DURATION_US",
    "RX_RAW_CAN1", "RX_RAW_CAN2", "CAN_TX_COUNT",
}


def read_rows(path: Path):
    with path.open("r", newline="", encoding="utf-8-sig") as f:
        for line_no, line in enumerate(f, 1):
            if not line.strip() or line.lstrip().startswith("#") or "=" not in line:
                continue
            row = {}
            for field in line.strip().split(","):
                key, sep, value = field.partition("=")
                if sep:
                    row[key.strip()] = value.strip()
            if not row:
                continue
            missing = REQUIRED - row.keys()
            if missing:
                raise ValueError(f"line {line_no}: missing fields: {', '.join(sorted(missing))}")
            parsed = {}
            for key, value in row.items():
                try:
                    parsed[key] = int(value, 0)
                except ValueError as exc:
                    raise ValueError(f"line {line_no}: {key} is not an integer: {value!r}") from exc
            yield parsed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path, help="serial log with one key=value record per line")
    args = parser.parse_args()
    try:
        rows = list(read_rows(args.log))
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    if not rows:
        print("error: no timing records found", file=sys.stderr)
        return 2
    print(f"records={len(rows)}")
    for key in sorted(REQUIRED):
        values = [row[key] for row in rows]
        print(f"{key}: first={values[0]} last={values[-1]} max={max(values)}")
    checks = {}
    checks["CAN_TX_ZERO"] = all(row["CAN_TX_COUNT"] == 0 for row in rows)
    base = rows[-1]["TIMER_BASE_US"]
    if base not in (50, 100) or any(row["TIMER_BASE_US"] != base for row in rows):
        checks["TIMER_BASE_VALID"] = False
        isr_limit = 0
    else:
        checks["TIMER_BASE_VALID"] = True
        isr_limit = base // 2
    checks["ISR_DURATION"] = checks["TIMER_BASE_VALID"] and max(
        row["ISR_DURATION_US"] for row in rows
    ) <= isr_limit
    checks["VIRTUAL_JITTER"] = max(row["VIRTUAL_JITTER_US"] for row in rows) <= 50
    checks["VIRTUAL_JITTER_VIOLATIONS"] = rows[-1]["VIRTUAL_JITTER_VIOLATIONS"] == 0
    checks["MISSED_VIRTUAL"] = rows[-1]["MISSED_VIRTUAL_EST"] == 0
    checks["ISR_OVERRUNS"] = rows[-1]["ISR_OVERRUNS"] == 0
    checks["MISSED_DEADLINES"] = rows[-1]["MISSED_DEADLINES"] == 0

    if all("HOST_ELAPSED_MS" in row for row in rows) and len(rows) >= 2:
        elapsed_ms = rows[-1]["HOST_ELAPSED_MS"] - rows[0]["HOST_ELAPSED_MS"]
        event_delta = rows[-1]["VIRTUAL_EVENTS"] - rows[0]["VIRTUAL_EVENTS"]
        event_hz = event_delta * 1000 / elapsed_ms if elapsed_ms > 0 else 0.0
        checks["VIRTUAL_RATE_2KHZ"] = 1990 <= event_hz <= 2010
        checks["CAPTURE_DURATION"] = elapsed_ms >= 60_000
        print(f"HOST_ELAPSED_MS={elapsed_ms}")
        print(f"VIRTUAL_RATE_HZ={event_hz:.3f}")
    else:
        print("VIRTUAL_RATE_2KHZ=UNAVAILABLE (HOST_ELAPSED_MS needed)")
        checks["VIRTUAL_RATE_2KHZ"] = False
        checks["CAPTURE_DURATION"] = False

    for name, passed in checks.items():
        print(f"{name}={'PASS' if passed else 'FAIL'}")
    print(f"OVERALL={'PASS' if all(checks.values()) else 'FAIL'}")
    return 0 if all(checks.values()) else 1


if __name__ == "__main__":
    raise SystemExit(main())
