"""Download one MC1 SD log from a Teensy running chirp_multi-cycle."""
import argparse
import csv
from datetime import datetime
import json
from pathlib import Path
import sys
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True, help='Teensy serial port, e.g. COM5')
    parser.add_argument('--which', choices=('latest', 'previous'), default='latest')
    parser.add_argument('--output-dir', type=Path, default=Path(__file__).parent / 'captures')
    parser.add_argument('--timeout', type=float, default=7200,
                        help='seconds to allow for a large 115200-baud transfer (default: 7200)')
    args = parser.parse_args()
    try:
        import serial
    except ImportError:
        print(f'Install pyserial with: "{sys.executable}" -m pip install pyserial', file=sys.stderr)
        return 1

    args.output_dir.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    stem = f'MC1_{args.which}_{stamp}'
    raw_path = args.output_dir / f'{stem}.txt'
    csv_path = args.output_dir / f'{stem}.csv'
    validation_path = args.output_dir / f'{stem}_validation.json'
    command = b'd' if args.which == 'latest' else b'p'
    errors = []
    config = None
    columns = None
    count = 0
    summary = None
    source_name = None
    begun = ended = False
    previous_index = previous_actual = None

    try:
        with serial.Serial(args.port, 115200, timeout=0.5, write_timeout=2) as device, \
                raw_path.open('xb') as raw_file, csv_path.open('x', newline='', encoding='utf-8') as csv_file:
            writer = None
            time.sleep(2)
            device.reset_input_buffer()
            device.write(command)
            device.flush()
            print(f'Downloading {args.which} MC1 log from {args.port}; raw log: {raw_path}')
            deadline = time.monotonic() + args.timeout
            pending = b''
            while time.monotonic() < deadline:
                block = device.read(max(1, min(device.in_waiting, 65536)))
                if not block:
                    continue
                raw_file.write(block)
                pending += block
                while b'\n' in pending:
                    raw, pending = pending.split(b'\n', 1)
                    line = raw.decode('ascii', errors='replace').strip()
                    fields = line.split(',')
                    try:
                        if line.startswith('SOURCE_FILE,'):
                            source_name = fields[1]
                        elif line.startswith('CHIRP_BEGIN,'):
                            if begun or fields[:4] != ['CHIRP_BEGIN', 'VERSION', 'MC1', 'REASON']:
                                raise ValueError('Unexpected or repeated CHIRP_BEGIN')
                            begun = True
                        elif line.startswith('CONFIG,'):
                            config = {fields[i]: float(fields[i+1]) for i in range(1, len(fields), 2)}
                            if int(config.get('CycleCount', 0)) != 15 or config.get('PostHoldRecorded') != 0:
                                raise ValueError('Unexpected MC1 cycle/recording configuration')
                        elif line.startswith('DATA,'):
                            values = fields[1:]
                            if values and values[0] == 'index':
                                columns = values
                                writer = csv.writer(csv_file)
                                writer.writerow(columns)
                            else:
                                if not begun or columns is None or writer is None or config is None:
                                    raise ValueError('Data arrived before MC1 metadata')
                                if len(values) != len(columns):
                                    raise ValueError('Incorrect MC1 data column count')
                                row = [int(v) for v in values[:5]] + [float(v) for v in values[5:11]] + [int(v) for v in values[11:]]
                                idx, nominal, actual, period, phase = row[:5]
                                period_us = int(config['Period_us'])
                                if nominal != idx * period_us or phase not in (0, 1, 3):
                                    raise ValueError(f'Invalid index/time/phase at row {count}')
                                if previous_index is not None and idx <= previous_index:
                                    raise ValueError(f'Non-increasing index at row {count}')
                                if previous_actual is None and actual != 0:
                                    raise ValueError('First actual_us must be zero')
                                if previous_actual is not None and (actual < previous_actual or period != actual-previous_actual):
                                    raise ValueError(f'Invalid actual timestamp at row {count}')
                                writer.writerow(values)
                                previous_index, previous_actual = idx, actual
                                count += 1
                        elif line.startswith('SUMMARY,'):
                            summary = int(fields[2])
                        elif line == 'CHIRP_END':
                            ended = True
                            break
                        elif line.startswith('ERROR,'):
                            raise ValueError(line)
                    except (ValueError, IndexError, KeyError) as exc:
                        errors.append(str(exc))
                if ended:
                    break
            if not ended:
                errors.append('Timeout before CHIRP_END')
    except (serial.SerialException, OSError, ValueError, KeyboardInterrupt) as exc:
        errors.append(str(exc) or type(exc).__name__)

    if not begun or not ended or config is None or columns is None:
        errors.append('Incomplete MC1 transfer')
    if summary != count:
        errors.append(f'SUMMARY count {summary} differs from received rows {count}')
    result = {'valid': not errors, 'source_file': source_name, 'rows': count,
              'summary_rows': summary, 'config': config, 'errors': errors}
    validation_path.write_text(json.dumps(result, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    print(f'CSV: {csv_path}\nRaw: {raw_path}\nValidation: {validation_path}')
    if errors:
        print('\n'.join(errors), file=sys.stderr)
        return 1
    print(f'PASS: downloaded {source_name}, {count} rows')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
