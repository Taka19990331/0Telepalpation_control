"""CHIRPSI1: zero-torque initial-position average, then PD + 20 s chirp."""
import csv
import json
import math
import struct
from chirp_sd3 import (COLUMNS, INT_KEYS, FLOAT_KEYS, ROW, FOOTER, chirp, phase_at,
                       validate_config as validate_legacy, decode_header as decode_legacy,
                       HEADER_V5, csv_record)
HEADER = HEADER_V5
STATUS = ['COMPLETE','USER_ABORT','RING_FULL','SD_ERROR','TIMER_ERROR','TX_FAILED',
          'FEEDBACK_LOST','MOTOR_ERROR','POSITION_INVALID','TORQUE_LIMIT','CONTROL_DEADLINE',
          'CAN_ERROR','START_TIMEOUT','STOP_FAILED','REFERENCE_INVALID','NONFINITE_CONTROL','RECORD_LIMIT']
EXTRA_STEPS = 4

def save_csv(path, records, c, result):
    """Add readable experiment metadata on the PC, after acquisition only."""
    comments = [
        'FORMAT = CHIRPSI1_CSV_WITH_METADATA_V1',
        'CHIRP_TYPE = logarithmic',
        'TIME_DEFINITION = t: nominal elapsed time since chirp start [s]',
        'CHIRP_INTERVAL = 0 <= t < T; tau_chirp = 0 outside this interval',
        'FREQUENCY_RATE_FORMULA = k = ln(f1/f0)/T',
        'FREQUENCY_FORMULA = f(t) = f0*exp(k*t)',
        'NORMALIZED_FREQUENCY_FORMULA = x(t) = clip((f(t)-f0)/(f1-f0), 0, 1)',
        'AMPLITUDE_FORMULA = A(t) = A0 + (A1-A0)*x(t)^a',
        'CHIRP_TORQUE_FORMULA = tau_chirp(t) = A(t)*sin(2*pi*f0*(exp(k*t)-1)/k)',
        'PD_TORQUE_FORMULA = tau_PD = Kp*(theta_ref-theta) - Kd*omega',
        'POSITION_DEFINITION = theta_ref and theta [deg]; omega [rad/s]',
        'COMMAND_FORMULA = tau_command = tau_chirp + tau_PD during sweep',
        'PRE_HOLD_POLICY = zero commanded torque; PD disabled; average fresh feedback positions',
        'POST_HOLD_POLICY = tau_chirp = 0; PD enabled',
        'TORQUE_LIMIT_ACTION = abort with zero command when abs(tau_chirp + tau_PD) > tau_limit',
        'PHASE_DEFINITION = 0: initial average; 1: sweep; 2: PD only; 3: stop',
    ]
    if c is not None:
        for label, key in [
            ('START_FREQUENCY_HZ (f0)', 'F0_Hz'),
            ('END_FREQUENCY_HZ (f1)', 'F1_Hz'),
            ('START_AMPLITUDE_NM (A0)', 'A0_Nm'),
            ('END_AMPLITUDE_NM (A1)', 'A1_Nm'),
            ('AMPLITUDE_EXPONENT (a)', 'AmplitudeExponent'),
            ('SWEEP_DURATION_S (T)', 'Duration_s'),
            ('KP_PD (Kp) [Nm/deg]', 'KP_Nm_per_deg'),
            ('KD_PD (Kd) [Nm/(rad/s)]', 'KD_Nm_per_rad_s'),
            ('REFERENCE_POSITION_DEG (theta_ref)', 'Hold_deg'),
            ('SAMPLING_FREQUENCY_HZ (fs)', 'Hz'),
            ('PRE_HOLD_DURATION_S (T_pre)', 'PreHold_s'),
            ('POST_HOLD_DURATION_S (T_post)', 'PostHold_s'),
            ('TORQUE_LIMIT_NM (tau_limit)', 'TorqueLimit_Nm'),
        ]:
            comments.append(f'{label} = {c.get(key, "unavailable")}')
        comments.append('REFERENCE_VALID_FOR_PD = ' + str(
            c.get('ReferenceSamples', 0) >= 2 and any(r[4] in (1, 2) for r in records)))
        # Retain every saved field, including hardware, averaging and diagnostics.
        comments.extend(f'CONFIG.{key} = {value}' for key, value in c.items())
    first_sweep = next((r for r in records if r[4] == 1), None)
    comments.append('CHIRP_START_RECORD_TIME_S (t_start) = ' +
                    (str(first_sweep[2]/1e6) if first_sweep is not None else 'unavailable'))
    comments.append('CHIRP_TIME_FORMULA = t = (index - sweep_start_index)/fs; actual_us is measured record time')
    comments.append('SWEEP_START_INDEX (sweep_start_index) = ' +
                    (str(first_sweep[0]) if first_sweep is not None else 'unavailable'))
    for key in ('valid', 'complete', 'rows', 'status_name', 'stop_flags', 'errors'):
        comments.append(f'RESULT.{key} = {result.get(key, "unavailable")}')
    with path.open('x', newline='', encoding='utf-8') as file:
        file.writelines('# ' + line + '\n' for line in comments)
        writer = csv.writer(file)
        writer.writerow(COLUMNS)
        writer.writerows(records)
    path.with_suffix('.config.json').write_text(json.dumps(c, indent=2) + '\n', encoding='utf-8')
    path.with_suffix('.validation.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')

