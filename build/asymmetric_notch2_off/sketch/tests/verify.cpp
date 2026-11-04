#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch2\\tests\\verify.cpp"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../control_algorithm.h"
int main() {
  using namespace ControlAlgorithm;
  using namespace ControlConfig;
  static_assert(INITIAL_NOTCH_MODE==NOTCH_ADAPTIVE,"startup notch mode must be adaptive");
  FixedNotch n;
  for(int i=0;i<10000;++i) assert(fabs(n.update(2)-2)<1e-6);
  Controller off,fixed,adaptive;
  assert(adaptive.notchMode()==NOTCH_ADAPTIVE && adaptive.signals.notchMode==2);
  adaptive.setNotchMode(NOTCH_FIXED);
  adaptive.reset();
  assert(adaptive.notchMode()==NOTCH_ADAPTIVE && adaptive.signals.notchMode==2);
  assert(adaptive.setAngleThresholds(.8f,1.0f));
  assert(adaptive.angleStartDeg()==.8f && adaptive.angleFullDeg()==1.0f);
  adaptive.signals.angleRms=.9f*0.01745329251994329577f;
  adaptive.command(true,.75f,0);
  assert(fabsf(adaptive.signals.gammaTheta-.5f)<1e-5f);
  adaptive.signals.angleRms=1.1f*0.01745329251994329577f;
  for(unsigned i=0;i<12000;++i) adaptive.command(true,.75f,0);
  assert(adaptive.signals.gammaTheta==1.0f && adaptive.signals.gamma>0.99f);
  adaptive.signals.angleRms=.79f*0.01745329251994329577f;
  for(unsigned i=0;i<12000;++i) adaptive.command(true,.75f,0);
  assert(adaptive.signals.gammaTheta==0.0f && adaptive.signals.gamma<1e-4f);
  assert(adaptive.setAngleThresholds(ANGLE_NOTCH_START_DEG_RMS,ANGLE_NOTCH_FULL_DEG_RMS));
  assert(adaptive.setAngleThresholds(.20f,.30f));
  assert(!adaptive.setAngleThresholds(.31f,.30f));
  assert(adaptive.angleStartDeg()==.20f && adaptive.angleFullDeg()==.30f);
  adaptive.setAngleThresholds(ANGLE_NOTCH_START_DEG_RMS,ANGLE_NOTCH_FULL_DEG_RMS);
  off.setNotchMode(NOTCH_OFF); fixed.setNotchMode(NOTCH_FIXED); adaptive.setNotchMode(NOTCH_ADAPTIVE);
  for(unsigned i=0;i<16000;++i) {
    const uint32_t us=i*CONTROL_PERIOD_US;
    const float theta=.01f*sinf(6.28318530718f*12.0f*float(us)*1e-6f);
    off.feedback(theta,0,0,us); fixed.feedback(theta,0,0,us); adaptive.feedback(theta,0,0,us);
    off.command(true,.75f,0); fixed.command(true,.75f,0); adaptive.command(true,.75f,0);
  }
  assert(off.finiteState()&&fixed.finiteState()&&adaptive.finiteState());
  assert(adaptive.signals.angleRms>0.0065f && adaptive.signals.angleRms<0.0078f);
  assert(adaptive.signals.gammaTheta==1.0f && adaptive.signals.gamma>0.95f);
  assert(off.signals.gamma==0.0f && fixed.signals.gamma==1.0f);
  // A repeated boundary crossing must unwrap as a small continuous increment.
  Controller wrap; wrap.setNotchMode(NOTCH_ADAPTIVE);
  wrap.feedback(12.49f,0,0,500); wrap.feedback(-12.49f,0,0,1000);
  assert(wrap.finiteState());
  puts("PASS: adaptive startup/reset, custom thresholds, adaptive decay, OFF/FIXED modes, 12 Hz angle RMS and adaptive gain, wrap crossing, finite states");
}
