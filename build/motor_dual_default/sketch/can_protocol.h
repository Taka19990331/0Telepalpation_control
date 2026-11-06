#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_tanhStb_noSD\\can_protocol.h"
#pragma once
#include <Arduino.h>
#include <math.h>
#include <FlexCAN_T4.h>
#include <IntervalTimer.h>
#include "control_config.h"
#include "can_diagnostics.h"
namespace CanProtocol {
using CanType=FlexCAN_T4<ControlConfig::CAN_BUS==1 ? CAN1 : (ControlConfig::CAN_BUS==2 ? CAN2 : CAN3),RX_SIZE_256,TX_SIZE_16>;
static CanType can;
struct Feedback { float position=0, velocity=0, torque=0; uint32_t rxUs=0, sequence=0; uint8_t error=0; bool valid=false; };
static Feedback feedback[ControlConfig::MOTOR_COUNT];
static bool motorFault[2]={false,false}, communicationFault[2]={false,false}, numericFault[2]={false,false};
constexpr uint32_t busBase=ControlConfig::CAN_BUS==1 ? CAN1 : (ControlConfig::CAN_BUS==2 ? CAN2 : CAN3);
inline bool selectedId(uint8_t id) { const int8_t i=ControlConfig::indexForId(id); return i>=0 && ControlConfig::selected(uint8_t(i)); }
inline bool anyMotorFault() { for(uint8_t i=0;i<2;++i) if(ControlConfig::selected(i) && (motorFault[i]||communicationFault[i]||numericFault[i])) return true; return false; }
inline CanDiagnostics::Snapshot snapshot(bool mailboxes=false) {
  CanDiagnostics::Snapshot s;
  s.us=micros()-CanDiagnostics::origin; s.queue=can.getTXQueueCount();
  s.esr=FLEXCANb_ESR1(busBase); s.ecr=FLEXCANb_ECR(busBase);
  if(mailboxes) for(unsigned i=8;i<16;++i) {
    const unsigned code=FLEXCAN_get_code(FLEXCANb_MBn_CS(busBase,i));
    if(code==FLEXCAN_MB_CODE_TX_INACTIVE) ++s.freeMB;
    if(code==FLEXCAN_MB_CODE_TX_ONCE) ++s.pendingMB;
  }
  CanDiagnostics::queueMax=max(CanDiagnostics::queueMax,s.queue);
  CanDiagnostics::observe(s.esr,s.ecr); return s;
}
inline bool writeDiagnosed(const CAN_message_t &m,float torque,uint8_t kind,uint8_t id) {
  using namespace CanDiagnostics;
  const uint32_t q=can.getTXQueueCount(); queueMax=max(queueMax,q);
  const uint32_t toWrite=measuring ? uint32_t(micros()-entry):0;
  const int result=can.write(m);
  queueMax=max(queueMax,can.getTXQueueCount());
  if(result) ++success; else {
    ++failures;
    if(!firstFailure.valid) {
      failureInIsr=measuring; firstFailure.valid=true; firstFailure.kind=kind; firstFailure.motorId=id; firstFailure.torque=torque;
      firstFailure.bus=snapshot(true); firstFailure.failures=failures;
      const int8_t ix=ControlConfig::indexForId(id); const auto &f=ix>=0?feedback[uint8_t(ix)]:feedback[0];
      firstFailure.age=f.valid ? uint32_t(micros()-f.rxUs):UINT32_MAX;
      firstFailure.interval=interval; firstFailure.previousDuration=duration; firstFailure.toWrite=toWrite;
    }
  }
  return result!=0;
}
inline void serviceDiagnostics() {
  CAN_error_t e;
  for(unsigned i=0;i<4 && can.error(e,false);++i) { noInterrupts(); ++CanDiagnostics::errorRecords; CanDiagnostics::observe(e.ESR1,e.ECR); interrupts(); }
}
static void (*feedbackHandler)(uint8_t,const Feedback &)=nullptr;
inline uint32_t encode(float x,float lo,float hi,uint8_t bits) {
  x=constrain(x,lo,hi); return uint32_t((x-lo)*float((1UL<<bits)-1)/(hi-lo));
}
inline float decode(uint32_t x,float lo,float hi,uint8_t bits) { return float(x)*(hi-lo)/float((1UL<<bits)-1)+lo; }
inline bool sendSpecial(uint8_t id,uint8_t code) {
  CAN_message_t m={}; m.id=id; m.len=8;
  for(uint8_t i=0;i<7;++i) m.buf[i]=0xff;
  m.buf[7]=code; return writeDiagnosed(m,0,code,id);
}
inline bool enable(uint8_t id) { return sendSpecial(id,0xfc); }
inline bool disable(uint8_t id) { return sendSpecial(id,0xfd); }
inline bool command(uint8_t id,float tau) {
  using namespace ControlConfig;
  if(!isfinite(tau)) { const int8_t ix=indexForId(id); if(ix>=0) numericFault[uint8_t(ix)]=true; tau=0; }
  const uint32_t pi=encode(0,POSITION_MIN,POSITION_MAX,16), vi=encode(0,VELOCITY_MIN,VELOCITY_MAX,12);
  const uint32_t kpi=encode(0,0,MIT_KP_MAX,12), kdi=encode(0,0,MIT_KD_MAX,12), ti=encode(tau,TORQUE_MIN,TORQUE_MAX,12);
  CAN_message_t m={}; m.id=id; m.len=8;
  m.buf[0]=pi>>8; m.buf[1]=pi; m.buf[2]=vi>>4; m.buf[3]=((vi&15)<<4)|(kpi>>8); m.buf[4]=kpi;
  m.buf[5]=kdi>>4; m.buf[6]=((kdi&15)<<4)|(ti>>8); m.buf[7]=ti;
  return writeDiagnosed(m,tau,0,id);
}
struct ReceivedFrame { CAN_message_t frame; uint32_t rxUs; uint8_t index; };
static ReceivedFrame received[ControlConfig::RX_QUEUE_ROWS];
static volatile uint32_t rxHead=0, rxTail=0;
static bool capture=false, haveRxTime[2]={false,false};
static uint32_t previousRxUs[2]={0,0}, rxGapMin[2]={UINT32_MAX,UINT32_MAX}, rxGapMax[2]={0,0};
inline void resetRx(bool enableCapture) { rxHead=rxTail=0; capture=enableCapture; for(uint8_t i=0;i<2;++i){haveRxTime[i]=false;previousRxUs[i]=0;rxGapMin[i]=UINT32_MAX;rxGapMax[i]=0;} }
inline void receiveFeedback(const CAN_message_t &m) {
  const uint32_t now=micros(); if(!capture) return;
  using namespace CanDiagnostics; ++rxFrames; if(m.flags.overrun) ++rxHardwareOverruns;
  const int8_t ix=ControlConfig::indexForId(m.buf[0]);
  if(m.flags.remote||m.flags.extended||m.len!=8||ix<0||!ControlConfig::selected(uint8_t(ix))) ++rxRejected;
  else {
    const uint8_t i=uint8_t(ix); ++rxAccepted;
    if(haveRxTime[i]) { const uint32_t gap=now-previousRxUs[i]; rxGapMin[i]=min(rxGapMin[i],gap); rxGapMax[i]=max(rxGapMax[i],gap); if(gap<ControlConfig::RX_INTERVAL_MIN_US||gap>ControlConfig::RX_INTERVAL_MAX_US) ++rxAbnormal; }
    haveRxTime[i]=true; previousRxUs[i]=now;
    if(rxHead-rxTail>=ControlConfig::RX_QUEUE_ROWS) ++rxOverflow;
    else { auto &r=received[rxHead%ControlConfig::RX_QUEUE_ROWS]; r.frame=m; r.rxUs=now; r.index=i; asm volatile("" ::: "memory"); ++rxHead; rxQueueMax=max(rxQueueMax,uint32_t(rxHead-rxTail)); }
  }
  rxIsrMax=max(rxIsrMax,uint32_t(micros()-now));
}
inline bool processFeedback(uint32_t now) {
  if(rxTail==rxHead) return false;
  const ReceivedFrame r=received[rxTail%ControlConfig::RX_QUEUE_ROWS]; asm volatile("" ::: "memory"); ++rxTail;
  const auto &m=r.frame; const uint8_t i=r.index; using namespace ControlConfig;
  if(!selected(i)) { ++CanDiagnostics::rxRejected; return true; }
  auto &f=feedback[i];
  if(uint32_t(now-r.rxUs)>RX_STALE_US) ++CanDiagnostics::rxStale;
  if(f.valid&&uint32_t(r.rxUs-f.rxUs)>FEEDBACK_TIMEOUT_US) communicationFault[i]=true;
  f.position=decode((uint32_t(m.buf[1])<<8)|m.buf[2],POSITION_MIN,POSITION_MAX,16);
  f.velocity=decode((uint32_t(m.buf[3])<<4)|(m.buf[4]>>4),VELOCITY_MIN,VELOCITY_MAX,12);
  f.torque=decode(((uint32_t(m.buf[4])&15)<<8)|m.buf[5],TORQUE_MIN,TORQUE_MAX,12);
  f.error=m.buf[7]; motorFault[i]=motorFault[i]||f.error!=0;
  numericFault[i]=numericFault[i]||!isfinite(f.position)||!isfinite(f.velocity)||!isfinite(f.torque);
  f.rxUs=r.rxUs; ++f.sequence; f.valid=true; ++CanDiagnostics::rxProcessed;
  if(feedbackHandler) feedbackHandler(MOTOR_IDS[i],f); return true;
}
inline void begin() {
  can.begin(); can.setBaudRate(ControlConfig::CAN_BAUD); can.setMaxMB(16); can.setMB(MB0,RX,STD);
  can.setMBFilter(REJECT_ALL); can.setMBFilter(MB0,ACCEPT_ALL); can.onReceive(MB0,receiveFeedback); can.enableMBInterrupt(MB0);
  NVIC_SET_PRIORITY(ControlConfig::CAN_BUS==1?IRQ_CAN1:(ControlConfig::CAN_BUS==2?IRQ_CAN2:IRQ_CAN3),ControlConfig::ISR_PRIORITY);
}
}
