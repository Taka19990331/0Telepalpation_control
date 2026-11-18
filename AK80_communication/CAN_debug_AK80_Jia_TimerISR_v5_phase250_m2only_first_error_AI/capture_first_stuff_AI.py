#!/usr/bin/env python3
"""Capture the authorized v5 baseline using only S, X, and STATUS serial commands."""

import argparse
import sys
import time
from pathlib import Path

import serial

ALLOWED_SERIAL_COMMANDS = {"S", "X", "STATUS"}
FAULT_MARKERS = (
    "ERROR BUS_OFF", "ERROR RX_TIMEOUT", "ERROR TX_FAIL", "ERROR MOTOR_ERROR",
    "ERROR ISR_OVERRUN", "ERROR CAN_STUFF", "START_ERROR", "FAULT_STOPPED",
)
COMM_ERROR_MARKERS = (" CAN_ERROR ", "CAN_IRQ_ERROR_SNAPSHOT")
V5_READY_PREFIX = "READY,CAN_debug_AK80_Jia_TimerISR_v5_PHASE250"


def send_command(device: serial.Serial, command: str, log) -> None:
    if command not in ALLOWED_SERIAL_COMMANDS:
        raise ValueError(f"serial command is not allowlisted: {command}")
    log.write(f"# HOST_COMMAND={command}\n")
    log.flush()
    device.write((command + "\n").encode("ascii"))
    device.flush()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="confirmed Teensy serial COM port")
    parser.add_argument("output", type=Path, help="new raw capture log path")
    parser.add_argument("--duration", type=int, choices=(10, 60, 600), default=10,
                        help="observation duration in seconds (default: 10)")
    parser.add_argument("--ready-timeout", type=int, default=30)
    parser.add_argument("--status-timeout", type=float, default=3.0,
                        help="read-only STATUS response window")
    parser.add_argument("--expected-motors", choices=("M1M2", "M1", "M2"), default="M1M2",
                        help="enabled motor set required before S is sent (default: M1M2)")
    parser.add_argument("--expected-period-us", type=int, choices=(500, 1000), default=500,
                        help="command period required by read-only STATUS (default: 500)")
    parser.add_argument("--settle-seconds", type=float, default=2.0)
    args = parser.parse_args()
    if args.ready_timeout < 1 or args.status_timeout <= 0 or args.settle_seconds < 0:
        parser.error("timeouts must be positive (settle time may be zero)")
    if args.output.exists():
        parser.error(f"refusing to overwrite existing capture: {args.output}")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.open("x", encoding="utf-8").close()
    records = 0
    started = None
    firmware_fault_stop = False
    host_stop_sent = False
    start_command_attempted = False
    stop_reason = "duration"

    try:
        # DTR/RTS remain low. Opening USB serial can reset some Teensy setups;
        # v5 stays idle until S, so no motor frame is sent before capture starts.
        device = serial.Serial(port=None, baudrate=115200, timeout=0.2, write_timeout=2)
        device.dtr = False
        device.rts = False
        device.port = args.port
        with device, args.output.open("a", encoding="utf-8", newline="\n") as log:
            log.write(f"# PORT={args.port}\n# BAUD=115200\n# DURATION_SECONDS={args.duration}\n")
            log.write("# Only serial commands S, X, STATUS are allowlisted.\n")
            log.flush()
            ready_deadline = time.monotonic() + args.ready_timeout
            ready = False
            while time.monotonic() < ready_deadline:
                raw = device.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
                log.write(f"{time.monotonic():.6f},READY_WAIT,{line}\n")
                log.flush()
                records += 1
                if line.startswith(V5_READY_PREFIX):
                    ready = True
                    break
            # READY is a one-shot boot message and may have been emitted before
            # the host opened COM3. Always validate IDLE and motor selection via
            # the read-only STATUS command before sending S.
            send_command(device, "STATUS", log)
            status_deadline = time.monotonic() + args.status_timeout
            status_lines = []
            while time.monotonic() < status_deadline:
                raw = device.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
                log.write(f"{time.monotonic():.6f},STATUS_PROBE,{line}\n")
                log.flush()
                records += 1
                status_lines.append(line)
            v5_idle_status_prefix = (
                f"STATE,IDLE,PERIOD_US,{args.expected_period_us},RX_POLL_US,100,"
            )
            idle_v5_status = any(line.startswith(v5_idle_status_prefix) for line in status_lines)
            observed_motors = {
                motor for motor in ("M1", "M2")
                if any(line.startswith(f"{motor} TX:") for line in status_lines)
            }
            expected_motors = {
                "M1M2": {"M1", "M2"}, "M1": {"M1"}, "M2": {"M2"}
            }[args.expected_motors]
            if not idle_v5_status:
                raise TimeoutError("v5 IDLE STATUS signature not observed; S was not sent")
            if observed_motors != expected_motors:
                raise ValueError(
                    f"enabled motors {sorted(observed_motors)} do not match "
                    f"expected {sorted(expected_motors)}; S was not sent"
                )

            start_command_attempted = True
            send_command(device, "S", log)
            started = time.monotonic()
            deadline = started + args.duration
            while time.monotonic() < deadline:
                raw = device.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
                elapsed = time.monotonic() - started
                log.write(f"{elapsed:.6f},{line}\n")
                log.flush()
                records += 1

                if any(marker in line for marker in FAULT_MARKERS):
                    firmware_fault_stop = True
                    stop_reason = "firmware_fault_stop"
                    if "ERROR CAN_STUFF" in line:
                        # Firmware already disabled periodic TX and sent its
                        # zero/exit stop frames. X only normalizes the latched
                        # FAULT_STOPPED state to IDLE for read-only post-checks.
                        send_command(device, "X", log)
                        host_stop_sent = True
                    break
                if any(marker in line for marker in COMM_ERROR_MARKERS):
                    # For the target first-STUFF experiment, firmware is
                    # required to latch/stop inside this diagnostic path.
                    # Wait for its fault report before X to avoid racing it.
                    if "CAN_IRQ_ERROR_SNAPSHOT" in line and "STUFF" in line:
                        continue
                    stop_reason = "communication_error"
                    send_command(device, "X", log)
                    host_stop_sent = True
                    break

            if not firmware_fault_stop and not host_stop_sent:
                send_command(device, "X", log)
                host_stop_sent = True
            settle_deadline = time.monotonic() + args.settle_seconds
            while time.monotonic() < settle_deadline:
                raw = device.readline()
                if raw:
                    line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
                    log.write(f"{time.monotonic() - started:.6f},{line}\n")
                    log.flush()
                    records += 1
            send_command(device, "STATUS", log)
            status_deadline = time.monotonic() + 2.0
            while time.monotonic() < status_deadline:
                raw = device.readline()
                if raw:
                    line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
                    log.write(f"{time.monotonic() - started:.6f},{line}\n")
                    log.flush()
                    records += 1
    except (OSError, serial.SerialException, TimeoutError, ValueError) as exc:
        # Keep the reason beside the partial raw capture so a timeout or port
        # error cannot be mistaken for a valid measurement.
        try:
            with args.output.open("a", encoding="utf-8", newline="\n") as log:
                log.write(f"# CAPTURE_FAILED={type(exc).__name__}: {exc}\n")
                log.write(f"# S_COMMAND_ATTEMPTED={int(start_command_attempted)}\n")
        except OSError:
            pass
        print(f"capture failed: {exc}", file=sys.stderr)
        print("No automatic retry was performed.", file=sys.stderr)
        return 2

    elapsed = 0.0 if started is None else time.monotonic() - started
    print(f"records={records} elapsed_seconds={elapsed:.3f} stop_reason={stop_reason}")
    print(f"firmware_fault_stop={int(firmware_fault_stop)} host_stop_sent={int(host_stop_sent)}")
    print(f"output={args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
