"""Strict FRIC001 binary -> time series, per-step CSV and metadata (stdlib only)."""
import argparse
import csv
import json
import math
import struct
from pathlib import Path

HEADER = struct.Struct('<8s3I')
ROW = struct.Struct('<7I6f7I')
FOOTER = struct.Struct('<8s4I')
COLUMNS = ('index actual_us phase step_index settling_status reason event target_torque_Nm '
           'position_deg speed_rad_s feedback_torque_estimate_Nm estimated_acceleration speed_std '
           'feedback_rx_us feedback_age_us feedback_sequence motor_error can_tx_accepted ISR_us period_us').split()
STATES = 'STARTUP SETTLING ACQUIRING STATIONARY COASTING BRAKING EXITING FINISHED'.split()
REASONS = ('OK NOT_SETTLED UNSTABLE USER_ABORT OVERSPEED FEEDBACK_LOST MOTOR_ERROR SD_ERROR '
           'RING_FULL TX_FAILED CAN_ERROR CONTROL_DEADLINE TORQUE_RANGE INVALID_FEEDBACK '
           'START_TIMEOUT STOP_FAILED RUN_LIMIT').split()
SUMMARY_COLUMNS = ('phase step_index target_torque_Nm mean_feedback_torque_estimate_Nm mean_speed_rad_s '
                   'speed_std_rad_s mean_acceleration_rad_s2 settling_time_s acquisition_time_s '
                   'valid_samples classification max_abs_speed_rad_s end_reason').split()


class Step:
    def __init__(self, r):
        self.first = r
        self.acquired = None
        self.last_seq = None
        self.n = 0
        self.sum_v = self.sum_vv = self.sum_t = self.sum_a = 0.0
        self.max_speed = 0.0

    def add(self, r):
        self.max_speed = max(self.max_speed, abs(r['speed_rad_s']))
        if r['settling_status'] not in (2, 3):
            return
        if self.acquired is None:
            self.acquired = r
        if r['feedback_sequence'] == self.last_seq:
            return
        self.last_seq = r['feedback_sequence']
        self.n += 1
        v = r['speed_rad_s']
        self.sum_v += v
        self.sum_vv += v*v
        self.sum_t += r['feedback_torque_estimate_Nm']
        self.sum_a += r['estimated_acceleration']

    def finish(self, r, reason):
        valid = reason == 'OK' and self.n > 0 and self.acquired is not None
        classification = ('STATIONARY' if self.acquired['settling_status'] == 3 else 'STEADY') if valid else 'NONSTATIONARY'
        n = self.n
        return dict(zip(SUMMARY_COLUMNS, [
            self.first['phase'], self.first['step_index'], self.first['target_torque_Nm'],
            self.sum_t/n if valid else '', self.sum_v/n if valid else '',
            math.sqrt(max(0, self.sum_vv/n-(self.sum_v/n)**2)) if valid else '',
            self.sum_a/n if valid else '',
            (self.acquired['actual_us']-self.first['actual_us'])/1e6 if self.acquired else '',
            (r['actual_us']-self.acquired['actual_us'])/1e6 if self.acquired else 0,
            n if valid else 0, classification, self.max_speed, reason]))


def convert(path):
    path = Path(path)
    summaries = []
    with path.open('rb') as f:
        raw = f.read(HEADER.size)
        if len(raw) != HEADER.size:
            raise ValueError('Truncated header')
        magic, row_bytes, period, config_bytes = HEADER.unpack(raw)
        if magic != b'FRIC001\0' or row_bytes != ROW.size or period != 500 or not 0 < config_bytes <= 4096:
            raise ValueError('Unsupported header')
        config = json.loads(f.read(config_bytes))
        size = path.stat().st_size
        if size < HEADER.size+config_bytes+FOOTER.size:
            raise ValueError('Truncated file')
        f.seek(-FOOTER.size, 2)
        magic, count, reason, stopped, ack = FOOTER.unpack(f.read(FOOTER.size))
        if magic != b'ENDFR001' or reason >= len(REASONS) or stopped not in (0, 1) or ack not in (0, 1):
            raise ValueError('Missing or invalid footer; do not treat as a completed run')
        if size != HEADER.size+config_bytes+count*ROW.size+FOOTER.size:
            raise ValueError('File length/row count mismatch')
        f.seek(HEADER.size+config_bytes)
        output = path.with_suffix('.csv')
        temporary = output.with_suffix('.csv.tmp')
        step = None
        previous_time = -1
        last = None
        try:
            with temporary.open('w', newline='', encoding='utf-8') as out:
                out.write('# FRIC001; feedback is the latest pre-command snapshot, not a synchronous response\n')
                out.write('# config = '+json.dumps(config, ensure_ascii=False)+'\n')
                writer = csv.DictWriter(out, ['nominal_us']+COLUMNS)
                writer.writeheader()
                for index in range(count):
                    r = dict(zip(COLUMNS, ROW.unpack(f.read(ROW.size))))
                    if r['index'] != index or r['actual_us'] <= previous_time or r['settling_status'] >= len(STATES) or r['reason'] >= len(REASONS):
                        raise ValueError(f'Invalid row {index}')
                    previous_time = r['actual_us']
                    r['nominal_us'] = index*period
                    writer.writerow(r)
                    if r['event'] & 1:
                        if step is not None:
                            raise ValueError('Step start without preceding end')
                        step = Step(r)
                    if step is not None:
                        step.add(r)
                        if r['event'] & 2:
                            summaries.append(step.finish(r, REASONS[r['reason']]))
                            step = None
                    last = r
                if step is not None:
                    summaries.append(step.finish(last, REASONS[reason] if reason else 'INCOMPLETE'))
            temporary.replace(output)
        except Exception:
            temporary.unlink(missing_ok=True)
            raise
    summary_path = path.with_suffix('.steps.csv')
    with summary_path.open('w', newline='', encoding='utf-8') as out:
        writer = csv.DictWriter(out, SUMMARY_COLUMNS)
        writer.writeheader()
        writer.writerows(summaries)
    result = dict(reason=REASONS[reason], rows=count, stop_confirmed=bool(stopped), exit_acknowledged=bool(ack),
                  complete=reason == 0 and bool(stopped) and (bool(config['NO_MOTOR_TEST']) or bool(ack)))
    metadata = dict(config=config, result=result, states=dict(enumerate(STATES)), reasons=dict(enumerate(REASONS)),
                    feedback_note='Internal torque estimate; never an external torque sensor. RX timestamp is callback time.',
                    summary_note='Unique feedback within accepted acquisition intervals only; invalidated intervals have zero valid samples.')
    path.with_suffix('.metadata.json').write_text(json.dumps(metadata, indent=2, ensure_ascii=False), encoding='utf-8')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    print(json.dumps(convert(parser.parse_args().binary), indent=2))