def decode_header(data):
    v = HEADER.unpack(data)
    if v[0] != b'CHIRPSI1': raise ValueError('Unsupported SI1 header')
    c = decode_legacy(HEADER.pack(b'CHIRPSD5',*v[1:]))
    for key in ('ZeroingTimeout_us','Settle_us','HomingRows','SettlePosition_deg','SettleSpeed_rad_s'):
        c.pop(key)
    c.update(zip(('Average_us','ReferenceSamples','AverageRows','ReferenceMin_rad','ReferenceMax_rad'),v[27:32]))
    return c

def validate_config(c):
    # Base validator receives a normalized legacy-compatible configuration.
    validate_legacy(c)

def normalized(c):
    if c['HoldPolicy'] != 2 or c['RowBytes'] != ROW.size or c['Average_us'] != c['Pre_us'] or c['Average_us'] <= 0:
        raise ValueError('Invalid initial-position average policy')
    period=c['Period_us']
    if period <= 0 or c['Average_us'] % period: raise ValueError('Invalid average duration')
    count=c['ReferenceSamples']; rows=c['AverageRows']
    if count<0 or count!=int(count) or rows<0 or rows!=int(rows) or rows>c['Average_us']/period+EXTRA_STEPS:
        raise ValueError('Invalid average count')
    expected=round((c['PreHold_s']+c['Duration_s']+c['PostHold_s'])*1e6/period)+1+EXTRA_STEPS
    if c['ExpectedRows']!=expected or expected*4*period>=2**32 or HEADER.size+expected*ROW.size+FOOTER.size>2**32-1:
        raise ValueError('Invalid SI1 capacity')
    for raw,seconds in (('Pre_us','PreHold_s'),('Sweep_us','Duration_s'),('Post_us','PostHold_s')):
        if not math.isclose(c[raw],c[seconds]*1e6,abs_tol=1e-5): raise ValueError('Inconsistent duration')
    if count:
        if not all(math.isfinite(c[k]) for k in ('ReferenceMin_rad','ReferenceMax_rad','Hold_deg')): raise ValueError('Invalid reference mean')
        if not c['PositionMin_rad']<c['ReferenceMin_rad']<=math.radians(c['Hold_deg'])+1e-6<=c['ReferenceMax_rad']+2e-6<c['PositionMax_rad']:
            raise ValueError('Reference outside observed position range')
    n=dict(c,PreHold_s=rows*period/1e6,ExpectedRows=rows+round((c['Duration_s']+c['PostHold_s'])*1e6/period)+1)
    validate_legacy(n)
    return n

def validate_records_base(records, c, status, stop_flags):
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
        if expected_phase in (0,3) or safety_zero:
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

def validate_records(records,c,status,flags):
    n=normalized(c)
    if status==0 or any(r[4]==1 for r in records):
        if c['ReferenceSamples']<2 or c['AverageRows']*c['Period_us']<c['Average_us']:
            raise ValueError('Sweep without initial average')
        if len(records)>c['AverageRows'] and records[int(c['AverageRows'])][2]<c['Average_us']:
            raise ValueError('Sweep before 2 second averaging interval')
    result=validate_records_base(records,n,status,flags)
    result['config']=c
    return result

def convert(source):
    with source.open('rb') as f:
        c=decode_header(f.read(HEADER.size)); normalized(c)
        f.seek(-FOOTER.size,2); magic,rows,status,flags=FOOTER.unpack(f.read(FOOTER.size))
        if magic!=b'ENDSI001' or rows>c['ExpectedRows'] or source.stat().st_size!=HEADER.size+rows*ROW.size+FOOTER.size: raise ValueError('Invalid SI1 footer/size')
        f.seek(HEADER.size); records=[]; previous=None
        for _ in range(rows):
            v=ROW.unpack(f.read(ROW.size)); records.append(csv_record(v,c['Period_us'],previous)); previous=v[1]
    result=validate_records(records,c,status,flags)
    save_csv(source.with_suffix('.csv'),records,c,result); return result

def extract(raw_path,csv_path):
    c=None; records=[]; status=flags=summary=None; begun=ended=header=False; reason=None; errors=[]
    for line in raw_path.read_text(encoding='ascii').splitlines():
        f=line.split(',')
        try:
            if f[0]=='CHIRP_BEGIN':
                if begun or f[1:4]!=['VERSION','SI1','REASON']: raise ValueError('Invalid SI1 begin')
                begun=True; reason=f[4]
            elif f[0]=='CONFIG': c={f[i]:float(f[i+1]) for i in range(1,len(f),2)}; normalized(c)
            elif f[0]=='RESULT':
                d=dict(zip(f[1::2],f[2::2])); status=int(d['Status']); flags=int(d['StopFlags'])
            elif f[0]=='DATA':
                if f[1:]==COLUMNS: header=True
                else:
                    if not begun or not header or c is None: raise ValueError('Missing SI1 header/config')
                    records.append([float(v) if 5<=i<=10 else int(v) for i,v in enumerate(f[1:])])
            elif f[0]=='SUMMARY': summary=int(f[2])
            elif f[0]=='CHIRP_END': ended=True; break
            elif f[0]=='ERROR': raise ValueError(line)
        except (ValueError,KeyError,IndexError) as e: errors.append(str(e))
    try:
        if not(begun and ended and header and c is not None) or summary!=len(records) or reason!=('COMPLETE' if status==0 else 'ABORTED'): raise ValueError('Incomplete SI1 capture')
        result=validate_records(records,c,status,flags)
    except (ValueError,KeyError,TypeError) as e:
        errors.append(str(e)); result=dict(valid=False,complete=False,rows=len(records),config=c,errors=[])
    result['errors']+=errors
    if errors: result.update(valid=False,complete=False)
    save_csv(csv_path,records,c,result); return result
