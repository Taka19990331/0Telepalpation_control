#!/usr/bin/env python3
"""Download the latest AK80 MIT speed-test SD log as a CSV over USB serial."""
import argparse
import csv
from datetime import datetime
import json
from pathlib import Path
import re
import sys
import time

BAUD = 115200
HEADER = 'timestamp_us,command_rad_s,position_rad,velocity_rad_s,torque_estimate_nm,error_code,feedback_valid'


def parse_download(raw_path: Path, csv_path: Path):
    errors = []
    config = {}
    rows = []
    begun = ended = header_seen = False
    with raw_path.open('r', encoding='ascii', errors='replace') as source:
        for line_number, raw in enumerate(source, 1):
            line = raw.strip()
            if line.startswith('ERROR,'):
                errors.append(line)
            elif line.startswith('SPEED_BEGIN,'):
                begun = True
            elif line.startswith('CONFIG,'):
                fields = line.split(',')
                if len(fields) < 3 or (len(fields) - 1) % 2:
                    errors.append(f'Invalid CONFIG at line {line_number}')
                else:
                    config = dict(zip(fields[1::2], fields[2::2]))
            elif line == HEADER:
                header_seen = True
            elif header_seen and line == 'SPEED_END':
                ended = True
                break
            elif header_seen and line:
                fields = line.split(',')
                if len(fields) != 7 or not re.fullmatch(r'\d+', fields[0]):
                    errors.append(f'Invalid CSV row at line {line_number}')
                    continue
                rows.append(fields)
    if not begun:
        errors.append('Missing SPEED_BEGIN marker')
    if not header_seen or not ended:
        errors.append('Missing CSV header or SPEED_END marker')
    try:
        expected = int(config['rows'])
        if len(rows) != expected:
            errors.append(f'Expected {expected} rows, received {len(rows)}')
    except (KeyError, ValueError):
        errors.append('Missing or invalid row count in CONFIG')
    if rows:
        try:
            timestamps = [int(row[0]) for row in rows]
            if any(b <= a for a, b in zip(timestamps, timestamps[1:])):
                errors.append('Timestamps are not strictly increasing')
            for row in rows:
                for value in row[1:5]:
                    if value != 'nan':
                        float(value)
                int(row[5]) if row[5] != 'nan' else None
                if row[6] not in ('0', '1'):
                    raise ValueError('Invalid feedback_valid value')
        except ValueError as exc:
            errors.append(f'Invalid numeric data: {exc}')
    with csv_path.open('x', newline='', encoding='utf-8') as dest:
        writer = csv.writer(dest)
        writer.writerow(HEADER.split(','))
        writer.writerows(rows)
    return {'valid': not errors, 'rows': len(rows), 'config': config, 'errors': errors}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', help='Teensy serial port, for example COM5')
    parser.add_argument('--list-ports', action='store_true', help='List available serial ports')
    parser.add_argument('--run', action='store_true', help='Start a new motor trial, then download it')
    parser.add_argument('--timeout', type=float, default=60, help='Timeout in seconds (default: 60)')
    parser.add_argument('--output-dir', type=Path, default=Path(__file__).resolve().parent / 'captures')
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error('--timeout must be positive')
    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        print(f'Install dependency with: "{sys.executable}" -m pip install pyserial', file=sys.stderr)
        return 2
    if args.list_ports:
        for port in list_ports.comports():
            print(f'{port.device}: {port.description}')
        return 0
    if not args.port:
        parser.error('--port is required (use --list-ports to find it)')

    args.output_dir.mkdir(parents=True, exist_ok=True)
    stem = 'speed_' + datetime.now().strftime('%Y%m%d_%H%M%S_%f')
    raw_path = args.output_dir / (stem + '_raw.txt')
    csv_path = args.output_dir / (stem + '.csv')
    result_path = args.output_dir / (stem + '_validation.json')
    try:
        with serial.Serial(args.port, BAUD, timeout=0.25, write_timeout=3) as device:
            # Opening Teensy USB serial can reset it. Let setup initialize CAN and SD.
            time.sleep(2)
            device.reset_input_buffer()
            device.write(b'g' if args.run else b'd')
            device.flush()
            action = 'Running trial, then downloading SD log' if args.run else 'Downloading latest SD log'
            print(f'{action} on {args.port}; close Serial Monitor first.')
            deadline = time.monotonic() + args.timeout
            pending = b''
            download_sent = not args.run
            ended = False
            with raw_path.open('xb') as raw_file:
                while time.monotonic() < deadline:
                    block = device.read(max(1, min(device.in_waiting, 65536)))
                    if not block:
                        continue
                    raw_file.write(block)
                    pending += block
                    while b'\n' in pending:
                        line, pending = pending.split(b'\n', 1)
                        line = line.strip()
                        if line.startswith(b'ERROR,'):
                            raise RuntimeError(line.decode('ascii', errors='replace'))
                        if args.run and not download_sent and line.startswith(b'SAVED,'):
                            device.write(b'd')
                            device.flush()
                            download_sent = True
                            print('Trial saved; receiving CSV...')
                        if line == b'SPEED_END':
                            ended = True
                            break
                    if ended:
                        break
                    if len(pending) > 65536:
                        raise RuntimeError('Received over 64 KiB without a newline')
            if not ended:
                raise TimeoutError('Timed out before SPEED_END; inspect the raw capture for details')
        result = parse_download(raw_path, csv_path)
        result_path.write_text(json.dumps(result, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
        print(f'CSV: {csv_path}\nRaw serial: {raw_path}\nValidation: {result_path}')
        if result['valid']:
            print(f"OK: {result['rows']} rows downloaded.")
            return 0
        print('Download incomplete or invalid:', file=sys.stderr)
        for error in result['errors']:
            print(f'  {error}', file=sys.stderr)
        return 1
    except (serial.SerialException, OSError, RuntimeError, TimeoutError, KeyboardInterrupt) as exc:
        print(f'Capture failed: {exc}', file=sys.stderr)
        print('Close Serial Monitor and check the COM port. Raw partial output, if any, is retained.', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
