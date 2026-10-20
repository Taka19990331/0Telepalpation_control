"""CHIRPSD3/4/5 format and validation shared by binary/Serial tools (little endian)."""
import csv
import json
import math
import struct

HEADER = struct.Struct('<8s13I13f')
HEADER_V4 = struct.Struct('<8s13I13f3I2fI')
HEADER_V5 = struct.Struct('<8s13I13f3I2f3I')
ROW = struct.Struct('<3I6f7I')
FOOTER = struct.Struct('<8s3I')
COLUMNS = ['index', 'nominal_us', 'actual_us', 'period_us', 'phase', 'position_deg',
           'speed_rad_s', 'feedback_torque_estimate_Nm', 'chirp_Nm', 'pd_Nm',
           'command_Nm', 'torque_limited', 'feedback_age_us', 'feedback_sequence',
           'can_tx_accepted', 'ISR_us', 'motor_error', 'feedback_raw']
STATUS = ['COMPLETE', 'USER_ABORT', 'RING_FULL', 'SD_ERROR', 'TIMER_ERROR', 'TX_FAILED',
          'FEEDBACK_LOST', 'MOTOR_ERROR', 'POSITION_INVALID', 'TORQUE_LIMIT',
          'CONTROL_DEADLINE', 'CAN_ERROR', 'START_TIMEOUT', 'STOP_FAILED', 'ZEROING_TIMEOUT']
INT_KEYS = ['Period_us', 'Sweep_us', 'Pre_us', 'Post_us', 'ExpectedRows', 'RowBytes',
            'MotorID', 'CAN_bitrate', 'NoMotor', 'PositionPolicy', 'FeedbackTimeout_us',
            'ProfileVerified', 'MotorTimeout_ms']
FLOAT_KEYS = ['F0_Hz', 'F1_Hz', 'A0_Nm', 'A1_Nm', 'AmplitudeExponent', 'KP_Nm_per_deg',
              'KD_Nm_per_rad_s', 'Hold_deg', 'TorqueLimit_Nm', 'PositionMin_rad',
              'PositionMax_rad', 'VelocityMax_rad_s', 'MITTorqueMax_Nm']


def decode_header(data):
    layout = HEADER_V5 if data[:8] == b'CHIRPSD5' else HEADER_V4 if data[:8] == b'CHIRPSD4' else HEADER
    values = layout.unpack(data)
    if values[0] not in (b'CHIRPSD3', b'CHIRPSD4', b'CHIRPSD5'):
        raise ValueError('Unsupported motor SD header')
    c = dict(zip(INT_KEYS + FLOAT_KEYS, values[1:27]))
    if layout in (HEADER_V4, HEADER_V5):
        c['ProfileDocumented'] = c.pop('ProfileVerified')
        c.update(zip(['ZeroingTimeout_us','Settle_us','HomingRows','SettlePosition_deg',
                      'SettleSpeed_rad_s','HoldPolicy'], values[27:33]))
    if layout == HEADER_V5:
        c.update(zip(['CANPort','MotorProtocol'],values[33:]))
    c.update(Hz=1e6 / c['Period_us'] if c['Period_us'] else 0,
             Duration_s=c['Sweep_us'] / 1e6, PreHold_s=c['Pre_us'] / 1e6,
             PostHold_s=c['Post_us'] / 1e6)
    return c


