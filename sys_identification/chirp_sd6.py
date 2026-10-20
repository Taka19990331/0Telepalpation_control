"""SD6 cascade homing logs. Legacy SD3/4/5 readers remain separate."""
import csv
import json
import math
import struct
import chirp_sd3 as old

HEADER = struct.Struct('<8s13I13f3I2f3I7f4I')
ROW = struct.Struct('<3I6f7I5fI')
FOOTER = old.FOOTER
HOME_KEYS = ['HomingMaxVelocity','HomingMaxAcceleration','HomingTorqueLimit','HomingPositionK',
             'HomingVelocityKp','HomingVelocityKi','HomingTorqueSlew']
TAIL_KEYS = ['Handover_us','PreHoldTimeout_us','TransitionRows','PreHoldRows']
COLUMNS = old.COLUMNS + ['target_position_rad','target_velocity_rad_s','pi_proportional_Nm',
                         'pi_integral_Nm','raw_torque_Nm','slew_limited']
STATUS = old.STATUS + ['PRE_HOLD_TIMEOUT','HANDOVER_FAILED','NONFINITE_CONTROL','RECORD_LIMIT']

def decode_header(data):
    v = HEADER.unpack(data)
    if v[0] != b'CHIRPSD6': raise ValueError('Unsupported SD6 header')
    c = old.decode_header(old.HEADER_V5.pack(b'CHIRPSD5', *v[1:35]))
    c.update(zip(HOME_KEYS+TAIL_KEYS, v[35:]))
    return c

def validate_config(c):
    if c.get('RowBytes') != ROW.size: raise ValueError('SD6 row size mismatch')
    # Reuse experiment/protocol validation with the legacy expected-count convention.
    legacy = dict(c, RowBytes=old.ROW.size)
    legacy['ExpectedRows'] = c['HomingRows']+round((c['Duration_s']+c['PreHold_s']+c['PostHold_s'])*1e6/c['Period_us'])+1
    old.validate_config(legacy)
    if any(not math.isfinite(c[k]) or c[k] <= 0 for k in [k for k in HOME_KEYS if k != 'HomingVelocityKp']+['Handover_us','PreHoldTimeout_us']):
        raise ValueError('Invalid homing parameters')
    if not math.isfinite(c['HomingVelocityKp']) or c['HomingVelocityKp'] < 0: raise ValueError('Invalid velocity Kp')
    if c['HomingTorqueLimit'] > c['TorqueLimit_Nm'] or c['HomingMaxVelocity'] > c['VelocityMax_rad_s']:
        raise ValueError('Invalid homing limits')
    period = c['Period_us']
    durations = [c[k] for k in ('ZeroingTimeout_us','Handover_us','PreHoldTimeout_us','Sweep_us','Post_us')]
    for raw, seconds in (('Sweep_us','Duration_s'),('Pre_us','PreHold_s'),('Post_us','PostHold_s')):
        if not math.isclose(c[raw],c[seconds]*1e6,abs_tol=1e-5): raise ValueError('Inconsistent SD6 duration metadata')
    if any(v % period for v in durations): raise ValueError('Noninteger SD6 duration')
    if c['PreHoldTimeout_us'] < max(c['Pre_us'],c['Settle_us']): raise ValueError('Invalid prehold timeout')
    expected = sum(durations)/period+2
    if c['ExpectedRows'] != expected or expected*4*period >= 2**32 or HEADER.size+expected*ROW.size+FOOTER.size > 2**32-1:
        raise ValueError('Invalid SD6 capacity/time range')
    for key in ('HomingRows','TransitionRows','PreHoldRows'):
        if c[key] < 0 or c[key] != int(c[key]): raise ValueError('Invalid phase count')
    if (c['TransitionRows'] > c['Handover_us']/period+1 or
        c['PreHoldRows'] > c['PreHoldTimeout_us']/period or abs(c['Hold_deg']) > 1e-6):
        raise ValueError('Invalid SD6 phase count/target')

