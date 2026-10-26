#pragma once
#include "friction_config.h"

enum State : uint32_t {
  STARTUP=0, SETTLING=1, ACQUIRING=2, STATIONARY=3, COASTING=4,
  BRAKING_LEGACY=5, EXITING=6, FINISHED=7
};
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

// Fixed-memory O(1) rolling stop check. Each controller tick is one 2 kHz sample.
// Unwrap the motor's 25 rad position range before tracking extrema.
struct StopWindow {
  float positions[STOP_WINDOW_SAMPLES], speeds[STOP_WINDOW_SAMPLES];
  uint32_t minQueue[STOP_WINDOW_SAMPLES], maxQueue[STOP_WINDOW_SAMPLES];
  uint32_t seen=0, minHead=0, minTail=0, maxHead=0, maxTail=0;
  float lastRaw=0, unwrapped=0;
  double speedSum=0;
  void reset() { seen=minHead=minTail=maxHead=maxTail=0; lastRaw=unwrapped=0; speedSum=0; }
  bool observe(float rawPosition,float speed,float &range,float &meanSpeed) {
    const uint32_t index=seen, slot=index%STOP_WINDOW_SAMPLES;
    const uint32_t first=index+1>=STOP_WINDOW_SAMPLES?index+1-STOP_WINDOW_SAMPLES:0;
    while(minHead<minTail && minQueue[minHead%STOP_WINDOW_SAMPLES]<first) ++minHead;
    while(maxHead<maxTail && maxQueue[maxHead%STOP_WINDOW_SAMPLES]<first) ++maxHead;
    if(index>=STOP_WINDOW_SAMPLES) speedSum-=speeds[slot];
    if(!index) unwrapped=rawPosition;
    else {
      float delta=rawPosition-lastRaw;
      if(delta>12.5f) delta-=25.f;
      else if(delta< -12.5f) delta+=25.f;
      unwrapped+=delta;
    }
    lastRaw=rawPosition;
    positions[slot]=unwrapped; speeds[slot]=speed; speedSum+=speed;
    while(minTail>minHead && unwrapped<=positions[minQueue[(minTail-1)%STOP_WINDOW_SAMPLES]%STOP_WINDOW_SAMPLES]) --minTail;
    minQueue[minTail%STOP_WINDOW_SAMPLES]=index; ++minTail;
    while(maxTail>maxHead && unwrapped>=positions[maxQueue[(maxTail-1)%STOP_WINDOW_SAMPLES]%STOP_WINDOW_SAMPLES]) --maxTail;
    maxQueue[maxTail%STOP_WINDOW_SAMPLES]=index; ++maxTail;
    ++seen;
    if(seen<STOP_WINDOW_SAMPLES) return false;
    range=positions[maxQueue[maxHead%STOP_WINDOW_SAMPLES]%STOP_WINDOW_SAMPLES]-
          positions[minQueue[minHead%STOP_WINDOW_SAMPLES]%STOP_WINDOW_SAMPLES];
    meanSpeed=float(speedSum/STOP_WINDOW_SAMPLES);
    return true;
  }
};

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
  bool candidate=false, stationaryCandidate=false, pendingNext=false, finalStop=false, overspeedStop=false;
  bool overspeedReversePending=false, overspeedReverseUsed=false;
  bool stopConfirmed=false, started=false;
  Regression regression;
  StopWindow stopWindow;
  void reset(uint32_t now) {
    state=STARTUP; phase=step=level=reason=result=event=0;
    stateUs=now; stepUs=candidateUs=stationaryUs=lastSeq=0;
    target=positionMin=positionMax=0;
    candidate=stationaryCandidate=pendingNext=finalStop=overspeedStop=false;
    overspeedReversePending=overspeedReverseUsed=false;
    stopConfirmed=started=false;
    regression.reset(now); stopWindow.reset();
  }
  void resetDetection(uint32_t now) { regression.reset(now); candidate=stationaryCandidate=false; lastSeq=0; }
  void startStep(uint32_t now) {
    state=SETTLING; stateUs=stepUs=now; result=OK; event|=1;
    overspeedStop=false; stopWindow.reset(); resetDetection(now);
  }
  void stop(uint32_t why,uint32_t now,bool reliable=true) {
    if(state==FINISHED || state==EXITING) return;
    if(reason==OK) reason=why;
    if(phase>=1 && phase<=4 && state<=STATIONARY) { result=why; event|=2; }
    bool beginOverspeedStop=reliable && why==OVERSPEED && !overspeedStop;
    if(beginOverspeedStop) {
      overspeedReversePending=!overspeedReverseUsed && phase>=1 && phase<=4;
      stopWindow.reset();
    } else if(why!=OVERSPEED) {
      // A different stop request during the wait cancels the planned reverse leg.
      overspeedReversePending=false;
    }
    overspeedStop=overspeedStop || (reliable && why==OVERSPEED);
    finalStop=!overspeedReversePending; pendingNext=false;
    if(!reliable) { target=0; state=EXITING; stateUs=now; event|=4; return; }
    target=0;
    if(state!=COASTING) { state=COASTING; stateUs=now; resetDetection(now); }
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
  void next(uint32_t now,float speed) {
    pendingNext=false;
    const unsigned intervals=unsigned(ceilf(TORQUE_LIMIT_NM/TORQUE_STEP_NM));
    const unsigned stepLsbs=fabsf(speed)>=TORQUE_STEP_CHANGE_SPEED_RAD_S?HIGH_SPEED_TORQUE_STEP_LSBS:1u;
    ++step; // global step index; endpoints clamped, including non-divisible increments
    if(phase==1) {
      if(level>=intervals) {
        phase=2; level=stepLsbs<intervals?stepLsbs:intervals;
        target=fmaxf(0.f,TORQUE_LIMIT_NM-level*TORQUE_STEP_NM);
      } else {
        level=level+stepLsbs<intervals?level+stepLsbs:intervals;
        target=fminf(TORQUE_LIMIT_NM,level*TORQUE_STEP_NM);
      }
    } else if(phase==2) {
      if(target<=0) { phase=0; state=COASTING; stateUs=now; target=0; resetDetection(now); return; }
      level=level+stepLsbs<intervals?level+stepLsbs:intervals;
      target=level>=intervals?0:fmaxf(0.f,TORQUE_LIMIT_NM-level*TORQUE_STEP_NM);
    } else if(phase==3) {
      if(level>=intervals) {
        phase=4; level=stepLsbs<intervals?stepLsbs:intervals;
        target=fminf(0.f,-TORQUE_LIMIT_NM+level*TORQUE_STEP_NM);
      } else {
        level=level+stepLsbs<intervals?level+stepLsbs:intervals;
        target=fmaxf(-TORQUE_LIMIT_NM,level*-TORQUE_STEP_NM);
      }
    } else {
      if(target>=0) { stop(OK,now); return; }
      level=level+stepLsbs<intervals?level+stepLsbs:intervals;
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
    if(state!=COASTING && fabsf(f.speed)>=SPEED_LIMIT_RAD_S) stop(OVERSPEED,now);
    if(state==STARTUP) {
      if(!REQUIRE_INITIAL_STATIONARY) {
        // Keep feedback/error/overspeed checks above, but skip only the
        // initial stationary wait and start Phase 1 on the first valid reply.
        phase=1; level=0; started=true; stopConfirmed=false; startStep(now);
        return;
      }
      state=COASTING; stateUs=now; resetDetection(now);
    }
    if(!overspeedStop) regression.observe(f);
    bool still=!overspeedStop && stationary(f,now);
    if(state==COASTING) {
      if(overspeedStop) {
        float range=0, meanSpeed=0;
        bool full=stopWindow.observe(f.position,f.speed,range,meanSpeed);
        if(full && range<STOP_POSITION_RANGE_RAD && fabsf(meanSpeed)<STOP_MEAN_SPEED_RAD_S) {
          target=0; stopConfirmed=true;
          if(overspeedReversePending) {
            // After confirming rest, perform exactly one sweep in the opposite direction.
            phase=phase<=2?3:1; level=0; started=true; stopConfirmed=false;
            overspeedReversePending=false; overspeedReverseUsed=true; finalStop=false;
            startStep(now); return;
          }
          state=EXITING; stateUs=now; event|=4; return;
        }
        if(now-stateUs>=STOP_TIMEOUT_US) {
          if(reason==OK) reason=STOP_FAILED;
          target=0; state=EXITING; stateUs=now; event|=4; return;
        }
        target=0;
        return;
      }
      if(still) {
        target=0; stopConfirmed=true;
        if(finalStop) { state=EXITING; stateUs=now; event|=4; }
        else { phase=started?3:1; level=0; started=true; stopConfirmed=false; startStep(now); }
        return;
      }
      if(now-stateUs>=STOP_TIMEOUT_US) {
        if(reason==OK) reason=STOP_FAILED;
        target=0; state=EXITING; stateUs=now; event|=4; return;
      }
      target=0;
      return;
    }
    if(pendingNext) { next(now,f.speed); return; }
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
