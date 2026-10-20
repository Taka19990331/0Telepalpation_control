"""SD3 offline desk tests. Synthetic data, not a Teensy hardware timing test."""
import contextlib
import copy
import io
import json
import math
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

import chirp_sd3 as sd3
from convert_sd_chirp import convert
from capture_chirp import extract


def config():
    c = dict(Period_us=500, Sweep_us=40000000, Pre_us=2000000, Post_us=1000000,
             ExpectedRows=86001, RowBytes=64, MotorID=2, CAN_bitrate=1000000,
             NoMotor=1, PositionPolicy=0, FeedbackTimeout_us=20000,
             ProfileVerified=0, MotorTimeout_ms=0,
             F0_Hz=1, F1_Hz=100, A0_Nm=1, A1_Nm=2.5, AmplitudeExponent=1,
             KP_Nm_per_deg=0.00834, KD_Nm_per_rad_s=0.1, Hold_deg=5,
             TorqueLimit_Nm=10, PositionMin_rad=-12.5, PositionMax_rad=12.5,
             VelocityMax_rad_s=37.5, MITTorqueMax_Nm=32)
    # Header is float32: validate exactly the metadata saved by firmware.
    return sd3.decode_header(sd3.HEADER.pack(b'CHIRPSD3', *(c[k] for k in sd3.INT_KEYS+sd3.FLOAT_KEYS)))


def model_rows(c):
    rows = []
    for i in range(c['ExpectedRows']):
        us = i*c['Period_us']; t = us/1e6
        phase = sd3.phase_at(i, c)
        position = 5 + .5*math.sin(.7*t)
        speed = .35*math.cos(.7*t)*math.pi/180
        chirp = sd3.chirp(t-c['PreHold_s'], c) if phase == 1 else 0
        pd = 0 if phase == 3 else c['KP_Nm_per_deg']*(c['Hold_deg']-position)-c['KD_Nm_per_rad_s']*speed
        values = [i, us, phase, position, speed, 0, chirp, pd, chirp+pd, 0, 0, i+1, 0, 4, 0, 0]
        # Quantize all binary float fields as on Teensy.
        values = sd3.ROW.unpack(sd3.ROW.pack(*values))
        rows.append(sd3.csv_record(values, c['Period_us'], None if i == 0 else us-c['Period_us']))
    return rows


def write_bin(path, c, records, status=0, flags=16):
    path.write_bytes(sd3.HEADER.pack(b'CHIRPSD3', *(c[k] for k in sd3.INT_KEYS+sd3.FLOAT_KEYS)) +
                     b''.join(sd3.ROW.pack(r[0], r[2], r[4], *r[5:]) for r in records) +
                     sd3.FOOTER.pack(b'ENDSD003', len(records), status, flags))


def write_serial(path, c, records, status=0, flags=16):
    reason = 'COMPLETE' if status == 0 else 'ABORTED'
    lines = [f'CHIRP_BEGIN,VERSION,SD3,REASON,{reason}',
             'CONFIG,' + ','.join(f'{k},{v}' for k, v in c.items()),
             f'RESULT,Status,{status},StopFlags,{flags}', 'DATA,'+','.join(sd3.COLUMNS)]
    lines += ['DATA,'+','.join(str(v) for v in r) for r in records]
    lines += [f'SUMMARY,Count,{len(records)}', 'CHIRP_END']
    path.write_text('\n'.join(lines), encoding='ascii')