def validate_records(records,c,status,flags):
    validate_config(c)
    if status not in range(len(STATUS)) or not 0 <= flags <= 31: raise ValueError('Unknown status/flags')
    if records and not math.isfinite(c['Hold_deg']): raise ValueError('Missing target')
    if len(records) > c['ExpectedRows']: raise ValueError('Capacity exceeded')
    counts = [int(c[k]) for k in ('HomingRows','TransitionRows','PreHoldRows')]
    home, trans, pre = counts
    sweep_start = sum(counts)
    sweep_steps = round(c['Sweep_us']/c['Period_us'])
    post_steps = round(c['Post_us']/c['Period_us'])
    if status == 0 and (len(records) != sweep_start+sweep_steps+post_steps+1 or min(counts) <= 0):
        raise ValueError('Invalid complete phase lengths')
    handover_initial = 0
    prior = None
    sequence = 0
    settled_since = None
    for i,r in enumerate(records):
        if len(r) != len(COLUMNS) or not all(math.isfinite(v) for v in r): raise ValueError(f'Invalid row {i}')
        if any(v < 0 or int(v) != v for v in [*r[:5],*r[11:18],r[23]]): raise ValueError('Invalid integer')
        if r[0] != i or r[1] != i*c['Period_us'] or r[3] != (r[2]-prior[2] if prior else 0) or (not prior and r[2] != 0) or r[3] < 0:
            raise ValueError('Invalid index/time')
        stop = r[4] == 3 and i == len(records)-1
        expected = 4 if i < home else 5 if i < home+trans else 0 if i < sweep_start else 1 if i < sweep_start+sweep_steps else 2 if i < sweep_start+sweep_steps+post_steps else 3
        if r[4] != expected and not (stop and status != 0): raise ValueError('Invalid phase order')
        if any(r[k] not in (0,1) for k in (11,14,23)) or r[16]>255 or r[17]>(65535 if c.get('MotorProtocol')==1 else 4095): raise ValueError('Invalid flags/raw')
        if (r[12]>c['FeedbackTimeout_us'] and not stop) or r[13] < sequence or not r[13]: raise ValueError('Stale feedback')
        sequence = r[13]
        if c['NoMotor'] and r[14]: raise ValueError('Simulation CAN send')
        if abs(r[18]-math.radians(c['Hold_deg']))>1e-6: raise ValueError('Wrong target')
        pd = c['KP_Nm_per_deg']*(c['Hold_deg']-r[5])-c['KD_Nm_per_rad_s']*r[6] if r[4] not in (3,4) else 0
        # Torque-limit abort keeps the attempted PD/chirp in its diagnostic STOP row.
        override = stop and status == 9 and r[11]
        if not override and not math.isclose(r[9],pd,abs_tol=3e-5,rel_tol=3e-5): raise ValueError('PD changed')
        ch = old.chirp((i-sweep_start)*c['Period_us']/1e6,c) if r[4]==1 else 0
        if not override and not math.isclose(r[8],ch,abs_tol=.002,rel_tol=1e-5): raise ValueError('Chirp changed')
        limit = c['HomingTorqueLimit'] if r[4] in (4,5) else c['TorqueLimit_Nm']
        if abs(r[10])>limit+1e-6 or (stop and r[10]!=0): raise ValueError('Invalid torque command')
        if r[4]==4:
            if r[9]!=0 or abs(r[19])>c['HomingMaxVelocity']+1e-6: raise ValueError('Invalid homing velocity/PD')
            previous_v = prior[19] if prior and prior[4]==4 else 0
            dt = r[3]/1e6 if prior else c['Period_us']/1e6
            if abs(r[19]-previous_v)>c['HomingMaxAcceleration']*dt+1e-6: raise ValueError('Acceleration exceeded')
            if not math.isclose(r[20],c['HomingVelocityKp']*(r[19]-r[6]),abs_tol=3e-5): raise ValueError('PI P mismatch')
            if not math.isclose(r[22],r[20]+r[21],abs_tol=3e-5): raise ValueError('PI raw mismatch')
        if r[4]==5:
            if not prior or prior[4]!=5: handover_initial=prior[10] if prior else 0
            if r[19]!=0 or (prior and (abs(r[20]-prior[20])>3e-5 or abs(r[21]-prior[21])>3e-5)):
                raise ValueError('PI changed in handover')
            fraction=min(1,(r[2]-records[home][2])/c['Handover_us'])
            blend=(1-fraction)*handover_initial+fraction*r[9]
            if not math.isclose(r[22],blend,abs_tol=3e-5): raise ValueError('Handover blend mismatch')
        if prior and prior[4]==5 and r[4]==0:
            if prior[2]-records[home][2]<c['Handover_us'] or abs(prior[10]-prior[9])>3e-5:
                raise ValueError('Incomplete PI/PD handover')
        if r[4] in (4,5):
            dt = r[3]/1e6 if prior else c['Period_us']/1e6
            previous_cmd = prior[10] if prior else 0
            if abs(r[10]-previous_cmd)>c['HomingTorqueSlew']*dt+2e-6: raise ValueError('Torque slew exceeded')
            clipped=max(-limit,min(limit,r[22]))
            wanted=max(previous_cmd-c['HomingTorqueSlew']*dt,min(previous_cmd+c['HomingTorqueSlew']*dt,clipped))
            if not math.isclose(r[10],wanted,abs_tol=3e-5): raise ValueError('Limiter mismatch')
            if r[11]!=int(abs(r[22])>limit) and abs(abs(r[22])-limit)>1e-5: raise ValueError('Saturation flag mismatch')
        elif not stop:
            if any(r[k] != 0 for k in (19,20,21,23)): raise ValueError('PI leaked into experiment')
            if not math.isclose(r[22],r[8]+r[9],abs_tol=3e-5) or not math.isclose(r[10],r[22],abs_tol=3e-5): raise ValueError('Experiment torque mismatch')
        within = abs(r[5])<=c['SettlePosition_deg'] and abs(r[6])<=c['SettleSpeed_rad_s']
        if r[4] in (4,0):
            if not prior or prior[4]!=r[4]: settled_since=None
            if within and settled_since is None: settled_since=r[2]
            if not within: settled_since=None
        if prior and prior[4]==4 and r[4]==5:
            if not within or settled_since is None or r[2]-settled_since<c['Settle_us']: raise ValueError('Unsettled homing handover')
            settled_since=None
        if prior and prior[4]==0 and r[4]==1:
            if not within or settled_since is None or r[2]-settled_since<c['Settle_us']: raise ValueError('Sweep before PD settling')
            if r[2]-records[home+trans-1][2]<c['Pre_us']: raise ValueError('Prehold too short')
        if status==0 and (r[16] or (r[11] and r[4] not in (4,5)) or (not c['NoMotor'] and not stop and not r[14])): raise ValueError('Fault in complete run')
        prior=r
    stopped = flags == (16 if c['NoMotor'] else 3 if c.get('MotorProtocol')==1 else 15)
    if status==0 and not stopped: raise ValueError('Unconfirmed stop')
    return dict(valid=True,complete=status==0,rows=len(records),status=status,status_name=STATUS[status],stop_flags=flags,
                stop_tx_complete=stopped and not bool(c['NoMotor']),simulation_stop_complete=stopped and bool(c['NoMotor']),
                reason='COMPLETE' if status==0 else 'ABORTED',config=c,errors=[])