def validate_config(c):
    if c.get('RowBytes', ROW.size) != ROW.size:
        raise ValueError('Unsupported SD3 row size')
    period = c['Period_us']
    if period <= 0 or period != int(period):
        raise ValueError('Invalid period')
    if not math.isclose(c['Hz'], 1e6 / period, abs_tol=1e-6, rel_tol=1e-10):
        raise ValueError('Inconsistent sampling frequency')
    for key in ('Duration_s', 'PreHold_s', 'PostHold_s'):
        steps = c[key] * 1e6 / period
        if not math.isfinite(steps) or steps < 0 or not math.isclose(steps, round(steps), abs_tol=1e-6, rel_tol=0):
            raise ValueError('Noninteger recording duration: ' + key)
    if 'HomingRows' in c:
        if (c['HomingRows'] < 0 or int(c['HomingRows']) != c['HomingRows'] or
                c['HomingRows']*period > c['ZeroingTimeout_us'] or c['HoldPolicy'] != 1 or
                not 0 < c['Settle_us'] <= c['ZeroingTimeout_us'] or
                c['SettlePosition_deg'] <= 0 or c['SettleSpeed_rad_s'] <= 0):
            raise ValueError('Invalid zeroing configuration')
    expected = c.get('HomingRows',0) + round((c['Duration_s'] + c['PreHold_s'] + c['PostHold_s']) * 1e6 / period) + 1
    if c['ExpectedRows'] != expected or c['Duration_s'] <= 0:
        raise ValueError('Inconsistent expected row count')
    keys = [key for key in FLOAT_KEYS if key != 'Hold_deg']
    if not all(math.isfinite(c[key]) for key in keys):
        raise ValueError('Non-finite configuration')
    if not (0 < c['F0_Hz'] < c['F1_Hz'] < 5e5 / period and c['AmplitudeExponent'] > 0 and
            min(c['A0_Nm'], c['A1_Nm'], c['KP_Nm_per_deg'], c['KD_Nm_per_rad_s']) >= 0 and
            0 < c['TorqueLimit_Nm'] <= c['MITTorqueMax_Nm'] and
            c['PositionMin_rad'] < c['PositionMax_rad'] and c['VelocityMax_rad_s'] > 0):
        raise ValueError('Invalid sweep/PD/protocol configuration')
    if (c['NoMotor'] not in (0, 1) or c['PositionPolicy'] != 0 or
            c['FeedbackTimeout_us'] <= 0 or c['CAN_bitrate'] != 1000000):
        raise ValueError('Unsupported motor/position policy')
    if 'MotorProtocol' in c:
        if (c['CANPort'],c['MotorID'],c['MotorProtocol']) not in ((1,1,0),(2,2,0),(3,3,1)):
            raise ValueError('Inconsistent motor selection')
    elif c['MotorID'] not in (1,2):
        raise ValueError('Unsupported legacy motor ID')


def phase_at(index, c):
    home = c.get('HomingRows',0)
    if index < home:
        return 4
    index -= home
    pre = round(c['PreHold_s'] * 1e6 / c['Period_us'])
    sweep = round(c['Duration_s'] * 1e6 / c['Period_us'])
    return 0 if index < pre else 1 if index < pre + sweep else 2 if index < c['ExpectedRows'] - home - 1 else 3


def chirp(t, c):
    if t < 0 or t >= c['Duration_s']:
        return 0.0
    k = math.log(c['F1_Hz'] / c['F0_Hz']) / c['Duration_s']
    growth = math.expm1(k * t)
    f = c['F0_Hz'] * (1 + growth)
    fraction = max(0.0, min(1.0, (f-c['F0_Hz'])/(c['F1_Hz']-c['F0_Hz'])))
    amplitude = c['A0_Nm'] + (c['A1_Nm']-c['A0_Nm']) * fraction ** c['AmplitudeExponent']
    return amplitude * math.sin(2 * math.pi * c['F0_Hz'] * growth / k)


def csv_record(values, period, previous):
    index, actual, phase, *rest = values
    return [index, index * period, actual, 0 if previous is None else actual-previous, phase, *rest]


