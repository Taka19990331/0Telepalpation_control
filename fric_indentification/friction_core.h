#pragma once
#include "friction_config.h"

enum State : uint32_t { STARTUP, SETTLING, ACQUIRING, STATIONARY, COASTING, BRAKING, EXITING, FINISHED };
enum Reason : uint32_t { OK, NOT_SETTLED, UNSTABLE, USER_ABORT, OVERSPEED, FEEDBACK_LOST,
  MOTOR_ERROR, SD_ERROR, RING_FULL, TX_FAILED, CAN_ERROR, CONTROL_DEADLINE,
  TORQUE_RANGE, INVALID_FEEDBACK, START_TIMEOUT, STOP_FAILED, RUN_LIMIT };
struct Observation { float position, speed, torque; uint32_t rxUs, sequence, error; };
struct Sample {
  uint32_t index, actualUs, phase, step, state, reason, event;
  float target, positionDeg, speed, feedbackTorque, acceleration, speedStd;
  uint32_t rxUs, ageUs, sequence, motorError, txAccepted, executionUs, periodUs;
};
// event: 1 step start; 2 step end (reason is terminal result); 4 shutdown.
static_assert(sizeof(Sample)==80,"Update converter");
struct Header { char magic[8]; uint32_t rowBytes, periodUs, configBytes; };
struct Footer { char magic[8]; uint32_t rows, reason, stopConfirmed, exitAcknowledged; };
static_assert(sizeof(Header)==20 && sizeof(Footer)==24,"Update converter");

// O(1) rolling least squares on UNIQUE feedback and actual RX times.
// Times are relative to reset, bounded by step/stop duration (not micros uptime).
struct Regression {
  struct Point { double t; float v; } points[WINDOW_CAPACITY];
  unsigned begin=0, n=0; uint32_t origin=0, lastSeq=0;
  double st=0, sv=0, stt=0, stv=0, svv=0;
  float acceleration=NAN, deviation=NAN; bool ready=false;
  void reset(uint32_t now) { begin=n=0; origin=now; lastSeq=0; st=sv=stt=stv=svv=0; ready=false; acceleration=deviation=NAN; }
  void remove() { auto p=points[begin]; st-=p.t; sv-=p.v; stt-=p.t*p.t; stv-=p.t*p.v; svv-=double(p.v)*p.v; begin=(begin+1)%WINDOW_CAPACITY; --n; }
  void observe(const Observation &f) {
    if(f.sequence==lastSeq || int32_t(f.rxUs-origin)<0) return;
    lastSeq=f.sequence; double t=uint32_t(f.rxUs-origin)*1e-6;
    while(n && (t-points[begin].t>WINDOW_US*1e-6+1e-9 || n==WINDOW_CAPACITY)) remove();
    points[(begin+n)%WINDOW_CAPACITY]={t,f.speed}; ++n;
    st+=t; sv+=f.speed; stt+=t*t; stv+=t*f.speed; svv+=double(f.speed)*f.speed;
    double den=n*stt-st*st;
    ready=n>=3 && t-points[begin].t >= (WINDOW_US-FEEDBACK_TIMEOUT_US)*1e-6 && den>1e-12;
    acceleration=ready ? (n*stv-st*sv)/den : NAN;
    deviation=ready ? sqrt(fmax(0.0,svv/n-(sv/n)*(sv/n))) : NAN;
  }
  bool settled() const { return ready && fabsf(acceleration)<=ACCEL_THRESHOLD && deviation<=SPEED_STD_THRESHOLD; }
};

