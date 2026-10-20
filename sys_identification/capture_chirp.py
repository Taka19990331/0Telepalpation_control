"""Capture chirp_timer_test USB Serial output without a monitor line limit."""
import argparse
import csv
from datetime import datetime
import json
import math
from pathlib import Path
import sys
import time


HEADER = ['index', 'nominal_us', 'actual_us', 'period_us', 'torque_Nm', 'ISR_us']


def extract(raw_path, csv_path):
    """Keep all valid rows, but report any incomplete/corrupt capture as failure."""
    with raw_path.open('rb') as probe:
        if any(line.startswith((b'CHIRP_BEGIN,VERSION,SD3,',b'CHIRP_BEGIN,VERSION,SD4,',b'CHIRP_BEGIN,VERSION,SD5,')) for line in probe):
            from chirp_sd3 import extract as extract_sd3
            return extract_sd3(raw_path, csv_path)
    with raw_path.open('rb') as probe:
        if any(line.startswith(b'CHIRP_BEGIN,VERSION,SD6,') for line in probe):
            from chirp_sd6 import extract as extract_sd6
            return extract_sd6(raw_path,csv_path)
    with raw_path.open('rb') as probe:
        if any(line.startswith(b'CHIRP_BEGIN,VERSION,SI1,') for line in probe):
            from chirp_si1 import extract as extract_si1
            return extract_si1(raw_path,csv_path)
    errors = []
    begun = ended = header_seen = False
    reason = None
    config = None
    period_us = expected_rows = None
    count = 0
    summary_count = None
    previous_actual = None
    last_torque = None
    with raw_path.open('rb') as source, csv_path.open('x', newline='', encoding='utf-8') as dest:
        writer = csv.writer(dest)
        writer.writerow(HEADER)
        for line_number, raw in enumerate(source, 1):
            try:
                line = raw.decode('ascii').strip()
                fields = line.split(',')
                if line.startswith('CHIRP_BEGIN,'):
                    if begun:
                        errors.append('Multiple CHIRP_BEGIN markers')
                    begun = True
                    reason = fields[fields.index('REASON') + 1]
                elif line == 'CHIRP_END':
                    ended = True
                    break
                elif line.startswith('CONFIG,'):
                    config = {fields[i]: float(fields[i + 1]) for i in range(1, len(fields), 2)}
                    hz, duration = config['Hz'], config['Duration_s']
                    if not math.isfinite(hz) or not math.isfinite(duration) or hz <= 0 or duration <= 0:
                        raise ValueError('Invalid sampling frequency/duration')
                    period_us = round(1e6 / hz)
                    steps = round(duration * hz)
                    if (period_us <= 0 or steps < 1 or
                            not math.isclose(period_us, 1e6 / hz, rel_tol=0, abs_tol=1e-6) or
                            not math.isclose(steps, duration * hz, rel_tol=0, abs_tol=1e-6)):
                        raise ValueError('Noninteger timer period or step count')
                    expected_rows = steps + 1
                    if (config.get('Period_us', period_us) != period_us or
                            config.get('ExpectedRows', expected_rows) != expected_rows):
                        raise ValueError('Inconsistent CONFIG')
                    config.setdefault('AmplitudeExponent', 1.0)
                elif line.startswith('DATA,'):
                    if not begun:
                        raise ValueError('DATA before CHIRP_BEGIN')
                    if fields[1:] == HEADER:
                        header_seen = True
                        continue
                    if len(fields) != 7:
                        raise ValueError('Incorrect DATA column count')
                    index, nominal, actual, period = map(int, fields[1:5])
                    torque = float(fields[5])
                    execution = int(fields[6])
                    if index != count or period_us is None or nominal != index * period_us:
                        errors.append(f'Unexpected index/time at row {count}: {index}, {nominal}')
                    if not math.isfinite(torque) or min(index, nominal, actual, period, execution) < 0:
                        raise ValueError('Invalid numeric value')
                    expected_period = 0 if previous_actual is None else actual - previous_actual
                    if period != expected_period:
                        errors.append(f'Inconsistent period at index {index}')
                    if count == 0 and actual != 0:
                        errors.append('First actual_us is not zero')
                    writer.writerow(fields[1:])
                    previous_actual, last_torque = actual, torque
                    count += 1
                elif line.startswith('SUMMARY,'):
                    summary_count = int(fields[fields.index('Count') + 1])
                elif line.startswith('ERROR,'):
                    errors.append(line)
            except (ValueError, IndexError, KeyError, OverflowError, UnicodeError) as exc:
                errors.append(f'Raw line {line_number}: {exc}')
    if not begun or not ended or not header_seen:
        errors.append('Missing begin/end marker or DATA header')
    if reason != 'COMPLETE':
        errors.append(f'Run did not complete normally: {reason}')
    if expected_rows is None or count != expected_rows:
        errors.append(f'Expected {expected_rows} data rows, received {count}')
    if config is not None:
        csv_path.with_suffix('.config.json').write_text(json.dumps(config, indent=2) + '\n', encoding='utf-8')
    if summary_count != count:
        errors.append(f'SUMMARY count {summary_count} differs from data count {count}')
    if last_torque != 0.0:
        errors.append('Final torque is not zero')
    return {'valid': not errors, 'rows': count, 'reason': reason, 'errors': errors, 'config': config}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', help='Serial port, e.g. COM5')
    parser.add_argument('--list-ports', action='store_true')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--sd', action='store_true', help='Start SD recording, wait for SAVED, then download to CSV')
    mode.add_argument('--download', action='store_true', help='Download the saved SD log without starting a new test')
    parser.add_argument('--output-dir', type=Path, default=Path(__file__).resolve().parent / 'captures')
    parser.add_argument('--timeout', type=float, default=900, help='Total capture timeout in seconds (default: 900)')
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error('--timeout must be positive')
    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        print(f'Install dependency with this Python: "{sys.executable}" -m pip install pyserial', file=sys.stderr)
        return 2
    if args.list_ports:
        for port in list_ports.comports():
            print(f'{port.device}: {port.description}')
        return 0
    if not args.port:
        parser.error('--port is required (use --list-ports to find it)')
    args.output_dir.mkdir(parents=True, exist_ok=True)
    stem = 'chirp_' + datetime.now().strftime('%Y%m%d_%H%M%S_%f')
    raw_path = args.output_dir / (stem + '_raw.txt')
    csv_path = args.output_dir / (stem + '.csv')
    result_path = args.output_dir / (stem + '_validation.json')
    capture_error = None
    ended = False
    try:
        with serial.Serial(args.port, 115200, timeout=0.25, write_timeout=2) as device, raw_path.open('xb') as raw_file:
            # Allow USB connection/reset to settle, then discard old boot/idle output.
            time.sleep(2)
            device.reset_input_buffer()
            device.write(b'd' if args.download else b's')
            device.flush()
            action = ('Downloading saved SD log' if args.download else
                      'Recording to SD, then downloading' if args.sd else
                      'Measuring for 20 s, then receiving the RAM log')
            print(f'{action} on {args.port}...')
            print(f'Raw log: {raw_path}')
            deadline = time.monotonic() + args.timeout
            pending = b''
            download_requested = args.download
            while time.monotonic() < deadline:
                block = device.read(max(1, min(device.in_waiting, 65536)))
                if not block:
                    continue
                raw_file.write(block)
                pending += block
                while b'\n' in pending:
                    line, pending = pending.split(b'\n', 1)
                    if line.startswith(b'ERROR,'):
                        raise ValueError(line.decode('ascii', errors='replace').strip())
                    if args.sd and not download_requested and line.startswith(b'SAVED,'):
                        values = line.decode('ascii').strip().split(',')
                        if values[values.index('Status') + 1] != '0':
                            print('Run aborted; downloading its diagnostic log.')
                        device.write(b'd')
                        device.flush()
                        download_requested = True
                        print('SD save complete. Receiving CSV data...')
                    if line.strip() == b'CHIRP_END':
                        ended = True
                        break
                if ended:
                    break
                if len(pending) > 65536:
                    raise ValueError('Received over 64 KiB without a newline')
            if not ended:
                capture_error = 'Timeout before CHIRP_END'
    except (serial.SerialException, OSError, ValueError, KeyboardInterrupt) as exc:
        capture_error = str(exc) or type(exc).__name__
    if not raw_path.exists():
        print(f'Capture failed: {capture_error}. Close the serial monitor and check --port.', file=sys.stderr)
        return 1
    result = extract(raw_path, csv_path)
    if capture_error:
        result['errors'].append(capture_error)
        result['valid'] = False
    result_path.write_text(json.dumps(result, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    print(f'CSV: {csv_path}\nValidation: {result_path}')
    if result['valid'] and result.get('complete', True):
        print(f"PASS: {result['rows']} rows, sequential indices, complete markers, final zero torque.")
        return 0
    if result['valid']:
        print(f"ABORTED: {result.get('status_name', result['reason'])}; diagnostic CSV retained.")
        return 2
    print('FAIL: capture is incomplete or invalid; partial files have been retained.', file=sys.stderr)
    for error in result['errors'][:20]:
        print(f'  {error}', file=sys.stderr)
    return 1


if __name__ == '__main__':
    sys.exit(main())
