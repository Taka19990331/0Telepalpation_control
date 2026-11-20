#pragma once
#include <stdint.h>
#include <math.h>
#include "control_config.h"
#include "fixed_notch.h"
namespace ControlAlgorithm {
struct Signals {
  float velocity=0, friction=0, input=0, state=0, external=0;
  float torqueR=0, torqueF=0, unsaturated=0, command=0;
  float angleBpf=0, angleRms=0, meanSpeed=0, gammaTheta=0, gammaSpeed=1, gamma=0;
  float notchInput=0, notchOutput=0;
  uint8_t notchMode=ControlConfig::INITIAL_NOTCH_MODE;
  bool saturated=false;
};
// Bilinear-transform Butterworth biquad, transposed direct-form II.
class Biquad {
 public:
  Biquad(float cutoff,float q,bool highpass) {
    const double w=6.2831853071795864769*cutoff*ControlConfig::CONTROL_PERIOD_US*1e-6;
    const double c=cos(w), s=sin(w), alpha=s/(2*q), a0=1+alpha;
    if(highpass) { b0=(1+c)/(2*a0); b1=-(1+c)/a0; b2=b0; }
    else { b0=(1-c)/(2*a0); b1=(1-c)/a0; b2=b0; }
    a1=-2*c/a0; a2=(1-alpha)/a0;
  }
  void reset() { z1=z2=0; initialized=false; }
  double update(double x) {
    if(!initialized) {
      const double dc=(b0+b1+b2)/(1+a1+a2);
      const double y=dc*x; z1=y-b0*x; z2=b2*x-a2*y; initialized=true;
    }
    const double y=b0*x+z1; z1=b1*x-a1*y+z2; z2=b2*x-a2*y; return y;
  }
  double magnitude(double w) const {
    const double zr=cos(w),zi=-sin(w),z2r=cos(2*w),z2i=-sin(2*w);
    const double nr=b0+b1*zr+b2*z2r,ni=b1*zi+b2*z2i;
    const double dr=1+a1*zr+a2*z2r,di=a1*zi+a2*z2i;
    return sqrt((nr*nr+ni*ni)/(dr*dr+di*di));
  }
  bool finiteState() const { return isfinite(z1)&&isfinite(z2); }
 private: double b0=0,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0; bool initialized=false;
};
class AngleDetector {
 public:
  AngleDetector(): hp1(ControlConfig::ANGLE_BPF_LOW_HZ,0.541196100146197,true), hp2(ControlConfig::ANGLE_BPF_LOW_HZ,1.306562964876377,true),
    lp1(ControlConfig::ANGLE_BPF_HIGH_HZ,0.541196100146197,false), lp2(ControlConfig::ANGLE_BPF_HIGH_HZ,1.306562964876377,false) {
    const double fc=sqrt(double(ControlConfig::ANGLE_BPF_LOW_HZ)*ControlConfig::ANGLE_BPF_HIGH_HZ);
    const double w=6.2831853071795864769*fc*ControlConfig::CONTROL_PERIOD_US*1e-6;
    const double h=hp1.magnitude(w)*hp2.magnitude(w)*lp1.magnitude(w)*lp2.magnitude(w);
    normalization=1.0/h;
  }
  void reset() { hp1.reset();hp2.reset();lp1.reset();lp2.reset();initialized=false;unwrapped=0;rms2=meanSpeed=0; }
  void update(float position,float velocity,uint32_t dtUs) {
    using namespace ControlConfig;
    if(!initialized) { unwrapped=position; initialized=true; }
    else {
      double d=double(position)-lastPosition, span=ANGLE_WRAP_MAX_RAD-ANGLE_WRAP_MIN_RAD;
      if(d>span*.5) d-=span; else if(d< -span*.5) d+=span;
      unwrapped+=d;
    }
    lastPosition=position;
    double y=hp1.update(unwrapped); y=hp2.update(y); y=lp1.update(y); y=lp2.update(y)*normalization;
    const double dt=double(CONTROL_PERIOD_US)*1e-6;
    const double ar=exp(-dt/ANGLE_RMS_TAU_S); rms2=ar*rms2+(1-ar)*y*y;
    const double av=exp(-6.2831853071795864769*MEAN_SPEED_LPF_HZ*double(dtUs)*1e-6);
    meanSpeed=av*meanSpeed+(1-av)*double(velocity);
    bpf=float(y); rms=float(sqrt(rms2)); speed=float(fabs(meanSpeed));
  }
  bool finiteState() const { return hp1.finiteState()&&hp2.finiteState()&&lp1.finiteState()&&lp2.finiteState()&&isfinite(unwrapped)&&isfinite(rms2)&&isfinite(meanSpeed)&&isfinite(normalization); }
  float bpf=0,rms=0,speed=0;
 private:
  Biquad hp1,hp2,lp1,lp2; bool initialized=false; double lastPosition=0,unwrapped=0,rms2=0,meanSpeed=0,normalization=1;
};
class Controller {
 public:
  Signals signals;
  float torqueBias=0;
  void reset() { mode=ControlConfig::INITIAL_NOTCH_MODE; signals=Signals{}; signals.notchMode=uint8_t(mode); initialized=false; numericFault=false; lastUs=0; notch.reset(); velocityNotch.reset(); detector.reset(); latestRawVelocity=0; appliedGamma=0; holdSeconds=0; }
  bool ready() const { return initialized && !numericFault; }
  bool faulted() const { return numericFault; }
  void zeroOutput() { signals.torqueR=signals.torqueF=signals.unsaturated=signals.command=0; signals.saturated=false; }
  void trip() { numericFault=true; zeroOutput(); }
  void setNotchMode(ControlConfig::NotchMode m) { mode=m; signals.notchMode=uint8_t(m); appliedGamma=0; holdSeconds=0; }
  ControlConfig::NotchMode notchMode() const { return mode; }
  bool setAngleThresholds(float startDeg,float fullDeg) {
    if(!isfinite(startDeg)||!isfinite(fullDeg)||startDeg<=0||fullDeg<=startDeg) return false;
    angleStartThresholdDeg=startDeg; angleFullThresholdDeg=fullDeg; return true;
  }
  float angleStartDeg() const { return angleStartThresholdDeg; }
  float angleFullDeg() const { return angleFullThresholdDeg; }
  static float friction(float w,ControlConfig::FrictionModel model) {
    if(!isfinite(w)) return NAN;
    const auto &p=w>0 ? ControlConfig::POSITIVE : ControlConfig::NEGATIVE;
    if(model==ControlConfig::FRICTION_STRIBECK) {
      if(w==0) return 0;
      const float r=w/p.vs;
      const float magnitude=p.tc+(p.ts-p.tc)*expf(-r*r);
      return (w>0 ? magnitude : -magnitude)+p.b*w;
    }
    return p.tc*tanhf(w/ControlConfig::OMEGA_0)+p.b*w;
  }
  bool finiteState() const {
    return notch.finiteState()&&velocityNotch.finiteState()&&detector.finiteState()&&
      isfinite(torqueBias)&&isfinite(latestRawVelocity)&&isfinite(appliedGamma)&&isfinite(holdSeconds)&&
      isfinite(signals.velocity)&&isfinite(signals.friction)&&isfinite(signals.input)&&isfinite(signals.state)&&isfinite(signals.external)&&
      isfinite(signals.torqueR)&&isfinite(signals.torqueF)&&isfinite(signals.unsaturated)&&isfinite(signals.command)&&
      isfinite(signals.angleBpf)&&isfinite(signals.angleRms)&&isfinite(signals.meanSpeed)&&isfinite(signals.gammaTheta)&&isfinite(signals.gammaSpeed)&&isfinite(signals.gamma)&&isfinite(signals.notchInput)&&isfinite(signals.notchOutput);
  }
  // Called exactly once per new CAN feedback frame; BPF is designed for 2 kHz sampling.
  void feedback(float position,float rawVelocity,float motorTorque,uint32_t rxUs) {
    using namespace ControlConfig;
    if(numericFault) return;
    if(!isfinite(position)||!isfinite(rawVelocity)||!isfinite(motorTorque)||!finiteState()) { trip(); return; }
    if(initialized && uint32_t(rxUs-lastUs)==0) return;
    const uint32_t dtUs=initialized?uint32_t(rxUs-lastUs):CONTROL_PERIOD_US;
    detector.update(position,rawVelocity,dtUs);
    if(!initialized) {
      signals.velocity=rawVelocity; signals.friction=friction(signals.velocity,RFOB_FRICTION_MODEL);
      signals.input=motorTorque-torqueBias-signals.friction+J*G*signals.velocity;
      signals.state=J*G*signals.velocity; initialized=true;
    } else {
      const float dt=float(dtUs)*US_TO_SECONDS;
      signals.velocity+=(1.0f-expf(-RADIANS_PER_REVOLUTION*VELOCITY_HZ*dt))*(rawVelocity-signals.velocity);
      signals.friction=friction(signals.velocity,RFOB_FRICTION_MODEL);
      signals.input=motorTorque-torqueBias-signals.friction+J*G*signals.velocity;
      signals.state+=(1.0f-expf(-G*dt))*(signals.input-signals.state);
    }
    lastUs=rxUs; latestRawVelocity=rawVelocity;
    signals.external=J*G*signals.velocity-signals.state;
    signals.angleBpf=detector.bpf; signals.angleRms=detector.rms; signals.meanSpeed=detector.speed;
    if(!finiteState()) trip();
  }
  // Called once per 500 us command tick. Both notch instances run continuously.
  // notch: diagnostics ONLY (legacy RFOB logs). velocityNotch: damping branch.
  float command(bool enabled,float Kr,float Kf) {
    using namespace ControlConfig;
    if(numericFault) { zeroOutput(); return 0; }
    if(!isfinite(Kr)||!isfinite(Kf)||!finiteState()) { trip(); return 0; }
    const float raw=signals.external, filtered=notch.update(raw);
    // Sample-and-hold the latest raw CAN velocity. Do not reuse the RFOB 1000 Hz LPF.
    // The notch includes NOTCH_DEPTH internally; do not multiply depth again.
    const float velocityNotched=velocityNotch.update(latestRawVelocity);
    const float oscillationVelocity=latestRawVelocity-velocityNotched;
    const double dt=CONTROL_PERIOD_US*1e-6;
    float gt=0,gv=1;
    if(mode==NOTCH_FIXED) { gt=gv=1; }
    else if(mode==NOTCH_ADAPTIVE) {
      const float ar=angleStartThresholdDeg*0.01745329251994329577f;
      const float full=angleFullThresholdDeg*0.01745329251994329577f;
      gt=fmaxf(0,fminf(1,(signals.angleRms-ar)/(full-ar)));
      if(ENABLE_SPEED_GATE) gv=fmaxf(0,fminf(1,(SPEED_GATE_ZERO_RAD_S-signals.meanSpeed)/(SPEED_GATE_ZERO_RAD_S-SPEED_GATE_FULL_RAD_S)));
    }
    float target=gt*gv;
    if(mode==NOTCH_ADAPTIVE) {
      if(target>appliedGamma) holdSeconds=NOTCH_HOLD_S;
      else if(holdSeconds>0) { holdSeconds=fmaxf(0,float(holdSeconds-dt)); target=fmaxf(target,appliedGamma); }
      const float tau=(target>appliedGamma)?NOTCH_ATTACK_S:NOTCH_RELEASE_S;
      appliedGamma+=float(1-exp(-dt/tau))*(target-appliedGamma);
    } else { appliedGamma=target; holdSeconds=0; }
    signals.gammaTheta=gt; signals.gammaSpeed=gv; signals.gamma=appliedGamma;
    signals.notchInput=raw; signals.notchOutput=filtered; signals.notchMode=uint8_t(mode);
    // NOTE: RFOB torque is no longer attenuated at any frequency.
    // OFF/FIXED/ADAPTIVE now select OFF/full/adaptive band-limited VELOCITY damping.
    const float dampingRequested=-DAMPING_GAIN*appliedGamma*oscillationVelocity;
    const float dampingTorque=fmaxf(-DAMPING_TORQUE_LIMIT_NM,
                              fminf(DAMPING_TORQUE_LIMIT_NM,dampingRequested));
    if(!isfinite(dampingTorque)||!finiteState()) { trip(); return 0; }
    if(!enabled||!ready()) { zeroOutput(); return 0; }
    signals.torqueR=Kr*raw;
    signals.torqueF=Kf*friction(signals.velocity,FF_FRICTION_MODEL);
    signals.unsaturated=signals.torqueR+signals.torqueF+dampingTorque;
    if(!finiteState()) { trip(); return 0; }
    signals.command=fmaxf(-TORQUE_LIMIT,fminf(TORQUE_LIMIT,signals.unsaturated));
    signals.saturated=signals.command!=signals.unsaturated;
    if(!isfinite(signals.command)) trip();
    return signals.command;
  }
 private:
  FixedNotch notch,velocityNotch; AngleDetector detector; bool initialized=false,numericFault=false;
  uint32_t lastUs=0; float latestRawVelocity=0;
  ControlConfig::NotchMode mode=ControlConfig::INITIAL_NOTCH_MODE;
  float appliedGamma=0,holdSeconds=0;
  float angleStartThresholdDeg=ControlConfig::ANGLE_NOTCH_START_DEG_RMS;
  float angleFullThresholdDeg=ControlConfig::ANGLE_NOTCH_FULL_DEG_RMS;
};
}