struct Engine {
  State state=STARTUP; uint32_t phase=0, step=0, level=0, reason=OK, result=OK, event=0;
  uint32_t stateUs=0, stepUs=0, candidateUs=0, stationaryUs=0, lastSeq=0;
  float target=0, positionMin=0, positionMax=0;
  bool candidate=false, stationaryCandidate=false, pendingNext=false, finalStop=false;
  bool stopConfirmed=false, started=false;
  Regression regression;
  void reset(uint32_t now) { *this=Engine(); stateUs=now; regression.reset(now); }
  void resetDetection(uint32_t now) { regression.reset(now); candidate=stationaryCandidate=false; lastSeq=0; }
  void startStep(uint32_t now) {
    state=SETTLING; stateUs=stepUs=now; result=OK; event|=1; resetDetection(now);
  }
  void stop(uint32_t why,uint32_t now,bool reliable=true) {
    if(state==FINISHED || state==EXITING) return;
    if(reason==OK) reason=why;
    if(phase>=1 && phase<=4 && state<=STATIONARY) { result=why; event|=2; }
    finalStop=true; pendingNext=false;
    if(!reliable) { target=0; state=EXITING; stateUs=now; event|=4; return; }
    if(state!=COASTING && state!=BRAKING) { target=0; state=COASTING; stateUs=now; resetDetection(now); }
  }
  bool stationary(const Observation &f,uint32_t now) {
    if(f.sequence==lastSeq) return stationaryCandidate && now-stationaryUs>=STATIONARY_CONFIRM_US;
    lastSeq=f.sequence;
    if(!stationaryCandidate || fabsf(f.speed)>STATIONARY_SPEED) {
      stationaryCandidate=fabsf(f.speed)<=STATIONARY_SPEED;
      stationaryUs=now; positionMin=positionMax=f.position;
    } else {
      positionMin=fminf(positionMin,f.position); positionMax=fmaxf(positionMax,f.position);
      // Raw angle discontinuities (including wraps) invalidate the stationary interval.
      // No assumption about the firmware's physical wrap period is needed.
      if(positionMax-positionMin>STATIONARY_POSITION_RANGE) {
        stationaryUs=now; positionMin=positionMax=f.position;
      }
    }
    return stationaryCandidate && now-stationaryUs>=STATIONARY_CONFIRM_US;
  }
  void next(uint32_t now) {
    pendingNext=false;
    const unsigned intervals=unsigned(ceilf(TORQUE_LIMIT_NM/TORQUE_STEP_NM));
    ++step; // global step index; endpoints clamped, including non-divisible increments
    if(phase==1) {
      if(level==intervals) { phase=2; level=1; target=fmaxf(0.f,TORQUE_LIMIT_NM-TORQUE_STEP_NM); }
      else { ++level; target=fminf(TORQUE_LIMIT_NM,level*TORQUE_STEP_NM); }
    } else if(phase==2) {
      if(target<=0) { phase=0; state=COASTING; stateUs=now; target=0; resetDetection(now); return; }
      ++level;
      target=level>=intervals?0:fmaxf(0.f,TORQUE_LIMIT_NM-level*TORQUE_STEP_NM);
    } else if(phase==3) {
      if(level==intervals) { phase=4; level=1; target=fminf(0.f,-TORQUE_LIMIT_NM+TORQUE_STEP_NM); }
      else target=fmaxf(-TORQUE_LIMIT_NM,(++level)*-TORQUE_STEP_NM);
    } else {
      if(target>=0) { stop(OK,now); return; }
      ++level;
      target=level>=intervals?0:fminf(0.f,-TORQUE_LIMIT_NM+level*TORQUE_STEP_NM);
    }
    startStep(now);
  }
  void tick(uint32_t now,const Observation &f) {
    event=0;
    if(state==FINISHED || state==EXITING) return;
    if(!f.sequence || now-f.rxUs>FEEDBACK_TIMEOUT_US) {
      if(state!=STARTUP || now-stateUs>=START_TIMEOUT_US) stop(state==STARTUP?START_TIMEOUT:FEEDBACK_LOST,now,false);
      return;
    }
    if(f.error) { stop(MOTOR_ERROR,now,false); return; }
    if(!isfinite(f.position)||!isfinite(f.speed)||!isfinite(f.torque)) { stop(INVALID_FEEDBACK,now,false); return; }
    if(state!=COASTING && state!=BRAKING && fabsf(f.speed)>=SPEED_LIMIT_RAD_S) stop(OVERSPEED,now);
    if(state==STARTUP) { // Require stationary initial condition before any sweep.
      state=COASTING; stateUs=now; resetDetection(now);
    }
    regression.observe(f);
    bool still=stationary(f,now);
    if(state==COASTING || state==BRAKING) {
      if(still) {
        target=0; stopConfirmed=true;
        if(finalStop) { state=EXITING; stateUs=now; event|=4; }
        else { phase=started?3:1; level=0; started=true; stopConfirmed=false; startStep(now); }
        return;
      }
      if(state==COASTING && now-stateUs>=COAST_US) {
        state=BRAKING; stateUs=now; regression.reset(now);
      }
      if(state==BRAKING) {
        if(now-stateUs>=BRAKE_TIMEOUT_US || (regression.ready && fabsf(regression.acceleration)>BRAKE_MAX_DECEL)) {
          if(reason==OK) reason=STOP_FAILED;
          target=0; state=EXITING; stateUs=now; event|=4; return;
        }
        float desired=fabsf(f.speed)<=STATIONARY_SPEED?0:-copysignf(BRAKE_TORQUE_NM,f.speed);
        // Never retain a braking command that now accelerates a reversed shaft.
        if(target*f.speed>0) target=0;
        float delta=BRAKE_SLEW_NM_S*PERIOD_US*1e-6f;
        target+=fmaxf(-delta,fminf(delta,desired-target));
      }
      return;
    }
    if(pendingNext) { next(now); return; }
    bool settled=regression.settled();
    if(state==SETTLING) {
      if(settled) { if(!candidate) {candidate=true; candidateUs=now;} } else candidate=false;
      if(now-stepUs>=MIN_SETTLE_US && still) { state=STATIONARY; stateUs=now; }
      else if(now-stepUs>=MIN_SETTLE_US && candidate && now-candidateUs>=CONFIRM_US &&
              fabsf(f.speed)>STATIONARY_SPEED) { state=ACQUIRING; stateUs=now; }
      else if(now-stepUs>=MAX_SETTLE_US) { result=NOT_SETTLED; event|=2; pendingNext=true; }
    } else {
      bool valid=state==STATIONARY?still:settled;
      if(!valid) { result=UNSTABLE; event|=2; pendingNext=true; }
      else if(now-stateUs>=(state==STATIONARY?STATIONARY_ACQUIRE_US:ACQUIRE_US)) { result=OK; event|=2; pendingNext=true; }
    }
    if(!isfinite(target)||fabsf(target)>TORQUE_LIMIT_NM) stop(TORQUE_RANGE,now);
  }
};
