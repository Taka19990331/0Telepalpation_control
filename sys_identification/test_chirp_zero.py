"""Existing-zero approach / SD4 desk verification; no motor or board attached."""
import contextlib
import io
import math
from pathlib import Path
import tempfile
import unittest
import chirp_sd3 as sd
from test_chirp_sd3 import config, write_serial
from convert_sd_chirp import convert
from capture_chirp import extract


def make_run():
    c=config()
    c.update(Hold_deg=0,ZeroingTimeout_us=30000000,Settle_us=1000000,
             HomingRows=0,SettlePosition_deg=2,SettleSpeed_rad_s=.1,HoldPolicy=1)
    settled=None
    for i in range(60000):
        t=i*.0005
        p=5*math.exp(-2*t)+.05*math.sin(.7*t)
        v=(-10*math.exp(-2*t)+.035*math.cos(.7*t))*math.pi/180
        if abs(p)<=2 and abs(v)<=.1:
            if settled is None: settled=i*500
        else: settled=None
        if settled is not None and i*500-settled>=1000000:
            c['HomingRows']=i
            break
    c['ExpectedRows']=c['HomingRows']+86001
    rows=[]
    for i in range(c['ExpectedRows']):
        t=i*.0005; phase=sd.phase_at(i,c)
        p=5*math.exp(-2*t)+.05*math.sin(.7*t)
        v=(-10*math.exp(-2*t)+.035*math.cos(.7*t))*math.pi/180
        chirp=sd.chirp((i-c['HomingRows'])*.0005-2,c) if phase==1 else 0
        pd=0 if phase==3 else -c['KP_Nm_per_deg']*p-c['KD_Nm_per_rad_s']*v
        val=sd.ROW.unpack(sd.ROW.pack(i,i*500,phase,p,v,0,chirp,pd,chirp+pd,0,0,i+1,0,4,0,0))
        rows.append(sd.csv_record(val,500,None if i==0 else (i-1)*500))
    return c,rows


class ZeroTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): cls.c,cls.rows=make_run()

    def test_settle_then_43_second_experiment(self):
        c,r=self.c,self.rows; h=c['HomingRows']
        self.assertGreater(h,2000)
        self.assertEqual(r[h-1][4],4)
        self.assertEqual([r[i][4] for i in [h,h+4000,h+84000,h+86000]],[0,1,2,3])
        self.assertTrue(sd.validate_records(r,c,0,16)['complete'])

    def test_binary_and_serial_sd4(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as d:
            p=Path(d)/'zero.BIN'; c=self.c
            header=sd.HEADER_V4.pack(b'CHIRPSD4',*(c[k] for k in sd.INT_KEYS+sd.FLOAT_KEYS),
                c['ZeroingTimeout_us'],c['Settle_us'],c['HomingRows'],c['SettlePosition_deg'],c['SettleSpeed_rad_s'],1)
            p.write_bytes(header+b''.join(sd.ROW.pack(r[0],r[2],r[4],*r[5:]) for r in self.rows)+sd.FOOTER.pack(b'ENDSD004',len(self.rows),0,16))
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertTrue(convert(p)['complete'])
            raw=Path(d)/'serial_zero.txt'; serial_config=dict(c); serial_config['ProfileDocumented']=serial_config.pop('ProfileVerified')
            write_serial(raw,serial_config,self.rows)
            raw.write_text(raw.read_text().replace('VERSION,SD3','VERSION,SD4'))
            result=extract(raw,raw.with_suffix('.csv'))
            self.assertTrue(result['complete'],result['errors'])

    def test_unsettled_rejected_and_homing_abort_preserved(self):
        c=dict(self.c,Settle_us=3000000)
        with self.assertRaises(ValueError): sd.validate_records(self.rows,c,0,16)
        rows=self.rows[:100]
        c=dict(self.c,HomingRows=100,ExpectedRows=86101)
        self.assertEqual(sd.validate_records(rows,c,14,16)['status_name'],'ZEROING_TIMEOUT')

if __name__=='__main__': unittest.main()

