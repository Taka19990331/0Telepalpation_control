#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_tanhStb_noSD\\can_diagnostics.h"
#pragma once
// Fixed-size RAM diagnostics. No formatting or Serial in interrupt context.
namespace CanDiagnostics {
struct Snapshot {
  uint32_t us=0, queue=0, esr=0, ecr=0, freeMB=0, pendingMB=0;
};
struct Failure {
  bool valid=false; uint8_t kind=0, motorId=0; float torque=0;
  uint32_t age=0, interval=0, previousDuration=0, toWrite=0, duration=0, failures=0;
  uint32_t rxPending=0, rxOverflow=0, rxMax=0;
  Snapshot bus;
};
static uint32_t origin=0;
static uint32_t success[2]={0,0},failures[2]={0,0},queueMax[2]={0,0};
static uint32_t interval=0, intervalMax=0, duration=0, durationMax=0;
static uint32_t intervalOverruns=0, executionOverruns=0, lastEntry=0, entry=0;
static uint32_t errorRecords[2]={0,0},errorBits=0,txErrorMax=0,rxErrorMax=0,busOffObserved=0;
static uint32_t intervalMin=UINT32_MAX,rxIsrMax[2]={0,0};
static uint32_t rxFrames[2]={0,0},rxAccepted[2]={0,0},rxRejected[2]={0,0},rxOverflow[2]={0,0},rxOverflowTotal=0,rxQueueMax[2]={0,0};
static uint32_t rxHardwareOverruns[2]={0,0},rxStale[2]={0,0},rxAbnormal[2]={0,0},rxProcessed[2]={0,0},rxBudgetHits[2]={0,0};
static uint32_t rxStartupDiscarded[2]={0,0};
static bool measuring=false,failureInIsr[2]={false,false};
static Snapshot latest[2];
static Failure firstFailure[2];
static bool stopZero[2]={false,false},stopDisable[2]={false,false};
static uint32_t stopUs=0, stopReason=0;
inline void reset(uint32_t now) {
  origin=now;
  for(uint8_t i=0;i<2;++i){success[i]=failures[i]=queueMax[i]=errorRecords[i]=rxIsrMax[i]=0;rxFrames[i]=rxAccepted[i]=rxRejected[i]=rxOverflow[i]=rxQueueMax[i]=0;rxHardwareOverruns[i]=rxStale[i]=rxAbnormal[i]=rxProcessed[i]=rxBudgetHits[i]=rxStartupDiscarded[i]=0;failureInIsr[i]=false;latest[i]=Snapshot{};firstFailure[i]=Failure{};stopZero[i]=stopDisable[i]=false;}
  interval=intervalMax=duration=durationMax=intervalOverruns=executionOverruns=0;
  lastEntry=entry=0; errorBits=txErrorMax=rxErrorMax=busOffObserved=0;
  measuring=false; intervalMin=UINT32_MAX; rxOverflowTotal=0; stopUs=stopReason=0;
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
  for(uint8_t i=0;i<2;++i) if(failureInIsr[i]) firstFailure[i].duration=duration;
  failureInIsr[0]=failureInIsr[1]=false;
  measuring=false;
}
}


