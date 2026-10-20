#include "friction_core.h"
#include "motor_protocol.h"
#include "config_metadata.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <string.h>

Observation observation(uint32_t t,float speed=0,float position=0) {
  return {position,speed,0,t,t/PERIOD_US+1,0};
}
void assertions() {
  Regression r; r.reset(0);
  for(uint32_t t=0;t<=300000;t+=500) r.observe(observation(t,2+1.5f*t*1e-6f));
  assert(r.ready && fabsf(r.acceleration-1.5f)<.001f);
  auto n=r.n; r.observe(observation(300000,100)); assert(r.n==n); // duplicate RX excluded
  r.reset(300000); r.observe(observation(299500)); assert(r.n==0); // pre-step RX excluded
  Engine e; e.reset(0);
  for(uint32_t t=0;t<=500000;t+=500) e.tick(t,observation(t));
  assert(e.phase==1 && e.target==0);
  // Independent stationary delay: no stationary acquisition before 0.4 s.
  uint32_t start=e.stepUs;
  for(uint32_t t=500500;t<start+STATIONARY_CONFIRM_US;t+=500) { e.tick(t,observation(t)); assert(e.state!=STATIONARY); }
  for(uint32_t t=start+STATIONARY_CONFIRM_US;t<=start+STATIONARY_CONFIRM_US+1000;t+=500) e.tick(t,observation(t));
  assert(e.state==STATIONARY);
  e.tick(start+STATIONARY_CONFIRM_US+1500,observation(start+STATIONARY_CONFIRM_US+1500,.5f,.1f));
  assert(e.result==UNSTABLE && e.pendingNext);
  // Timeout with continuously accelerating data.
  e.reset(0); e.phase=1; e.startStep(0);
  for(uint32_t t=0;t<=MAX_SETTLE_US;t+=500) e.tick(t,observation(t,1+t*1e-6f,t*1e-6f));
  assert(e.result==NOT_SETTLED && (e.event&2));
  // A wrapped position cannot falsely confirm rest.
  e.reset(0); e.phase=1; e.startStep(0);
  for(uint32_t t=0;t<=450000;t+=500) e.tick(t,observation(t,0,t<300000?12.49f:-12.49f));
  assert(e.state==SETTLING);
  // Overspeed, startup refusal and bounded braking timeout.
  e.reset(0); e.tick(0,observation(0,20)); assert(e.finalStop && e.reason==OVERSPEED && e.phase==0);
  for(uint32_t t=500;t<=COAST_US+BRAKE_TIMEOUT_US;t+=500) {
    e.tick(t,observation(t,20,t*.00002f));
    assert(fabsf(e.target)<=BRAKE_TORQUE_NM+.00001f);
    assert(e.phase==0);
  }
  assert(e.state==EXITING && !e.stopConfirmed);
  e.reset(0); e.tick(START_TIMEOUT_US,{}); assert(e.state==EXITING && e.reason==START_TIMEOUT);
  e.reset(0); e.phase=1; e.startStep(0);
  e.tick(21000,observation(0)); assert(e.reason==FEEDBACK_LOST && e.state==EXITING);
  e.reset(0); auto f=observation(0); f.error=3; e.tick(0,f); assert(e.reason==MOTOR_ERROR);
  e.reset(0); f=observation(0); f.speed=NAN; e.tick(0,f); assert(e.reason==INVALID_FEEDBACK);
  e.reset(0); e.phase=1; e.startStep(0); e.target=1.1f; e.tick(0,observation(0)); assert(e.reason==TORQUE_RANGE);
  // Repeated ring-full faults must not reset braking slew to zero each tick.
  for(auto why: {SD_ERROR,RING_FULL,USER_ABORT}) {
    e.reset(0); e.phase=1; e.startStep(0); e.stop(why,0);
    for(uint32_t t=0;t<=COAST_US+1000000;t+=500) {
      e.stop(why,t); e.tick(t,observation(t,2,t*.000002f));
    }
    assert(e.state==BRAKING && e.target<-.1f && e.reason==why && e.finalStop);
    e.tick(COAST_US+1021000,observation(0)); assert(e.state==EXITING && !e.stopConfirmed);
  }
  e.reset(0); e.state=BRAKING; e.finalStop=true; e.stateUs=0;
  for(uint32_t t=0;t<=250000 && e.state!=EXITING;t+=500) e.tick(t,observation(t,10-t*.00001f));
  assert(e.state==EXITING && e.reason==STOP_FAILED); // excessive measured deceleration
  // Acquisition invalidation by a delayed acceleration (entire interval rejected).
  e.reset(0); e.phase=1; e.startStep(0);
  for(uint32_t t=0;t<=400000;t+=500) e.tick(t,observation(t,1,t*1e-6f));
  assert(e.state==ACQUIRING);
  for(uint32_t t=400500;t<=500000 && !e.pendingNext;t+=500) e.tick(t,observation(t,1+(t-400000)*.00001f,t*1e-6f));
  assert(e.result==UNSTABLE && e.pendingNext);
  // Protocol has exactly zero Kp/Kd, quantized torque stays inside experiment bound.
  for(float torque: {-1.f,0.f,1.f}) {
    auto m=torqueFrame(torque);
    assert((m.data[3]&15)==0 && m.data[4]==0 && m.data[5]==0);
    float decoded=decode((m.data[6]<<8)|m.data[7],-32,32,12);
    assert(fabsf(decoded)<=1 && fabsf(decoded-torque)<.016f);
  }
}