def validate_records(records, c, status, stop_flags):
    validate_config(c)
    if status not in range(len(STATUS)) or not 0 <= stop_flags <= 31:
        raise ValueError('Unknown run status/stop flags')
    if len(records) > c['ExpectedRows'] or (status == 0 and len(records) != c['ExpectedRows']):
        raise ValueError('Row count inconsistent with completion status')
    if records and not math.isfinite(c['Hold_deg']):
        raise ValueError('Missing trial hold position')
    previous = None
    sequence = 0
    settled_since = None
    for i, r in enumerate(records):
        if len(r) != len(COLUMNS):
            raise ValueError(f'Incorrect columns at row {i}')
        integers = [*r[:5], *r[11:]]
        if any(not math.isfinite(v) or v < 0 or int(v) != v for v in integers):
            raise ValueError(f'Invalid integer field at row {i}')
        if not all(math.isfinite(v) for v in r[5:11]):
            raise ValueError(f'Non-finite row {i}')
        if r[0] != i or r[1] != i * c['Period_us'] or (i == 0 and r[2] != 0):
            raise ValueError(f'Invalid index/nominal/initial time at row {i}')
        if r[3] != (0 if previous is None else r[2]-previous) or (previous is not None and r[2] < previous):
            raise ValueError(f'Invalid period at row {i}')
        previous = r[2]
        if r[11] not in (0, 1) or r[14] not in (0, 1) or r[16] > 255 or r[17] > (65535 if c.get('MotorProtocol',0) == 1 else 4095):
            raise ValueError(f'Invalid flags/raw data at row {i}')
        override = status == 9 and i == len(records)-1 and r[4] == 3 and r[11] == 1
        safety_zero = status != 0 and i == len(records)-1 and r[4] == 3 and r[11] == 0
        expected_phase = phase_at(i, c)
        if 'HomingRows' in c:
            if abs(c['Hold_deg']) > 1e-6:
                raise ValueError('SD4 target must be the existing driver zero')
            if i <= c['HomingRows']:
                in_tolerance = abs(r[5]) <= c['SettlePosition_deg'] and abs(r[6]) <= c['SettleSpeed_rad_s']
                if in_tolerance and settled_since is None:
                    settled_since = r[2]
                elif not in_tolerance:
                    settled_since = None
                if i == c['HomingRows'] and status == 0 and (settled_since is None or r[2]-settled_since < c['Settle_us']):
                    raise ValueError('Experiment began without continuous zero settling')
        if r[4] != expected_phase and not (override or safety_zero):
            raise ValueError(f'Invalid phase boundary at row {i}')
        if (r[12] > c['FeedbackTimeout_us'] and not safety_zero) or r[13] < sequence or r[13] == 0:
            raise ValueError(f'Stale feedback or invalid sequence at row {i}')
        sequence = r[13]
        expected_chirp = chirp((r[0]-c.get('HomingRows',0))*c['Period_us']/1e6-c['PreHold_s'], c) if expected_phase == 1 and not safety_zero else 0
        if not math.isclose(r[8], expected_chirp, abs_tol=2e-3, rel_tol=1e-5):
            raise ValueError(f'Chirp formula mismatch at row {i}')
        pd = c['KP_Nm_per_deg']*(c['Hold_deg']-r[5])-c['KD_Nm_per_rad_s']*r[6]
        if expected_phase == 3 or safety_zero:
            pd = 0
        if not math.isclose(r[9], pd, abs_tol=2e-5, rel_tol=2e-5):
            raise ValueError(f'PD formula/unit mismatch at row {i}')
        total = r[8]+r[9]
        cmd = 0 if override or safety_zero else max(-c['TorqueLimit_Nm'], min(c['TorqueLimit_Nm'], total))
        if (not math.isclose(r[10], cmd, abs_tol=2e-5, rel_tol=2e-5) or
                abs(r[10]) > c['TorqueLimit_Nm']+1e-5):
            raise ValueError(f'Invalid command at row {i}')
        if r[11] != int(abs(total) > c['TorqueLimit_Nm']) and not math.isclose(abs(total), c['TorqueLimit_Nm'], abs_tol=2e-5):
            raise ValueError(f'Invalid torque limit flag at row {i}')
        if c['NoMotor'] and r[14]:
            raise ValueError('No-motor record must not claim a CAN request')
        if status == 0 and (r[11] or r[16] or (not c['NoMotor'] and r[4] != 3 and not r[14])):
            raise ValueError(f'Fault in normally completed row {i}')
    stopped = stop_flags == 16 if c['NoMotor'] else stop_flags == (3 if c.get('MotorProtocol',0) == 1 else 15)
    if status == 0 and not stopped:
        raise ValueError('Normal completion without required stop TX completion')
    return dict(valid=True, complete=status == 0, rows=len(records), status=status,
                status_name=STATUS[status], stop_flags=stop_flags,
                stop_tx_complete=stopped and not bool(c['NoMotor']),
                simulation_stop_complete=stopped and bool(c['NoMotor']),
                reason='COMPLETE' if status == 0 else 'ABORTED', config=c, errors=[])


