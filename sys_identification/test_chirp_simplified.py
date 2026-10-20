"""Non-driving tests for chirp_simplified_test1 and its isolated SI1 logs."""
from pathlib import Path
import copy
import csv
import math
import subprocess
import tempfile
import unittest
import chirp_si1 as si
from convert_sd_chirp import convert
from capture_chirp import extract
from test_chirp_sd3 import config as base_config

ROOT=Path(__file__).parent/'chirp_simplified_test1'
COMPILER=Path('C:/Users/User/AppData/Local/Arduino15/packages/teensy/tools/teensy-compile/15.2.1/arm/bin/arm-none-eabi-g++.exe')

def config(extra=0):
    c=base_config(); c.pop('ProfileVerified',None)
    c.update(ProfileDocumented=1,Duration_s=20,Sweep_us=20000000,Hold_deg=30,
             ExpectedRows=46005,RowBytes=64,Average_us=2000000,ReferenceSamples=4000,
             AverageRows=4000+extra,ReferenceMin_rad=math.radians(29),ReferenceMax_rad=math.radians(31),
             HoldPolicy=2,CANPort=2,MotorID=2,MotorProtocol=0)
    return c

def make_records(c):
    rows=[]; n=int(c['AverageRows']); previous=None
    for i in range(n+42001):
        phase=0 if i<n else 1 if i<n+40000 else 2 if i<n+42000 else 3
        position=29 if phase==0 and i%2==0 else 31 if phase==0 else 30.1
        speed=.2
        ch=si.chirp((i-n)*.0005,c) if phase==1 else 0
        pd=0 if phase in (0,3) else c['KP_Nm_per_deg']*(30-position)-c['KD_Nm_per_rad_s']*speed
        v=[i,i*500,phase,position,speed,0,ch,pd,ch+pd,0,0,i+1,0,2,0,2047]
        v=si.ROW.unpack(si.ROW.pack(*v)); rows.append(si.csv_record(v,500,previous)); previous=i*500
    return rows

class SimplifiedTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): cls.c=config(); cls.rows=make_records(cls.c)

    def test_cpp_fresh_average_reset_and_boundaries(self):
        if not COMPILER.exists(): self.skipTest('Teensy compiler unavailable')
        source=r'''
#include "reference_average.h"
#include "motor_protocol.h"
constexpr bool averageCheck() {
  ReferenceAverage a;
  if(a.valid()) return false;
  if(!a.observe(6,1) || !a.observe(9,1) || a.count!=1 || a.valid()) return false;
  if(!a.observe(-2,2) || a.count!=2 || a.meanRad()!=2 || a.minRad!=-2 || a.maxRad!=6 || !a.valid()) return false;
  a.reset();
  if(a.count || a.lastSequence || a.sumRad || a.valid()) return false;
  a.observe(-6,1); a.observe(-8,2);
  return a.meanRad()==-7;
}
static_assert(averageCheck(), "unique replies, multi-turn arithmetic mean, reset");
static_assert(PRE_STEPS==4000 && STEPS==40000 && POST_STEPS==2000 && EXPECTED_ROWS==46005);
static_assert(sizeof(Header)==144 && sizeof(Sample)==64);
'''
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as d:
            p=Path(d)/'check.cpp'; p.write_text(source)
            r=subprocess.run([str(COMPILER),'-std=gnu++17','-fsyntax-only','-I'+str(ROOT.resolve()),str(p)],capture_output=True,text=True)
            self.assertEqual(r.returncode,0,r.stderr)
        firmware=(ROOT/'chirp_simplified_test1.ino').read_text()
        self.assertIn('(row.phase == PRE_HOLD || row.phase == STOP_ROW) ? 0',firmware)
        self.assertNotIn('homing_control.h',firmware)

    def test_zero_torque_then_average_referenced_pd_and_20s(self):
        r=si.validate_records(self.rows,self.c,0,16)
        self.assertTrue(r['complete']); self.assertEqual(len(self.rows),46001)
        self.assertTrue(all(row[8]==row[9]==row[10]==0 for row in self.rows[:4000]))
        self.assertNotEqual(self.rows[4001][8],0)
        self.assertAlmostEqual(self.rows[4000][9],self.c['KP_Nm_per_deg']*(-.1)-self.c['KD_Nm_per_rad_s']*.2,places=6)
        bad=copy.deepcopy(self.rows); bad[0][9]=.01; bad[0][10]=.01
        with self.assertRaises(ValueError): si.validate_records(bad,self.c,0,16)
        with self.assertRaises(ValueError): si.validate_records(self.rows,dict(self.c,Hold_deg=0),0,16)
        with self.assertRaises(ValueError): si.validate_records(self.rows,dict(self.c,ReferenceSamples=1),0,16)
        jitter=config(1); self.assertTrue(si.validate_records(make_records(jitter),jitter,0,16)['complete'])

    def test_binary_serial_roundtrip_and_abort(self):
        c=self.c; rows=self.rows
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as d:
            p=Path(d)/'SI0000.BIN'
            fields=[c['ProfileDocumented'] if k=='ProfileVerified' else c[k] for k in si.INT_KEYS+si.FLOAT_KEYS]
            tail=[c[k] for k in ('Average_us','ReferenceSamples','AverageRows','ReferenceMin_rad','ReferenceMax_rad','HoldPolicy','CANPort','MotorProtocol')]
            p.write_bytes(si.HEADER.pack(b'CHIRPSI1',*fields,*tail)+b''.join(si.ROW.pack(r[0],r[2],r[4],*r[5:]) for r in rows)+si.FOOTER.pack(b'ENDSI001',len(rows),0,16))
            self.assertTrue(convert(p)['complete'])
            raw=Path(d)/'serial.txt'
            raw.write_text('CHIRP_BEGIN,VERSION,SI1,REASON,COMPLETE\nCONFIG,'+','.join(f'{k},{v}' for k,v in c.items())+'\nRESULT,Status,0,StopFlags,16\nDATA,'+','.join(si.COLUMNS)+'\n'+''.join('DATA,'+','.join(map(str,r))+'\n' for r in rows)+f'SUMMARY,Count,{len(rows)}\nCHIRP_END\n')
            self.assertTrue(extract(raw,raw.with_suffix('.csv'))['complete'])
            output=raw.with_suffix('.csv').read_text(encoding='utf-8')
            self.assertTrue(output.startswith('# FORMAT = CHIRPSI1_CSV_WITH_METADATA_V1\n'))
            self.assertIn('# SWEEP_DURATION_S (T) = 20.0\n',output)
            self.assertIn('# AMPLITUDE_FORMULA = A(t) = A0 + (A1-A0)*x(t)^a\n',output)
            self.assertIn('# CHIRP_START_RECORD_TIME_S (t_start) = 2.0\n',output)
            self.assertIn('# REFERENCE_POSITION_DEG (theta_ref) = 30.0\n',output)
            table=list(csv.reader(line for line in output.splitlines() if not line.startswith('#')))
            self.assertEqual(table[0],si.COLUMNS)
            self.assertEqual(len(table)-1,len(rows))
            p.write_bytes(p.read_bytes()[:-1])
            with self.assertRaises(ValueError): convert(p)
        partial=copy.deepcopy(rows[:1001]); partial[-1][4]=3
        c=dict(c,AverageRows=1000,ReferenceSamples=1000)
        self.assertEqual(si.validate_records(partial,c,1,16)['status_name'],'USER_ABORT')
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as d:
            path=Path(d)/'abort.csv'
            si.save_csv(path,partial,c,si.validate_records(partial,c,1,16))
            output=path.read_text(encoding='utf-8')
            self.assertIn('# CHIRP_START_RECORD_TIME_S (t_start) = unavailable\n',output)
            self.assertIn('# REFERENCE_VALID_FOR_PD = False\n',output)

if __name__=='__main__': unittest.main()