// Same Engine as firmware. No Arduino, serial port or CAN access.
int main(int argc,char **argv) {
  assertions();
  FILE *out=argc>1?fopen(argv[1],"wb"):nullptr;
  if(argc>1) assert(out);
  char json[4096]; size_t bytes=makeConfig(json,sizeof(json)); assert(bytes<sizeof(json));
  Header h={{'F','R','I','C','0','0','1',0},sizeof(Sample),PERIOD_US,uint32_t(bytes)};
  if(out) { fwrite(&h,sizeof(h),1,out); fwrite(json,bytes,1,out); }
  Engine e; e.reset(0); float speed=0,position=0;
  std::vector<float> commands; std::vector<unsigned> phases;
  uint32_t rows=0;
  for(uint32_t t=0;t<MAX_RUN_US;t+=PERIOD_US) {
    float acceleration=0;
    if(fabsf(speed)>.0001f || fabsf(e.target)>.18f)
      acceleration=(e.target-.12f*copysignf(1.f,speed==0?e.target:speed)-.2f*speed)/.01f;
    float next=speed+acceleration*.0005f;
    if(next*speed<0 && fabsf(e.target)<.18f) next=0;
    speed=next; position+=speed*.0005f;
    float wrapped=fmodf(position+12.5f,25.f); if(wrapped<0) wrapped+=25.f;
    auto f=observation(t,speed,wrapped-12.5f); f.torque=e.target;
    e.tick(t,f);
    if(e.event&1) {commands.push_back(e.target); phases.push_back(e.phase);}
    Sample s={}; s.index=rows; s.actualUs=t; s.phase=e.phase; s.step=e.step;
    s.state=e.state; s.reason=e.event&2?e.result:e.reason; s.event=e.event;
    s.target=e.target; s.positionDeg=f.position*57.2957795f; s.speed=speed; s.feedbackTorque=f.torque;
    s.acceleration=e.regression.acceleration; s.speedStd=e.regression.deviation;
    s.rxUs=t; s.sequence=f.sequence; s.periodUs=PERIOD_US;
    if(out) fwrite(&s,sizeof(s),1,out); ++rows;
    if(e.state==EXITING) break;
  }
  assert(e.reason==OK && e.stopConfirmed && e.state==EXITING);
  assert(commands.size()==202);
  assert(commands[0]==0 && commands[50]==1 && commands[100]==0 && commands[101]==0 && commands[151]==-1 && commands.back()==0);
  assert(phases[50]==1 && phases[51]==2 && phases[101]==3 && phases[152]==4);
  for(size_t i=1;i<commands.size();++i) assert(fabsf(commands[i]-commands[i-1])<=TORQUE_STEP_NM+.00001f);
  if(out) { Footer f={{'E','N','D','F','R','0','0','1'},rows,e.reason,e.stopConfirmed,0}; fwrite(&f,sizeof(f),1,out); fclose(out); }
  printf("PASS: regression, freshness, wraps, settling, faults, protocol, 202-step sweep; %u rows\n",rows);
}
