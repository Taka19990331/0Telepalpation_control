"""Convert CHxxxx.BIN or SIxxxx.BIN from the chirp sketches to a CSV usable by plot_chirp.py."""
import argparse
import csv
import math
import json
from pathlib import Path
import struct
import sys

HEADER = struct.Struct('<8s4I4f')
HEADER_V2 = struct.Struct('<8s4I5f')
ROW = struct.Struct('<IIfI')
FOOTER = struct.Struct('<8sII')


def convert(source):
    with source.open('rb') as probe:
        if probe.read(8) in (b'CHIRPSD3', b'CHIRPSD4', b'CHIRPSD5'):
            from chirp_sd3 import convert as convert_sd3
            return convert_sd3(source)
    with source.open('rb') as probe:
        if probe.read(8) == b'CHIRPSD6':
            from chirp_sd6 import convert as convert_sd6
            return convert_sd6(source)
    with source.open('rb') as probe:
        if probe.read(8) == b'CHIRPSI1':
            from chirp_si1 import convert as convert_si1
            return convert_si1(source)
    destination = source.with_suffix('.csv')
    with source.open('rb') as file:
        magic = file.read(8)
        layout = HEADER_V2 if magic == b'CHIRPSD2' else HEADER
        file.seek(0)
        header = file.read(layout.size)
        if len(header) != layout.size:
            raise ValueError('Incomplete header')
        values = layout.unpack(header)
        magic, period, duration, expected, row_bytes, f0, f1, a0, a1 = values[:9]
        exponent = values[9] if layout == HEADER_V2 else 1.0
        if (magic not in (b'CHIRPSD1', b'CHIRPSD2') or row_bytes != ROW.size or
                period <= 0 or expected < 2 or duration != period * (expected - 1) or
                not all(math.isfinite(v) for v in (f0, f1, a0, a1, exponent)) or
                not 0 < f0 < f1 < 500000 / period or min(a0, a1) < 0 or exponent <= 0):
            raise ValueError('Unsupported format or invalid chirp configuration')
        config = dict(Hz=1000000 / period, Duration_s=duration / 1000000,
                      F0_Hz=f0, F1_Hz=f1, A0_Nm=a0, A1_Nm=a1,
                      AmplitudeExponent=exponent, Period_us=period, ExpectedRows=expected)
        size = source.stat().st_size
        if size < layout.size + FOOTER.size:
            raise ValueError('Incomplete file')
        file.seek(-FOOTER.size, 2)
        end, rows, status = FOOTER.unpack(file.read(FOOTER.size))
        if end != b'ENDSD001' or size != layout.size + rows * ROW.size + FOOTER.size:
            raise ValueError('Missing footer or inconsistent file size; recording is incomplete')
        if status != 0 or rows != expected:
            raise ValueError(f'Run incomplete: status={status}, rows={rows}; 1=user, 2=buffer full, 3=I/O, 4=timer')
        file.seek(layout.size)
        # Validate all indices and timestamps before creating output.
        previous = None
        records = []
        for i in range(rows):
            index, actual, torque, execution = ROW.unpack(file.read(ROW.size))
            if index != i or (previous is not None and actual < previous):
                raise ValueError(f'Invalid index/timestamp at row {i}')
            if i == 0 and actual != 0:
                raise ValueError('First timestamp must be zero')
            if not math.isfinite(torque):
                raise ValueError(f'Non-finite torque at row {i}')
            records.append((index, index * period, actual, 0 if previous is None else actual-previous, torque, execution))
            previous = actual
        if records[-1][4] != 0:
            raise ValueError('Final torque must be zero')
    with destination.open('x', newline='', encoding='utf-8') as file:
        writer = csv.writer(file)
        writer.writerow(['index', 'nominal_us', 'actual_us', 'period_us', 'torque_Nm', 'ISR_us'])
        writer.writerows(records)
    destination.with_suffix('.config.json').write_text(json.dumps(config, indent=2) + '\n', encoding='utf-8')
    print(f'PASS: {rows} rows -> {destination}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    args = parser.parse_args()
    try:
        result = convert(args.binary)
        if isinstance(result, dict) and not result['complete']:
            sys.exit(2)
    except (OSError, ValueError, struct.error) as exc:
        print(f'FAIL: {exc}', file=sys.stderr)
        sys.exit(1)