class SD3Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.c = config()
        cls.rows = model_rows(cls.c)

    def test_complete_43s_boundaries_and_pd_after_chirp(self):
        r = sd3.validate_records(self.rows, self.c, 0, 16)
        self.assertTrue(r['complete'])
        self.assertEqual(len(self.rows), 86001)
        self.assertEqual([self.rows[i][4] for i in (0,3999,4000,83999,84000,85999,86000)], [0,0,1,1,2,2,3])
        self.assertEqual(self.rows[-1][1], 43000000)
        self.assertEqual(sum(row[4] == 0 for row in self.rows), 4000)
        self.assertEqual(sum(row[4] == 1 for row in self.rows), 80000)
        self.assertEqual(sum(row[4] == 2 for row in self.rows), 2000)
        self.assertEqual(self.rows[84000][8], 0)
        self.assertNotEqual(self.rows[84000][10], 0) # post phase retains PD
        self.assertEqual(self.rows[-1][10], 0)
        self.assertEqual(sd3.HEADER.size+len(self.rows)*sd3.ROW.size+sd3.FOOTER.size, 5504196)

    def test_binary_and_serial_roundtrip(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as directory:
            path = Path(directory)/'run.BIN'
            write_bin(path, self.c, self.rows)
            with contextlib.redirect_stdout(io.StringIO()):
                result = convert(path)
            self.assertTrue(result['complete'])
            self.assertEqual(json.loads(path.with_suffix('.config.json').read_text())['TorqueLimit_Nm'],10)
            serial = Path(directory)/'serial.txt'
            write_serial(serial, self.c, self.rows)
            result = extract(serial, serial.with_suffix('.csv'))
            self.assertTrue(result['complete'], result['errors'])

    def test_aborted_prefix_all_causes_and_empty_start_failure(self):
        for status in range(1,14):
            result = sd3.validate_records(self.rows[:100], self.c, status, 16)
            self.assertTrue(result['valid'])
            self.assertFalse(result['complete'])
        c = dict(self.c, Hold_deg=float('nan'))
        self.assertFalse(sd3.validate_records([], c, 12, 16)['complete'])
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as directory:
            path = Path(directory)/'aborted.BIN'
            write_bin(path,self.c,self.rows[:100],status=1)
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(convert(path)['status_name'],'USER_ABORT')
            raw = Path(directory)/'abort.txt'
            write_serial(raw,self.c,self.rows[:100],status=1)
            result=extract(raw,raw.with_suffix('.csv'))
            self.assertTrue(result['valid']); self.assertFalse(result['complete'])

    def test_limit_aborts_with_zero_override(self):
        rows = copy.deepcopy(self.rows[:2])
        row = rows[-1]
        row[5] = -700 # full difference, not modulo 360
        row[6] = -37.5
        row[9] = self.c['KP_Nm_per_deg']*(self.c['Hold_deg']-row[5])-self.c['KD_Nm_per_rad_s']*row[6]
        self.assertGreater(row[9], 9)
        # Use a smaller independent limit to exercise threshold at this pre-hold sample.
        c = dict(self.c,TorqueLimit_Nm=9)
        row[4]=3; row[10]=0; row[11]=1
        self.assertEqual(sd3.validate_records(rows,c,9,16)['status_name'],'TORQUE_LIMIT')

    def test_reject_corruption_and_false_normal_completion(self):
        for column, bad in [(0,20),(1,2),(2,-1),(3,33),(4,2),(8,999),(9,1),(10,11),(12,20001),(13,0),(14,1),(16,256)]:
            rows = copy.deepcopy(self.rows[:3]); rows[1][column]=bad
            with self.subTest(column=column), self.assertRaises(ValueError):
                sd3.validate_records(rows,self.c,1,16)
        with self.assertRaises(ValueError):
            sd3.validate_records(self.rows[:3],self.c,0,16)
        with self.assertRaises(ValueError):
            sd3.validate_records(self.rows,self.c,0,0)
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as directory:
            path=Path(directory)/'bad.BIN'
            write_bin(path,self.c,self.rows[:3],status=1)
            path.write_bytes(path.read_bytes()[:-1])
            with self.assertRaises((ValueError,struct.error)):
                convert(path)

    def test_fault_snapshot_and_live_stop_completion(self):
        rows = copy.deepcopy(self.rows[:3])
        row = rows[-1]
        row[4] = 3; row[8] = row[9] = row[10] = 0
        row[12] = 25000; row[16] = 4
        result = sd3.validate_records(rows, self.c, 7, 16)
        self.assertTrue(result['valid'])
        self.assertEqual(result['status_name'], 'MOTOR_ERROR')
        # Zero may be submitted after the terminal sample while an old TX aborts.
        live = copy.deepcopy(self.rows)
        for r in live[:-1]:
            r[14] = 1
        c = dict(self.c, NoMotor=0)
        self.assertTrue(sd3.validate_records(live,c,0,15)['complete'])
        with self.assertRaises(ValueError):
            sd3.validate_records(live,c,0,7)

    def test_serial_corruption_is_reported(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as directory:
            path=Path(directory)/'corrupt.txt'
            write_serial(path,self.c,self.rows[:3],status=1)
            path.write_bytes(path.read_bytes().replace(b'DATA,1,',b'\xff\nDATA,1,',1))
            result=extract(path,path.with_suffix('.csv'))
            self.assertFalse(result['valid'])
            self.assertTrue(result['errors'])

    def test_cpp_derived_settings_reject_rounding(self):
        compiler = Path('C:/Users/User/AppData/Local/Arduino15/packages/teensy/tools/teensy-compile/15.2.1/arm/bin/arm-none-eabi-g++.exe')
        if not compiler.exists():
            self.skipTest('Teensy ARM compiler not installed at this path')
        source = Path(__file__).with_name('chirp_sd_test').joinpath('chirp_config.h').read_text(encoding='utf-8-sig')
        cases = [('', '', True), ('START_FREQUENCY_HZ = 1.0', 'START_FREQUENCY_HZ = 2.0', True),
                 ('SAMPLING_FREQUENCY_HZ = 2000.0', 'SAMPLING_FREQUENCY_HZ = 3000.0', False),
                 ('SWEEP_DURATION_S = 40.0', 'SWEEP_DURATION_S = 40.0001', False),
                 ('PRE_HOLD_DURATION_S = 2.0', 'PRE_HOLD_DURATION_S = 2.0001', False),
                 ('POST_HOLD_DURATION_S = 1.0', 'POST_HOLD_DURATION_S = 1.0001', False),
                 ('TORQUE_LIMIT_NM = 10.0f', 'TORQUE_LIMIT_NM = 33.0f', False)]
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as folder:
            path = Path(folder)/'config.cpp'
            for old, new, success in cases:
                path.write_text(source.replace(old,new) if old else source,encoding='utf-8')
                result=subprocess.run([str(compiler),'-std=gnu++17','-fsyntax-only',str(path)],capture_output=True,text=True)
                with self.subTest(setting=new):
                    self.assertEqual(result.returncode == 0,success,result.stderr)

    def test_nonunit_start_frequency_phase(self):
        c=dict(self.c,F0_Hz=2,F1_Hz=80,AmplitudeExponent=2)
        t=1.5; k=math.log(40)/40; f=2*math.exp(k*t)
        a=1+1.5*((f-2)/78)**2
        self.assertAlmostEqual(sd3.chirp(t,c),a*math.sin(2*math.pi*2*math.expm1(k*t)/k))


if __name__ == '__main__':
    unittest.main()
