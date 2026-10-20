"""Non-driving SD6 and actual C++ cascade/gate tests (compile-time execution)."""
from pathlib import Path
import math
import subprocess
import tempfile
import struct
import unittest
import chirp_sd6 as sd
from convert_sd_chirp import convert
from capture_chirp import extract

ROOT=Path(__file__).parent/'chirp_sd_test'
COMPILER=Path('C:/Users/User/AppData/Local/Arduino15/packages/teensy/tools/teensy-compile/15.2.1/arm/bin/arm-none-eabi-g++.exe')
CPP=r'''
#include "homing_control.h"
#include "motor_protocol.h"
constexpr bool near(float a,float b,float e=1e-5f) { return a-b<e && b-a<e; }
constexpr bool direction(float position) {
  HomingControl h;
  for(int i=0;i<4000;i++) {
    float priorV=h.velocity, priorT=h.command;
    h.step(0,position,0,.0005f);
    if(fabsf(h.velocity)>HOMING_MAX_VELOCITY+1e-6f ||
       fabsf(h.velocity-priorV)>HOMING_MAX_ACCELERATION*.0005f+1e-7f ||
       fabsf(h.command)>1 || fabsf(h.command-priorT)>.000501f) return false;
  }
  return position*h.command<0 && position*h.velocity<0;
}
static_assert(direction(6.2831853f) && direction(-6.2831853f), "multi-turn direction/velocity/acceleration/torque/slew");
constexpr bool antiwindup() {
  HomingControl h;
  for(int i=0;i<4000;i++) h.step(0,6.0f,1.0f,.0005f);
  if(!h.saturated || h.integral!=0 || fabsf(h.command)>1) return false;
  h.reset();
  // Hold a stationary motor at far positive position for much longer than timeout.
  for(int i=0;i<40000;i++) h.step(0,6.0f,0,.01f);
  if(fabsf(h.integral)>1.01f || !near(h.command,-1,.002f)) return false;
  h.integral=-2; h.command=-1; // force saturated stored state to verify unwinding
  float before=h.integral;
  h.step(0,6.0f,-.15f,.01f); // opposing error must unwind even while rate-limited
  if(h.integral<=before) return false;
  h.reset();
  if(h.velocity || h.integral || h.command || h.raw || h.proportional || h.saturated || h.slewLimited) return false;
  return h.step(0,-6.0f,0,.0005f)>0;
}
static_assert(antiwindup(), "saturation/unwind/new-trial reset");
constexpr bool handoverCheck() {
  HomingControl h; h.command=.7f; h.integral=.65f;
  for(int i=0;i<=4000;i++) {
    float previous=h.command;
    h.handover(.7f,-.1f,float(i)/4000,.0005f);
    if(fabsf(h.command)>1 || fabsf(h.command-previous)>.000501f || h.integral!=.65f) return false;
  }
  return near(h.command,-.1f);
}
static_assert(!handoverReady(.9f,2) && !handoverReady(.2f,.3f) && handoverReady(.1f,.1f));
static_assert(handoverCheck(), "smooth bounded handover; no integration");
constexpr bool gates() {
  ContinuousSettle s;
  if(zeroingGate(s,false,100,0,0)!=GATE_WAIT) return false;
  if(zeroingGate(s,true,200,0,1)!=GATE_WAIT) return false;
  if(zeroingGate(s,false,900000,0,1800)!=GATE_WAIT) return false;
  if(zeroingGate(s,true,1000000,0,2000)!=GATE_WAIT) return false;
  if(zeroingGate(s,true,1999999,0,3999)!=GATE_WAIT) return false;
  if(zeroingGate(s,true,2000000,0,4000)!=GATE_ADVANCE) return false;
  s.reset();
  if(zeroingGate(s,false,ZEROING_STEPS*PERIOD_US,0,ZEROING_STEPS)!=GATE_TIMEOUT) return false;
  s.reset();
  if(preHoldGate(s,false,0,0,0)!=GATE_WAIT) return false;
  if(preHoldGate(s,false,PRE_WAIT_STEPS*PERIOD_US,0,PRE_WAIT_STEPS)!=GATE_TIMEOUT) return false;
  s.reset();
  if(preHoldGate(s,true,0,0,0)!=GATE_WAIT) return false;
  if(preHoldGate(s,true,SETTLE_STEPS*PERIOD_US,0,SETTLE_STEPS)!=GATE_WAIT) return false;
  return preHoldGate(s,true,PRE_STEPS*PERIOD_US,0,PRE_STEPS)==GATE_ADVANCE;
}
static_assert(gates(), "continuous settling, no early sweep, finite zero/prehold timeout");
constexpr bool wireLimit() {
  for(int i=-100;i<=100;i++) {
    const MotorFrame f=torqueFrame(float(i)/100,1);
    const uint32_t t=(uint32_t(f.data[6]&15)<<8)|f.data[7];
    if(fabsf(decode(t,-MIT_TORQUE_MAX_NM,MIT_TORQUE_MAX_NM,12))>1.000001f) return false;
    if(IS_60 ? (f.data[0] || f.data[1] || f.data[2]) : ((f.data[3]&15) || f.data[4] || f.data[5] || (f.data[6]&240))) return false;
  }
  return true;
}
static_assert(wireLimit(), "MIT internal Kp/Kd zero and inward-rounded 1 Nm wire bound");
static_assert(sizeof(Header)==188 && sizeof(Sample)==88 && EXPECTED_ROWS==476002);
'''