def save_csv(destination, records, config, result):
    with destination.open('x', newline='', encoding='utf-8') as file:
        writer = csv.writer(file)
        writer.writerow(COLUMNS)
        writer.writerows(records)
    destination.with_suffix('.config.json').write_text(json.dumps(config, indent=2) + '\n', encoding='utf-8')
    destination.with_suffix('.validation.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')


def convert(source):
    with source.open('rb') as file:
        magic = file.read(8)
        layout = HEADER_V5 if magic == b'CHIRPSD5' else HEADER_V4 if magic == b'CHIRPSD4' else HEADER
        file.seek(0)
        c = decode_header(file.read(layout.size))
        validate_config(c)
        size = source.stat().st_size
        if size < layout.size + FOOTER.size:
            raise ValueError('Incomplete SD3 file')
        file.seek(-FOOTER.size, 2)
        magic, rows, status, flags = FOOTER.unpack(file.read(FOOTER.size))
        if (magic != (b'ENDSD005' if layout == HEADER_V5 else b'ENDSD004' if layout == HEADER_V4 else b'ENDSD003') or rows > c['ExpectedRows'] or
                size != layout.size + rows * ROW.size + FOOTER.size):
            raise ValueError('Missing SD3 footer or inconsistent size')
        file.seek(layout.size)
        records = []
        previous = None
        for _ in range(rows):
            values = ROW.unpack(file.read(ROW.size))
            records.append(csv_record(values, c['Period_us'], previous))
            previous = values[1]
    result = validate_records(records, c, status, flags)
    save_csv(source.with_suffix('.csv'), records, c, result)
    print(f"{'PASS' if result['complete'] else 'ABORTED'}: {rows} rows; {result['status_name']} -> {source.with_suffix('.csv')}")
    return result


def extract(raw_path, csv_path):
    c = None
    reason = None
    records = []
    status = flags = summary = None
    begun = ended = header = False
    errors = []
    with raw_path.open('rb') as file:
        for number, line in enumerate(file, 1):
            try:
                f = line.decode('ascii').strip().split(',')
                if f[0] == 'CHIRP_BEGIN':
                    if begun:
                        raise ValueError('Multiple begin markers')
                    begun = True
                    reason = f[f.index('REASON')+1]
                elif f[0] == 'CONFIG':
                    c = {f[i]: float(f[i+1]) for i in range(1, len(f), 2)}
                    validate_config(c)
                elif f[0] == 'RESULT':
                    values = dict(zip(f[1::2], f[2::2]))
                    status, flags = int(values['Status']), int(values['StopFlags'])
                elif f[0] == 'DATA':
                    if not begun or c is None:
                        raise ValueError('DATA before begin/config')
                    if f[1:] == COLUMNS:
                        header = True
                    else:
                        if not header or len(f)-1 != len(COLUMNS):
                            raise ValueError('Missing/invalid DATA header')
                        records.append([float(v) if 5 <= i <= 10 else int(v) for i, v in enumerate(f[1:])])
                elif f[0] == 'SUMMARY':
                    summary = int(f[f.index('Count')+1])
                elif f[0] == 'CHIRP_END':
                    ended = True
                    break
                elif f[0] == 'ERROR':
                    raise ValueError(','.join(f))
            except (ValueError, KeyError, IndexError, OverflowError, UnicodeError) as exc:
                errors.append(f'Line {number}: {exc}')
    try:
        if not (begun and ended and header and c is not None and status is not None):
            raise ValueError('Missing SD3 markers/config/result/header')
        if summary != len(records) or reason != ('COMPLETE' if status == 0 else 'ABORTED'):
            raise ValueError('Inconsistent summary/completion markers')
        result = validate_records(records, c, status, flags)
    except (ValueError, KeyError, TypeError, OverflowError) as exc:
        result = dict(valid=False, complete=False, rows=len(records), errors=[], config=c, reason=reason)
        errors.append(str(exc))
    result['errors'].extend(errors)
    result['valid'] = not result['errors']
    if not result['valid']:
        result['complete'] = False
    save_csv(csv_path, records, c, result)
    return result
