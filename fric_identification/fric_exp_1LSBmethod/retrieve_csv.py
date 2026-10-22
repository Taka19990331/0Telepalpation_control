#!/usr/bin/env python3
"""Start the AK80 1-LSB run or recover its single microSD CSV over USB serial."""
import argparse
import datetime as dt
import os
from pathlib import Path
import sys
import time
import zlib

try:
    import serial
except ImportError:
    serial = None

ROOT = Path(__file__).resolve().parents[2]
DATA_DIR = ROOT / "data"


def need_serial():
    if serial is None:
        raise RuntimeError("pyserial is required. Install it with: python -m pip install pyserial")


def open_port(port):
    need_serial()
    link = serial.Serial(port, 115200, timeout=0.25, write_timeout=3)
    # Teensy may reset when the USB serial port opens. Wait for its startup line.
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        line = link.readline().decode("ascii", "replace").strip()
        if line == "READY":
            return link
    return link


def command(link, text, timeout=5):
    link.reset_input_buffer()
    link.write((text + "\n").encode("ascii"))
    link.flush()
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = link.readline().decode("ascii", "replace").strip()
        if line:
            return line
    raise TimeoutError(f"No response to {text}")


def start(args):
    link = open_port(args.port)
    try:
        now = dt.datetime.now().astimezone()
        epoch = int(now.timestamp())
        start_text = now.isoformat(timespec="seconds")
        response = command(link, f"START,{epoch},{start_text}", timeout=8)
        if response != "STARTED":
            raise RuntimeError(f"Teensy did not start the run: {response}")
        # STARTED only acknowledges file preparation. Confirm the timer state
        # reached SWEEPING before telling the operator that acquisition began.
        deadline = time.monotonic() + 3
        observed = ""
        while time.monotonic() < deadline:
            time.sleep(0.2)
            observed = command(link, "STATUS", timeout=2)
            fields = observed.split(",")
            if len(fields) >= 3 and fields[0] == "STATUS":
                run_state = fields[1]
                timer_state = int(fields[2])
                if run_state == "RUNNING" and timer_state == 2:
                    break
                if timer_state in (3, 4) or run_state == "IDLE":
                    raise RuntimeError(
                        "Teensy accepted START but stopped before acquisition "
                        f"({observed}). Fetch the pending CSV to inspect its stop reason."
                    )
        else:
            raise TimeoutError(f"Acquisition did not reach SWEEPING; last status: {observed}")
        local_start = dt.datetime.fromtimestamp(epoch).astimezone().strftime("%Y-%m-%d %H:%M:%S %Z")
        print(f"Acquisition confirmed. PC start time: {local_start} (epoch {epoch}). Teensy logs to microSD independently.")
    finally:
        link.close()


