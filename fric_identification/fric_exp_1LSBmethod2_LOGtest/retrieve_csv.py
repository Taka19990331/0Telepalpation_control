#!/usr/bin/env python3
"""Start the AK80 fixed-step friction sweep or recover its microSD CSV over USB serial."""
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


def open_port(port, reset_on_open=True):
    need_serial()
    link = serial.Serial(port=None, baudrate=115200, timeout=0.25, write_timeout=3)
    if not reset_on_open:
        # Keep DTR low before opening so STATUS/FETCH do not reset an active run.
        link.dtr = False
    link.port = port
    link.open()
    try:
        # The Teensy USB CDC port can outrun the small default Windows queue.
        link.set_buffer_size(rx_size=4 * 1024 * 1024)
    except (AttributeError, OSError, serial.SerialException):
        pass
    if reset_on_open:
        # START uses the normal Teensy reset-on-open flow and waits for setup.
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
    link = open_port(args.port, reset_on_open=False)
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
        if size <= 0:
            raise RuntimeError(f"Unexpected transfer size: {size}")
        link.timeout = 2
        payload = bytearray()
        offset = 0
        while offset < size:
            attempts = 0
            while True:
                while True:
                    candidate = link.readline().decode("ascii", "replace").strip()
                    if candidate == "TRANSFER_END":
                        raise RuntimeError(f"Teensy ended transfer early at {offset} of {size} bytes")
                    if candidate.startswith("ERROR,"):
                        raise RuntimeError(f"Teensy returned {candidate}")
                    if candidate.startswith("BLOCK,"):
                        break
                fields = candidate.split(",")
                if len(fields) != 4:
                    raise RuntimeError(f"Invalid block header: {candidate}")
                block_offset, block_size, block_crc = int(fields[1]), int(fields[2]), int(fields[3], 16)
                if (block_offset != offset or not 0 < block_size <= 4096
                        or offset + block_size > size):
                    raise RuntimeError(f"Unexpected block header: {candidate}")

                chunk = bytearray()
                deadline = time.monotonic() + 30
                while len(chunk) < block_size and time.monotonic() < deadline:
                    chunk.extend(link.read(block_size - len(chunk)))
                if len(chunk) == block_size and (zlib.crc32(chunk) & 0xFFFFFFFF) == block_crc:
                    payload.extend(chunk)
                    link.write(b"+")
                    offset += block_size
                    break
                link.write(b"-")
                attempts += 1
                if attempts >= 25:
                    raise TimeoutError(f"Could not receive valid block at offset {offset} after 25 retries")

        trailer_deadline = time.monotonic() + 30
        while time.monotonic() < trailer_deadline:
            candidate = link.readline().decode("ascii", "replace").strip()
            if candidate == "TRANSFER_END":
                break
            if candidate:
                raise RuntimeError(f"Unexpected transfer trailer: {candidate}")
        else:
            raise TimeoutError("Missing transfer completion marker")
        actual_size = len(payload)
        crc = zlib.crc32(payload) & 0xFFFFFFFF
        if crc != expected_crc:
            raise RuntimeError(
                f"CRC32 mismatch after receiving {actual_size} bytes "
                f"(announced {size}): expected {expected_crc:08X}, got {crc:08X}"
            )
        # SdFat/Arduino Print writes CRLF for println(); accept either newline style.
        first_line = payload.splitlines()[0] if payload else b""
        if first_line != b"# experiment=AK80-8_1LSBfric2_fixed_steps":
            raise RuntimeError("Transferred file does not have the expected CSV header")
        result_line = next((line.decode("ascii", "replace") for line in payload.splitlines()
                            if line.startswith(b"# result=")), None)
        if not result_line:
            raise RuntimeError("CSV has no completion result footer; refusing to ACK an incomplete run")
        DATA_DIR.mkdir(parents=True, exist_ok=True)
        if epoch > 0:
            stamp = dt.datetime.fromtimestamp(epoch).astimezone().strftime("%Y%m%d_%H%M%S")
            source = "PC start time"
        else:
            stamp = dt.datetime.now().astimezone().strftime("%Y%m%d_%H%M%S")
            source = "retrieval time (start time unavailable)"
        output = DATA_DIR / f"1LSBfric2_{stamp}.csv"
        if output.exists():
            output = DATA_DIR / f"1LSBfric2_{stamp}_{crc:08X}.csv"
        duplicate_index = 2
        while output.exists():
            output = DATA_DIR / f"1LSBfric2_{stamp}_{crc:08X}_{duplicate_index}.csv"
            duplicate_index += 1
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
        if actual_size != size:
            print(f"Warning: Teensy announced {size} bytes; received {actual_size}; CRC and footer validated.", file=sys.stderr)
        print(f"Saved {output} ({actual_size} bytes, CRC32 {crc:08X}; filename date from {source}).")
        print(f"Teensy result: {result_line}")
    finally:
        if tmp_path is not None:
            try:
                tmp_path.unlink()
            except OSError:
                pass
        link.close()


def status(args):
    link = open_port(args.port, reset_on_open=False)
    try:
        response = command(link, "STATUS")
        fields = response.split(",")
        if len(fields) < 21 or fields[0] != "STATUS":
            print(response)
            return
        state = fields[3]
        phase = int(fields[4])
        mode = int(fields[5])
        phase_name = {1: "+1 LSB", 2: "+2 LSB", 3: "-2 LSB", 4: "-1 LSB",
                      5: "-1 LSB", 6: "-2 LSB", 7: "+2 LSB", 8: "+1 LSB"}.get(phase, "idle")
        mode_name = {0: "fixed_schedule"}.get(mode, "unknown")
        reason_names = {
            0: "none", 1: "CAN_overrun", 2: "motor_error", 3: "invalid_feedback",
            4: "user_abort", 5: "SD_error", 6: "start_timeout", 7: "feedback_timeout",
            8: "reserved", 9: "run_timeout", 10: "control_gap", 11: "stop_timeout",
            12: "CAN_TX_failed", 13: "reserved", 14: "position_jump",
            15: "CAN_error_state", 16: "disable_TX_unconfirmed",
        }
        primary_reason = int(fields[15])
        stop_reason = int(fields[16])
        print(f"RunState: {state} ({fields[2]}), acquisition: {fields[1]}")
        print(f"Phase: {phase} {phase_name}, Mode: {mode} {mode_name}, torque: {fields[6]} counts ({fields[7]} Nm)")
        print(f"Feedback: sequence {fields[8]}, age {fields[9]} us")
        print(f"CAN TX: accepted {fields[10]}, completed {fields[11]}, failed {fields[12]}, in-flight {fields[19]}")
        print(f"Steps in current leg: {fields[13]}, stop window samples: {fields[14]}")
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
