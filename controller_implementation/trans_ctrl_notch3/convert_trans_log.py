"""Convert TCBIN2/TCBIN3 microSD logs to CSV, validating sequence and CRC."""
import argparse
import csv
from pathlib import Path
import struct
import zlib

RECORD_V2 = struct.Struct('<I10I4f9f?3x2f2B2xII')
RECORD_V3 = struct.Struct('<I10I4f17fB?2x2f2B2xII')
RECORD = RECORD_V3  # Current format; retained import name for tests/tools.
BASE_HEADER = ('timestamp_us,position_rad,velocity_raw_rad_s,velocity_filtered_rad_s,'
               'feedback_torque_Nm,torque_bias_Nm,feedback_age_us,friction_model_Nm,'
               'rfob_input_Nm,rfob_filter_state_Nm,external_torque_est_Nm,torque_RFOB_Nm,'
               'torque_FF_Nm,torque_cmd_unsat_Nm,torque_cmd_Nm,K_R,K_F,control_enabled,'
               'torque_saturated,motor_error,fault_code,tx_queue_count,isr_interval_us,'
               'previous_isr_duration_us,can_snapshot_time_us,can_esr1,can_ecr,'
               'can_write_success,can_write_failure')
EXTRA_HEADER = ('angle_bpf_rad,angle_vibration_rms_rad,mean_velocity_rad_s,gamma_angle,'
                'gamma_speed,notch_application_rate,notch_input_external_torque_Nm,'
                'notch_filtered_external_torque_Nm,notch_mode')
HEADER = BASE_HEADER + ',' + EXTRA_HEADER + ',angle_vibration_rms_deg'


def decode(raw, expected, version=3):
    record = RECORD_V3 if version == 3 else RECORD_V2
    if len(raw) != record.size:
        raise ValueError(f'truncated record at sequence {expected}')
    if zlib.crc32(raw[:-4]) != struct.unpack_from('<I', raw, record.size-4)[0]:
        raise ValueError(f'CRC mismatch at sequence {expected}')
    v = record.unpack(raw)
    if v[0] == 0xffffffff:
        if v[1] != expected:
            raise ValueError('end record row count mismatch')
        return None
    if v[0] != expected:
        raise ValueError(f'sequence mismatch: expected {expected}, got {v[0]}')
    row = [v[1], v[11], v[12], v[15], v[13], v[14], v[2],
           *v[16:24], v[25] if version == 2 else v[33],
           v[26] if version == 2 else v[34], v[27] if version == 2 else v[35],
           int(v[24] if version == 2 else v[32]),
           v[28] if version == 2 else v[36], v[29] if version == 2 else v[37],
           *v[3:11]]
    if version == 3:
        row.extend([*v[24:32], v[32], v[25]*57.2957795131])
    return row


def convert(source, destination=None, recover=False):
    source = Path(source)
    destination = Path(destination) if destination else source.with_suffix('.csv')
    if source.resolve() == destination.resolve():
        raise ValueError('input and output must differ')
    partial = destination.with_suffix(destination.suffix + '.partial')
    rows = 0
    complete = False
    with source.open('rb') as src, partial.open('w', newline='', encoding='utf-8') as out:
        version = None
        while True:
            line = src.readline(4096)
            if not line or len(line) >= 4096:
                raise ValueError('TCBIN header missing')
            stripped = line.rstrip(b'\r\n')
            if stripped == b'#binary,TCBIN2,120': version = 2; break
            if stripped == b'#binary,TCBIN3,152': version = 3; break
            if not line.startswith(b'#'):
                raise ValueError('invalid metadata')
            out.write(line.decode('ascii').rstrip('\r\n') + '\n')
        out.write((BASE_HEADER if version == 2 else HEADER) + '\n')
        writer = csv.writer(out, lineterminator='\n')
        try:
            record = RECORD_V3 if version == 3 else RECORD_V2
            while True:
                raw = src.read(record.size)
                row = decode(raw, rows, version)
                if row is None:
                    footer = src.read().decode('ascii')
                    values = record.unpack(raw)
                    status = values[-2]
                    if f'#rows,{rows}' not in footer.splitlines() or f'#status,{status}' not in footer.splitlines():
                        raise ValueError('missing completion footer')
                    out.write(footer)
                    complete = True
                    break
                writer.writerow(format(x, '.7g') if isinstance(x, float) else x for x in row)
                rows += 1
        except (ValueError, UnicodeDecodeError) as exc:
            if not recover:
                raise
            out.write(f'#recovery,incomplete,{exc}\n#recovered_rows,{rows}\n')
    target = destination if complete else destination.with_suffix('.recovered.csv')
    partial.replace(target)
    return target, rows, complete


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('-o', '--output', type=Path)
    parser.add_argument('--recover', action='store_true', help='save validated prefix as .recovered.csv')
    args = parser.parse_args()
    try:
        path, count, complete = convert(args.binary, args.output, args.recover)
    except (ValueError, OSError) as exc:
        parser.exit(1, f'ERROR: {exc}\n')
    print(f'{path}: {count} rows, complete={complete}')