def fetch(args):
    link = open_port(args.port)
    tmp_path = None
    try:
        link.write(b"FETCH\n")
        link.flush()
        deadline = time.monotonic() + 10
        header = ""
        while time.monotonic() < deadline:
            candidate = link.readline().decode("ascii", "replace").strip()
            if candidate.startswith("DATA,") or candidate.startswith("ERROR,"):
                header = candidate
                break
        if not header.startswith("DATA,"):
            raise RuntimeError(f"Teensy cannot provide a CSV: {header or 'timeout'}")
        parts = header.split(",")
        if len(parts) != 4:
            raise RuntimeError(f"Invalid transfer header: {header}")
        size, expected_crc, epoch = int(parts[1]), int(parts[2], 16), int(parts[3])
        if size <= 0 or size > 2_000_000_000:
            raise RuntimeError(f"Unexpected transfer size: {size}")
        # USB serial may pause for more than the command-line timeout while
        # Teensy streams a file from SD. Keep waiting for the current block;
        # CRC validation still rejects any truncated or corrupted transfer.
        link.timeout = 5
        payload = bytearray()
        remaining = size
        while remaining:
            block = link.read(min(16384, remaining))
            if not block:
                raise TimeoutError(f"Transfer stopped with {remaining} bytes remaining")
            payload.extend(block)
            remaining -= len(block)
        crc = zlib.crc32(payload) & 0xFFFFFFFF
        if crc != expected_crc:
            raise RuntimeError(f"CRC32 mismatch: expected {expected_crc:08X}, got {crc:08X}")
        trailer_deadline = time.monotonic() + 10
        while time.monotonic() < trailer_deadline:
            line = link.readline().decode("ascii", "replace").strip()
            if line == "TRANSFER_END":
                break
        else:
            raise RuntimeError("Missing transfer completion marker")
        # SdFat/Arduino Print writes CRLF for println(); accept either newline style.
        first_line = payload.splitlines()[0] if payload else b""
        if first_line != b"# experiment=AK80-8_1LSBfric":
            raise RuntimeError("Transferred file does not have the expected CSV header")
        DATA_DIR.mkdir(parents=True, exist_ok=True)
        if epoch > 0:
            stamp = dt.datetime.fromtimestamp(epoch).astimezone().strftime("%Y%m%d_%H%M%S")
            source = "PC start time"
        else:
            stamp = dt.datetime.now().astimezone().strftime("%Y%m%d_%H%M%S")
            source = "retrieval time (start time unavailable)"
        output = DATA_DIR / f"1LSBfric_{stamp}.csv"
        if output.exists():
            output = DATA_DIR / f"1LSBfric_{stamp}_{crc:08X}.csv"
        tmp_path = output.with_suffix(output.suffix + ".partial")
        with open(tmp_path, "xb") as stream:
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(tmp_path, output)
        tmp_path = None
        response = command(link, "ACK", timeout=5)
        if response != "ACK_OK":
            raise RuntimeError(f"CSV was saved locally, but Teensy retained its source: {response}")
        print(f"Saved {output} ({size} bytes, CRC32 {crc:08X}; filename date from {source}).")
        result_line = next((line.decode("ascii", "replace") for line in payload.splitlines()
                            if line.startswith(b"# result=")), None)
        if result_line:
            print(f"Teensy result: {result_line}")
    finally:
        if tmp_path is not None:
            try:
                tmp_path.unlink()
            except OSError:
                pass
        link.close()


def status(args):
    link = open_port(args.port)
    try:
        response = command(link, "STATUS")
        fields = response.split(",")
        if len(fields) < 21 or fields[0] != "STATUS":
            print(response)
            return
        state = fields[3]
        phase = int(fields[4])
        mode = int(fields[5])
        phase_name = {1: "+increase", 2: "+decrease", 3: "-increase", 4: "-decrease"}.get(phase, "idle")
        mode_name = {0: "low_ramp_increase", 1: "steady_step", 2: "low_ramp_decrease"}.get(mode, "unknown")
        reason_names = {
            0: "none", 1: "CAN_overrun", 2: "motor_error", 3: "invalid_feedback",
            4: "user_abort", 5: "SD_error", 6: "start_timeout", 7: "feedback_timeout",
            8: "overspeed", 9: "run_timeout", 10: "control_gap", 11: "stop_timeout",
            12: "CAN_TX_failed", 13: "15_rad_s_limit", 14: "position_jump",
            15: "CAN_error_state", 16: "disable_TX_unconfirmed",
        }
        primary_reason = int(fields[15])
        stop_reason = int(fields[16])
        print(f"RunState: {state} ({fields[2]}), acquisition: {fields[1]}")
        print(f"Phase: {phase} {phase_name}, Mode: {mode} {mode_name}, torque: {fields[6]} counts ({fields[7]} Nm)")
        print(f"Feedback: sequence {fields[8]}, age {fields[9]} us")
        print(f"CAN TX: accepted {fields[10]}, completed {fields[11]}, failed {fields[12]}, in-flight {fields[19]}")
        print(f"Samples: steady window {fields[13]}, stop window {fields[14]}")
        print(f"Reason: primary {primary_reason} ({reason_names.get(primary_reason, 'other')}), latest stop/anomaly {stop_reason} ({reason_names.get(stop_reason, 'other')})")
        print(f"Stop confirmed: {fields[17]}, Disable TX completed: {fields[18]}, pending CSV rows: {fields[20]}")
    finally:
        link.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    for name, handler in (("start", start), ("fetch", fetch), ("status", status)):
        p = sub.add_parser(name)
        p.add_argument("--port", required=True, help="Teensy USB serial port, for example COM5")
        p.set_defaults(handler=handler)
    args = parser.parse_args()
    try:
        args.handler(args)
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