def save_csv(path,records,c,result):
    with path.open('x',newline='',encoding='utf-8') as f:
        w=csv.writer(f); w.writerow(COLUMNS); w.writerows(records)
    path.with_suffix('.config.json').write_text(json.dumps(c,indent=2)+'\n')
    path.with_suffix('.validation.json').write_text(json.dumps(result,indent=2)+'\n')

def convert(source):
    with source.open('rb') as f:
        c=decode_header(f.read(HEADER.size)); validate_config(c)
        f.seek(-FOOTER.size,2); magic,n,status,flags=FOOTER.unpack(f.read(FOOTER.size))
        if magic!=b'ENDSD006' or n>c['ExpectedRows'] or source.stat().st_size!=HEADER.size+n*ROW.size+FOOTER.size: raise ValueError('Invalid SD6 footer/size')
        f.seek(HEADER.size); records=[]; previous=None
        for _ in range(n):
            v=ROW.unpack(f.read(ROW.size)); records.append(old.csv_record(v,c['Period_us'],previous)); previous=v[1]
    result=validate_records(records,c,status,flags); save_csv(source.with_suffix('.csv'),records,c,result)
    return result

def extract(raw_path,csv_path):
    records=[]; c=None; status=flags=summary=None; begun=ended=header=False; reason=None
    errors=[]
    for line in raw_path.read_text(encoding='ascii').splitlines():
        f=line.split(',')
        try:
            if f[0]=='CHIRP_BEGIN':
                if begun or f[1:4]!=['VERSION','SD6','REASON']: raise ValueError('Invalid SD6 begin')
                begun=True; reason=f[4]
            elif f[0]=='CONFIG': c={f[i]:float(f[i+1]) for i in range(1,len(f),2)}; validate_config(c)
            elif f[0]=='RESULT':
                d=dict(zip(f[1::2],f[2::2])); status=int(d['Status']); flags=int(d['StopFlags'])
            elif f[0]=='DATA':
                if f[1:]==COLUMNS: header=True
                else:
                    if not begun or not header or c is None: raise ValueError('Missing SD6 header/config')
                    records.append([float(v) for v in f[1:]])
            elif f[0]=='SUMMARY': summary=int(f[2])
            elif f[0]=='CHIRP_END': ended=True; break
            elif f[0]=='ERROR': raise ValueError(line)
        except (ValueError,KeyError,IndexError) as e: errors.append(str(e))
    try:
        if not(begun and ended and header and c is not None) or summary!=len(records) or reason!=('COMPLETE' if status==0 else 'ABORTED'): raise ValueError('Incomplete SD6 capture')
        result=validate_records(records,c,status,flags)
    except (ValueError,TypeError,KeyError) as e:
        errors.append(str(e)); result=dict(valid=False,complete=False,rows=len(records),config=c,errors=[])
    result['errors']+=errors
    if errors: result.update(valid=False,complete=False)
    save_csv(csv_path,records,c,result); return result
