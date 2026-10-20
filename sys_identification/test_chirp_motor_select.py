"""Motor selection, constexpr CAN packing/decoding, and SD5 format tests."""
from pathlib import Path
import contextlib
import io
import subprocess
import tempfile
import unittest
import chirp_sd3 as sd
from test_chirp_zero import make_run
from test_chirp_sd3 import write_serial
from convert_sd_chirp import convert
from capture_chirp import extract

class MotorTests(unittest.TestCase):
    def test_cpp_profiles_and_wire_frames(self):
        compiler=Path('C:/Users/User/AppData/Local/Arduino15/packages/teensy/tools/teensy-compile/15.2.1/arm/bin/arm-none-eabi-g++.exe')
        if not compiler.exists(): self.skipTest('Teensy compiler unavailable')
        root=Path(__file__).parent/'chirp_sd_test'
        config=(root/'chirp_config.h').read_text(encoding='utf-8-sig')
        protocol=(root/'motor_protocol.h').read_text(encoding='utf-8-sig')
        source=r'''
#include "motor_protocol.h"
static_assert(MOTOR_ID==CAN_PORT_NUMBER);
constexpr MotorFrame tx=packRawCommand(0x1234,0xabc,0xdef);
static_assert(tx.len==8 && tx.extended==IS_60);
static_assert(IS_60 ? tx.id==0x803 : tx.id==MOTOR_ID);
static_assert(IS_60 ? (tx.data[0]==0 && tx.data[1]==0 && tx.data[2]==0 && tx.data[3]==0x12 && tx.data[4]==0x34 && tx.data[5]==0xab && tx.data[6]==0xcd && tx.data[7]==0xef) :
(tx.data[0]==0x12 && tx.data[1]==0x34 && tx.data[2]==0xab && tx.data[3]==0xc0 && tx.data[4]==0 && tx.data[5]==0 && tx.data[6]==0x0d && tx.data[7]==0xef));
constexpr MotorFrame rx=IS_60 ? MotorFrame{0x2903,true,false,false,8,{0x03,0x84,0,0x54,0xff,0x9c,25,0}} :
MotorFrame{MOTOR_ID,false,false,false,8,{uint8_t(MOTOR_ID),0x80,0,0x80,0x0f,0xff,25,0}};
constexpr MotorFeedback fb=decodeMotorFeedback(rx);
static_assert(fb.valid && fb.error==0);
static_assert(IS_60 ? (fb.positionRad>1.57079f && fb.positionRad<1.57080f && fb.speedRadS>1.04719f && fb.speedRadS<1.04721f && fb.torqueEstimateNm>-.59941f && fb.torqueEstimateNm<-.59939f && fb.raw==65436) :
(fb.raw==4095 && fb.torqueEstimateNm==32 && fb.positionRad>0 && fb.positionRad<.001f));
constexpr MotorFrame badId={99,IS_60,false,false,8,{}};
constexpr MotorFrame badLen={rx.id,rx.extended,false,false,7,{}};
constexpr MotorFrame remote={rx.id,rx.extended,true,false,8,{}};
static_assert(!isTargetFeedback(badId) && !isTargetFeedback(badLen) && !isTargetFeedback(remote));
'''
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as folder:
            folder=Path(folder); (folder/'motor_protocol.h').write_text(protocol,encoding='utf-8')
            (folder/'check.cpp').write_text(source)
            for profile in ('80A','80B','60','invalid'):
                (folder/'chirp_config.h').write_text(config.replace('MOTOR_SELECT[] = "80B"',f'MOTOR_SELECT[] = "{profile}"'),encoding='utf-8')
                result=subprocess.run([str(compiler),'-std=gnu++17','-fsyntax-only',str(folder/'check.cpp')],capture_output=True,text=True)
                with self.subTest(profile=profile): self.assertEqual(result.returncode==0,profile!='invalid',result.stderr)

    def test_sd5_all_selections_and_ak60_stop(self):
        base,rows=make_run()
        live=[list(r) for r in rows]
        for r in live[:-1]: r[14]=1
        for port,protocol in ((1,0),(2,0),(3,1)):
            with self.subTest(port=port), tempfile.TemporaryDirectory(dir=Path(__file__).parent) as directory:
                c=dict(base,CANPort=port,MotorID=port,MotorProtocol=protocol,NoMotor=0)
                if protocol:
                    c.update(PositionMin_rad=-12.56,PositionMax_rad=12.56,VelocityMax_rad_s=60,MITTorqueMax_Nm=12)
                    live[0][7]=-.5994; live[0][17]=65436
                flags=3 if protocol else 15
                p=Path(directory)/'motor.BIN'
                header=sd.HEADER_V5.pack(b'CHIRPSD5',*(c[k] for k in sd.INT_KEYS+sd.FLOAT_KEYS),
                    c['ZeroingTimeout_us'],c['Settle_us'],c['HomingRows'],c['SettlePosition_deg'],c['SettleSpeed_rad_s'],1,port,protocol)
                p.write_bytes(header+b''.join(sd.ROW.pack(r[0],r[2],r[4],*r[5:]) for r in live)+sd.FOOTER.pack(b'ENDSD005',len(live),0,flags))
                with contextlib.redirect_stdout(io.StringIO()): result=convert(p)
                self.assertTrue(result['complete']); self.assertEqual(result['config']['CANPort'],port)
                self.assertEqual(result['config']['MotorProtocol'],protocol)
                raw=Path(directory)/'serial.txt'
                write_serial(raw,c,live,flags=flags)
                raw.write_text(raw.read_text().replace('VERSION,SD3','VERSION,SD5'))
                result=extract(raw,raw.with_suffix('.csv')); self.assertTrue(result['complete'],result['errors'])
                with self.assertRaises(ValueError): sd.validate_records(live,c,0,0)
        with self.assertRaises(ValueError): sd.validate_config(dict(base,CANPort=1,MotorID=2,MotorProtocol=0))

if __name__=='__main__': unittest.main()
