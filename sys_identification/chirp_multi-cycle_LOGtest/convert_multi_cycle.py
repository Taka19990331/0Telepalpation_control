"""Convert MC1 binary logs copied from the chirp_multi-cycle SD card to CSV."""
import argparse
import csv
import json
from pathlib import Path
import struct
import sys


HEADER = struct.Struct('<8s13I13f3I2f3I')
SAMPLE = struct.Struct('<3I6f7I')
FOOTER = struct.Struct('<8s3I')
COLUMNS = [
    'index', 'nominal_us', 'actual_us', 'period_us', 'phase', 'position_deg',
    'speed_rad_s', 'feedback_torque_estimate_Nm', 'chirp_Nm', 'pd_Nm',
    'command_Nm', 'torque_limited', 'feedback_age_us', 'feedback_sequence',
    'can_tx_accepted', 'ISR_us', 'motor_error', 'feedback_raw',
]
STATUS = ['COMPLETE', 'USER_ABORT', 'RING_FULL', 'SD_ERROR', 'TIMER_ERROR',
          'TX_FAILED', 'FEEDBACK_LOST', 'MOTOR_ERROR', 'POSITION_INVALID',
          'TORQUE_LIMIT', 'CONTROL_DEADLINE', 'CAN_ERROR', 'START_TIMEOUT',
          'STOP_FAILED', 'REFERENCE_INVALID', 'NONFINITE_CONTROL', 'RECORD_LIMIT']


def convert(source):
    data = source.read_bytes()
    if len(data) < HEADER.size + FOOTER.size:
        raise ValueError('File is too short for an MC1 header and footer')
    h = HEADER.unpack_from(data)
    if h[0] != b'CHIRPMC1':
        raise ValueError('Not a CHIRPMC1 file')
    (period, sweep, pre, post, expected, row_bytes, motor_id, bitrate,
     no_motor, position_policy, feedback_timeout, profile_documented,
     motor_timeout) = h[1:14]
    (f0, f1, amp0, amp1, exponent, kp, kd, hold_deg, torque_limit,
     pos_min, pos_max, velocity_max, mit_torque_max) = h[14:27]
    average_us, reference_samples, average_rows = h[27:30]
    reference_min, reference_max = h[30:32]
    hold_policy, can_port, motor_protocol = h[32:35]
    if period <= 0 or row_bytes != SAMPLE.size or hold_policy & 0xFFFF != 2:
        raise ValueError('Invalid MC1 timing, row size, or averaging policy')
    cycle_count = hold_policy >> 16
    if cycle_count == 0 or average_us != pre or pre % period or sweep % period or post % period:
        raise ValueError('Unexpected MC1 cycle configuration')
    logged_steps = cycle_count * ((pre + sweep) // period)
    if expected != logged_steps + cycle_count * 4 + 1:
        raise ValueError('Header expectedRows does not match MC1 capacity')

    footer = FOOTER.unpack_from(data, len(data) - FOOTER.size)
    magic, rows, status, stop_flags = footer
    if magic != b'ENDMC001' or status >= len(STATUS):
        raise ValueError('Missing MC1 footer or unknown status')
    if len(data) != HEADER.size + rows * SAMPLE.size + FOOTER.size or rows > expected:
        raise ValueError('File size does not match its row count')
    if status == 0 and rows != logged_steps:
        raise ValueError('Completed run has an unexpected row count')

    config = {
        'Hz': 1_000_000 / period, 'Period_us': period, 'Sweep_us': sweep,
        'Pre_us': pre, 'Post_us': post, 'Duration_s': sweep / 1e6,
        'PreHold_s': pre / 1e6, 'PostHold_s': post / 1e6,
        'F0_Hz': f0, 'F1_Hz': f1, 'A0_Nm': amp0, 'A1_Nm': amp1,
        'AmplitudeExponent': exponent, 'KP_Nm_per_deg': kp,
        'KD_Nm_per_rad_s': kd, 'Hold_deg': hold_deg,
        'TorqueLimit_Nm': torque_limit, 'ExpectedRows': expected,
        'CycleCount': cycle_count, 'PostHoldRecorded': 0,
        'Average_us': average_us, 'ReferenceSamples': reference_samples,
        'AverageRows': average_rows, 'ReferenceMin_rad': reference_min,
        'ReferenceMax_rad': reference_max, 'HoldPolicy': hold_policy,
        'MotorID': motor_id, 'CAN_bitrate': bitrate, 'NoMotor': no_motor,
        'PositionPolicy': position_policy, 'FeedbackTimeout_us': feedback_timeout,
        'ProfileDocumented': profile_documented, 'MotorTimeout_ms': motor_timeout,
        'PositionMin_rad': pos_min, 'PositionMax_rad': pos_max,
        'VelocityMax_rad_s': velocity_max, 'MITTorqueMax_Nm': mit_torque_max,
        'CANPort': can_port, 'MotorProtocol': motor_protocol,
    }

    csv_path = source.with_suffix('.csv')
    config_path = source.with_suffix('.config.json')
    validation_path = source.with_suffix('.validation.json')
    previous_actual = None
    previous_index = None
    with csv_path.open('x', newline='', encoding='utf-8') as output:
        writer = csv.writer(output)
        writer.writerow(COLUMNS)
        offset = HEADER.size
        for row_number in range(rows):
            v = SAMPLE.unpack_from(data, offset + row_number * SAMPLE.size)
            index, actual, phase = v[:3]
            if phase not in (0, 1, 3) or (previous_index is not None and index <= previous_index):
                raise ValueError(f'Invalid phase or non-increasing time index at row {row_number}')
            if previous_actual is not None and actual < previous_actual:
                raise ValueError(f'Non-monotonic actual timestamp at row {row_number}')
            delta = 0 if previous_actual is None else actual - previous_actual
            writer.writerow([index, index * period, actual, delta, phase, *v[3:]])
            previous_index, previous_actual = index, actual

    config_path.write_text(json.dumps(config, indent=2, allow_nan=True) + '\n', encoding='utf-8')
    result = {'valid': True, 'complete': status == 0, 'rows': rows,
              'status': status, 'status_name': STATUS[status], 'stop_flags': stop_flags,
              'config': config, 'errors': []}
    validation_path.write_text(json.dumps(result, indent=2, allow_nan=True) + '\n', encoding='utf-8')
    return result, csv_path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', nargs='+', type=Path, help='one or more MC####.BIN files')
    args = parser.parse_args()
    exit_code = 0
    for source in args.binary:
        try:
            result, output = convert(source)
            print(f"{source.name}: {result['status_name']}, {result['rows']} rows -> {output}")
            if not result['complete']:
                exit_code = 2
        except (OSError, ValueError, struct.error) as exc:
            print(f'{source}: {exc}', file=sys.stderr)
            exit_code = 1
    return exit_code


if __name__ == '__main__':
    raise SystemExit(main())
