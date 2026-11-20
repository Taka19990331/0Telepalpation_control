#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "control_algorithm.h"
#define ControlAlgorithm Test1Algorithm
#define ControlConfig Test1Config
#include "../../trans_ctrl_test1/control_algorithm.h"
#undef ControlConfig
#undef ControlAlgorithm
using PositionDirection::Estimator;
using PositionDirection::UNKNOWN;
using PositionDirection::POSITIVE;
using PositionDirection::NEGATIVE;
using ControlAlgorithm::Controller;
static const int signs[]={1,-1};
static const PositionDirection::Direction directions[]={UNKNOWN,POSITIVE,NEGATIVE};
static const float boundaries[]={0.8f,1.2f};
static constexpr float lsb=25.0f/65535.0f;
static Estimator estimator;
static Controller revised;
static Test1Algorithm::Controller original;
static float wrap(float p) {
  while(p>12.5f) p-=25.0f;
  while(p<-12.5f) p+=25.0f;
  return p;
}
static void directionTests() {
  // Stationary noise with maximum peak-to-peak width 2 LSB (measured condition).
  for(unsigned i=0;i<10000;++i) {
    estimator.update(float(int(i%3)-1)*lsb,i*500);
    assert(estimator.direction()==UNKNOWN);
  }
  estimator.reset();
  estimator.update(0,0); estimator.update(3*lsb,50000);
  assert(estimator.direction()==UNKNOWN); // Strictly greater than 3 LSB.
  estimator.reset();
  for(unsigned i=0;i<=100;++i) {
    estimator.update(i*lsb/20,i*500);
    assert(estimator.direction()==(i<100 ? UNKNOWN : POSITIVE));
  }
  // Reversal: old-window direction is allowed to persist until displacement reverses.
  for(unsigned i=1;i<=200;++i) estimator.update(5*lsb-i*lsb/20,50000+i*500);
  assert(estimator.direction()==NEGATIVE);
  for(unsigned i=201;i<=400;++i) estimator.update(-5*lsb,50000+i*500);
  assert(estimator.direction()==UNKNOWN); // No latched direction at rest.
  for(int sign : signs) {
    estimator.reset();
    for(unsigned i=0;i<=200;++i)
      estimator.update(wrap(sign*(12.499f+i*lsb/20)),i*500);
    assert(estimator.direction()==(sign>0 ? POSITIVE : NEGATIVE));
  }
  // Endpoint switching is zero wrapped movement, not a 25-radian step.
  estimator.reset(); estimator.update(12.5f,0); estimator.update(-12.5f,50000);
  assert(estimator.direction()==UNKNOWN);
  // Irregular CAN timestamps, duplicates, and uint32 microsecond rollover.
  estimator.reset(); const uint32_t start=0xffffa000U;
  estimator.update(0,start); estimator.update(10*lsb,start); // Same timestamp ignored.
  estimator.update(2*lsb,start+49999); assert(estimator.direction()==UNKNOWN);
  estimator.update(5*lsb,start+50731); assert(estimator.direction()==POSITIVE);
  estimator.reset(); estimator.update(0,0);
  // Dense received-frame history exercises ring rotation at actual CAN rates.
  for(unsigned i=1;i<100000;++i) {
    estimator.update(wrap(i*0.00001f),i*100);
    assert(estimator.direction()==(i<500 ? UNKNOWN : POSITIVE));
  }
}
static void blendTests() {
  for(auto direction : directions) {
    for(int sign : signs) {
      for(unsigned i=0;i<=2000;++i) {
        const float speed=i*0.001f, velocity=sign*speed;
        const float old=Controller::friction(velocity);
        const float pos=direction==UNKNOWN ? 0 : Controller::friction(direction==POSITIVE ? speed : -speed);
        const float actual=Controller::feedforwardFriction(velocity,direction);
        if(speed>=1.2f) assert(actual==old);
        else if(speed<=0.8f) assert(actual==pos);
        else {
          const float alpha=(speed-0.8f)/(1.2f-0.8f);
          assert(actual==(1-alpha)*pos+alpha*old);
        }
      }
      for(float boundary : boundaries) {
        const float below=Controller::feedforwardFriction(sign*(boundary-0.000001f),direction);
        const float above=Controller::feedforwardFriction(sign*(boundary+0.000001f),direction);
        assert(fabsf(above-below)<0.00001f);
      }
    }
  }
  // Explicitly quantify the unsmoothed low-speed state-change steps.
  const float positive=Controller::feedforwardFriction(0.01f,POSITIVE);
  const float negative=Controller::feedforwardFriction(0.01f,NEGATIVE);
  assert(Controller::feedforwardFriction(0.01f,UNKNOWN)==0);
  printf("Direction-state steps at |v|=0.01: unknown->positive %.9g Nm, positive->negative %.9g Nm (before Kf)\n",positive,negative-positive);
}
static void differentialTests() {
  uint32_t us=0xffff0000U; float position=0;
  unsigned highSpeed=0, suppressed=0;
  for(unsigned i=0;i<120000;++i) {
    const float raw=i<20000 ? (i%2 ? 0.069f : -0.069f) : 4.0f*sinf(i*0.001f);
    const float torque=0.3f*cosf(i*0.017f);
    us+=i%17==0 ? 0 : (i%7==0 ? 913 : 500);
    position=wrap(position+raw*0.0005f);
    revised.feedback(raw,torque,us,position);
    original.feedback(raw,torque,us);
    const auto &a=revised.signals; const auto &b=original.signals;
    assert(a.velocity==b.velocity && a.friction==b.friction && a.input==b.input);
    assert(a.state==b.state && a.external==b.external);
    const bool on=i%4000>1000;
    const float kr=i%2 ? 0.5f : -0.2f, kf=0.6f;
    const float command=revised.command(on,kr,kf), oldCommand=original.command(on,kr,kf);
    assert(a.torqueR==b.torqueR);
    if(fabsf(a.velocity)>=1.2f) {
      assert(a.torqueF==b.torqueF && command==oldCommand); ++highSpeed;
    }
    if(i<20000 && on) { assert(a.torqueF==0); ++suppressed; }
    if(i==10000) { revised.reset(); original.reset(); }
  }
  assert(highSpeed>50000 && suppressed>1000);
  revised.feedback(0,0,++us,NAN); assert(revised.faulted());
  assert(revised.command(true,1,1)==0);
  revised.reset(); revised.feedback(NAN,0,++us,0); assert(revised.faulted());
  revised.reset(); revised.feedback(0.069f,0,++us,0);
  revised.command(true,0,1); assert(revised.signals.torqueF==0);
  // Production wiring: position selects the negative model despite positive CAN speed.
  revised.reset();
  for(unsigned i=0;i<=100;++i) revised.feedback(0.01f,0,i*500,-i*lsb/20);
  revised.command(true,0,1);
  assert(revised.signals.torqueF==Controller::friction(-0.01f));
  // Control ticks without fresh feedback do not advance the position history.
  revised.reset(); revised.feedback(0.01f,0,0,0);
  for(unsigned i=0;i<1000;++i) { revised.command(true,0,1); assert(revised.signals.torqueF==0); }
  revised.feedback(0.01f,0,1000,10*lsb);
  revised.command(true,0,1); assert(revised.signals.torqueF==0);
  printf("PASS: 120000 production-interface feedback samples; exact LPF/RFOB equality; %u high-speed FF/command matches; %u stationary FF suppressions\n",highSpeed,suppressed);
}
int main() { directionTests(); blendTests(); differentialTests(); }