def config():
    c=dict(Period_us=100000,Hz=10,Duration_s=1,PreHold_s=2,PostHold_s=1,
      Sweep_us=1000000,Pre_us=2000000,Post_us=1000000,ExpectedRows=242,RowBytes=88,
      MotorID=2,CANPort=2,MotorProtocol=0,CAN_bitrate=1000000,NoMotor=1,PositionPolicy=0,
      FeedbackTimeout_us=20000,ProfileDocumented=1,MotorTimeout_ms=0,F0_Hz=1,F1_Hz=2,
      A0_Nm=1,A1_Nm=2.5,AmplitudeExponent=1,KP_Nm_per_deg=.00834,KD_Nm_per_rad_s=.1,
      Hold_deg=0,TorqueLimit_Nm=10,PositionMin_rad=-12.5,PositionMax_rad=12.5,
      VelocityMax_rad_s=37.5,MITTorqueMax_Nm=32,ZeroingTimeout_us=5000000,Settle_us=1000000,
      HomingRows=10,SettlePosition_deg=2,SettleSpeed_rad_s=.1,HoldPolicy=1,
      HomingMaxVelocity=math.radians(5),HomingMaxAcceleration=math.radians(5),HomingTorqueLimit=1,
      HomingPositionK=1,HomingVelocityKp=2,HomingVelocityKi=1,HomingTorqueSlew=1,
      Handover_us=2000000,PreHoldTimeout_us=15000000,TransitionRows=21,PreHoldRows=20)
    return c

def records(c):
    start=c['HomingRows']+c['TransitionRows']+c['PreHoldRows']; rows=[]
    for i in range(start+21):
        phase=4 if i<c['HomingRows'] else 5 if i<c['HomingRows']+c['TransitionRows'] else 0 if i<start else 1 if i<start+10 else 2 if i<start+20 else 3
        ch=sd.old.chirp((i-start)*.1,c) if phase==1 else 0
        rows.append([i,i*100000,i*100000,100000 if i else 0,phase,0,0,0,ch,0,ch,0,0,i+1,0,1,0,0,0,0,0,0,ch,0])
    return rows

class HomingTests(unittest.TestCase):
    def test_actual_cpp_controller(self):
        if not COMPILER.exists(): self.skipTest('Teensy compiler unavailable')
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as folder:
            p=Path(folder)/'test.cpp'; p.write_text(CPP)
            folder=Path(folder)
            for name in ('chirp_config.h','homing_control.h','motor_protocol.h'):
                (folder/name).write_text((ROOT/name).read_text(encoding='utf-8'))
            original=(folder/'chirp_config.h').read_text()
            for profile in ('80A','80B','60'):
                (folder/'chirp_config.h').write_text(original.replace('MOTOR_SELECT[] = "80B"',f'MOTOR_SELECT[] = "{profile}"'))
                r=subprocess.run([str(COMPILER),'-std=gnu++17','-fconstexpr-ops-limit=100000000','-fsyntax-only','-I'+str(folder),str(p)],capture_output=True,text=True)
                self.assertEqual(r.returncode,0,r.stderr)

    def test_sd6_binary_serial_and_rejection(self):
        c=config(); rows=records(c)
        self.assertTrue(sd.validate_records(rows,c,0,16)['complete'])
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as folder:
            p=Path(folder)/'test.BIN'
            base=[c['ProfileDocumented'] if k=='ProfileVerified' else c[k] for k in sd.old.INT_KEYS+sd.old.FLOAT_KEYS]
            tail=[c[k] for k in ['ZeroingTimeout_us','Settle_us','HomingRows','SettlePosition_deg','SettleSpeed_rad_s','HoldPolicy','CANPort','MotorProtocol']+sd.HOME_KEYS+sd.TAIL_KEYS]
            header=sd.HEADER.pack(b'CHIRPSD6',*base,*tail)
            p.write_bytes(header+b''.join(sd.ROW.pack(r[0],r[2],r[4],*r[5:]) for r in rows)+sd.FOOTER.pack(b'ENDSD006',len(rows),0,16))
            self.assertTrue(convert(p)['complete'])
            raw=Path(folder)/'serial.txt'
            raw.write_text('CHIRP_BEGIN,VERSION,SD6,REASON,COMPLETE\nCONFIG,'+','.join(f'{k},{v}' for k,v in c.items())+'\nRESULT,Status,0,StopFlags,16\nDATA,'+','.join(sd.COLUMNS)+'\n'+''.join('DATA,'+','.join(map(str,r))+'\n' for r in rows)+f'SUMMARY,Count,{len(rows)}\nCHIRP_END\n')
            self.assertTrue(extract(raw,raw.with_suffix('.csv'))['complete'])
            data=p.read_bytes(); p.write_bytes(data[:-1])
            with self.assertRaises(ValueError): convert(p)
            p.write_bytes(b'CHIRPSD5'+data[8:])
            with self.assertRaises((ValueError,struct.error)): convert(p)
        for column,value in ((4,1),(10,1.01),(19,.2),(20,3)):
            bad=[r[:] for r in rows]; bad[0][column]=value
            with self.subTest(column=column),self.assertRaises(ValueError): sd.validate_records(bad,c,0,16)
        bad=[r[:] for r in rows]; bad[45][5]=3
        with self.assertRaises(ValueError): sd.validate_records(bad,c,0,16)

    def test_timeout_logs(self):
        for status,phase in ((14,4),(15,0),(16,5)):
            c=config(); rows=records(c)
            end=next(i for i,r in enumerate(rows) if r[4]==phase)+3
            rows=rows[:end]; counts=[sum(r[4]==v for r in rows) for v in (4,5,0)]
            c.update(zip(('HomingRows','TransitionRows','PreHoldRows'),counts))
            r=rows[-1][:]; r[0]+=1; r[1]+=100000; r[2]+=100000; r[3]=100000; r[4]=3; r[13]+=1
            rows.append(r)
            self.assertEqual(sd.validate_records(rows,c,status,16)['status_name'],sd.STATUS[status])

if __name__=='__main__': unittest.main()
