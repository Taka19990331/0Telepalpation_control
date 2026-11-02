#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch2\\can_diagnostics.h"
#pragma once
// Fixed-size RAM diagnostics. No formatting, SD or Serial in interrupt context.
namespace CanDiagnostics {
struct Snapshot {
  uint32_t us=0, queue=0, esr=0, ecr=0, freeMB=0, pendingMB=0;
};
struct Failure {
  bool valid=false; uint8_t kind=0; float torque=0;
  uint32_t age=0, interval=0, previousDuration=0, toWrite=0, duration=0, failures=0;
  uint32_t rxPending=0, rxOverflow=0, rxMax=0, logPending=0, logDropped=0;
  Snapshot bus;
};
static uint32_t origin=0, success=0, failures=0, queueMax=0, rxGapMax=0;
static uint32_t interval=0, intervalMax=0, duration=0, durationMax=0;
static uint32_t intervalOverruns=0, executionOverruns=0, lastEntry=0, entry=0;
static uint32_t errorRecords=0, errorBits=0, txErrorMax=0, rxErrorMax=0, busOffObserved=0;
static uint32_t sdMaxUs=0, ringMax=0;
static uint32_t intervalMin=UINT32_MAX, rxGapMin=UINT32_MAX, rxIsrMax=0;
static uint32_t rxFrames=0, rxAccepted=0, rxRejected=0, rxOverflow=0, rxQueueMax=0;
static uint32_t rxHardwareOverruns=0, rxStale=0, rxAbnormal=0, rxProcessed=0, rxBudgetHits=0;
static uint32_t logDropped=0, sdWriteFailures=0;
static uint32_t rxStartupDiscarded=0;
static bool measuring=false, failureInIsr=false;
static Snapshot latest;
static Failure firstFailure;
static bool stopZero=false, stopDisable=false;
static uint32_t stopUs=0, stopReason=0;
inline void reset(uint32_t now) {
  origin=now; success=failures=queueMax=rxGapMax=0;
  interval=intervalMax=duration=durationMax=intervalOverruns=executionOverruns=0;
  lastEntry=entry=0; errorRecords=errorBits=txErrorMax=rxErrorMax=busOffObserved=0;
  sdMaxUs=ringMax=0; measuring=false; failureInIsr=false; latest=Snapshot{}; firstFailure=Failure{};
  intervalMin=rxGapMin=UINT32_MAX; rxIsrMax=0;
  rxFrames=rxAccepted=rxRejected=rxOverflow=rxQueueMax=0;
  rxHardwareOverruns=rxStale=rxAbnormal=rxProcessed=rxBudgetHits=0;
  logDropped=sdWriteFailures=0;
  rxStartupDiscarded=0;
  stopZero=stopDisable=false; stopUs=stopReason=0;
}
inline void observe(uint32_t esr,uint32_t ecr) {
  errorBits|=esr; txErrorMax=max(txErrorMax,ecr&255U); rxErrorMax=max(rxErrorMax,(ecr>>8)&255U);
  if((esr&0x30U)==0x20U || (esr&0x04U)) busOffObserved=1;
}
inline void enter(uint32_t now) {
  entry=now; measuring=true;
  interval=lastEntry ? uint32_t(now-lastEntry):0; lastEntry=now;
  intervalMax=max(intervalMax,interval);
  if(interval) intervalMin=min(intervalMin,interval);
  if(interval>ControlConfig::CONTROL_PERIOD_US) ++intervalOverruns;
}
inline void leave() {
  duration=micros()-entry; durationMax=max(durationMax,duration);
  if(duration>ControlConfig::CONTROL_PERIOD_US) ++executionOverruns;
  if(failureInIsr)
    firstFailure.duration=duration;
  failureInIsr=false;
  measuring=false;
}
}


