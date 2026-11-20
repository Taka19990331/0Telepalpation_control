#pragma once
#include <Arduino.h>
#include <math.h>
#include <FlexCAN_T4.h>
#include <IntervalTimer.h>
#include "control_config.h"
#include "can_diagnostics.h"
namespace CanProtocol {
using CanType1=FlexCAN_T4<CAN1,RX_SIZE_256,TX_SIZE_16>;
using CanType2=FlexCAN_T4<CAN2,RX_SIZE_256,TX_SIZE_16>;
static CanType1 can1;
static CanType2 can2;
struct Feedback { float position=0, velocity=0, torque=0; uint32_t rxUs=0, sequence=0; uint8_t error=0; bool valid=false; };
static Feedback feedback[ControlConfig::MOTOR_COUNT];
static bool motorFault[2]={false,false}, communicationFault[2]={false,false}, numericFault[2]={false,false};
static uint32_t txAttempts[2]={0,0},txFailures[2]={0,0};
static bool selectedId(uint8_t id) { const int8_t i=ControlConfig::indexForId(id); return i>=0 && ControlConfig::selected(uint8_t(i)); }
inline bool anyMotorFault() { for(uint8_t i=0;i<2;++i) if(ControlConfig::selected(i) && (motorFault[i]||communicationFault[i]||numericFault[i])) return true; return false; }
inline CanDiagnostics::Snapshot snapshot(uint8_t index,bool mailboxes=false) {
  CanDiagnostics::Snapshot s; const uint32_t base=index==0?CAN1:CAN2;
  s.us=micros()-CanDiagnostics::origin;
  s.queue=index==0?can1.getTXQueueCount():can2.getTXQueueCount();
  s.esr=FLEXCANb_ESR1(base); s.ecr=FLEXCANb_ECR(base);
  if(mailboxes) for(unsigned i=8;i<16;++i) {
    const unsigned code=FLEXCAN_get_code(FLEXCANb_MBn_CS(base,i));
    if(code==FLEXCAN_MB_CODE_TX_INACTIVE) ++s.freeMB;
    if(code==FLEXCAN_MB_CODE_TX_ONCE) ++s.pendingMB;
  }
  CanDiagnostics::queueMax[index]=max(CanDiagnostics::queueMax[index],s.queue);
  CanDiagnostics::observe(s.esr,s.ecr); return s;
}
inline bool writeDiagnosed(const CAN_message_t &m,float torque,uint8_t kind,uint8_t id) {
  using namespace CanDiagnostics;
  const int8_t ix=ControlConfig::indexForId(id); if(ix<0) return false;
  const uint8_t index=uint8_t(ix); const uint32_t q=index==0?can1.getTXQueueCount():can2.getTXQueueCount();
  queueMax[index]=max(queueMax[index],q);
  const uint32_t toWrite=measuring?uint32_t(micros()-entry):0;
  const int result=index==0?can1.write(m):can2.write(m);
  const uint32_t after=index==0?can1.getTXQueueCount():can2.getTXQueueCount(); queueMax[index]=max(queueMax[index],after);
  ++txAttempts[index];
  if(result) ++success[index]; else {
    ++txFailures[index]; ++failures[index];
    if(!firstFailure[index].valid) {
      auto &fail=firstFailure[index]; fail.valid=true; fail.kind=kind; fail.motorId=id; fail.torque=torque;
      failureInIsr[index]=measuring; fail.bus=snapshot(index,true); fail.failures=failures[index];
      const auto &f=feedback[index]; fail.age=f.valid?uint32_t(micros()-f.rxUs):UINT32_MAX;
      fail.interval=interval; fail.previousDuration=duration; fail.toWrite=toWrite;
    }
  }
  return result!=0;
}
inline void serviceDiagnostics() {
  CAN_error_t e;
  for(unsigned i=0;i<4&&can1.error(e,false);++i) { noInterrupts(); ++CanDiagnostics::errorRecords[0]; CanDiagnostics::observe(e.ESR1,e.ECR); interrupts(); }
  for(unsigned i=0;i<4&&can2.error(e,false);++i) { noInterrupts(); ++CanDiagnostics::errorRecords[1]; CanDiagnostics::observe(e.ESR1,e.ECR); interrupts(); }
}
static void (*feedbackHandler)(uint8_t,const Feedback &)=nullptr;
inline uint32_t encode(float x,float lo,float hi,uint8_t bits) { x=constrain(x,lo,hi); return uint32_t((x-lo)*float((1UL<<bits)-1)/(hi-lo)); }
inline float decode(uint32_t x,float lo,float hi,uint8_t bits) { return float(x)*(hi-lo)/float((1UL<<bits)-1)+lo; }
inline bool sendSpecial(uint8_t id,uint8_t code) {
  CAN_message_t m={}; m.id=id; m.len=8; for(uint8_t i=0;i<7;++i)m.buf[i]=0xff; m.buf[7]=code;
  return writeDiagnosed(m,0,code,id);
}
inline bool enable(uint8_t id) { return sendSpecial(id,0xfc); }
inline bool disable(uint8_t id) { return sendSpecial(id,0xfd); }
inline bool command(uint8_t id,float tau) {
  using namespace ControlConfig;
  const int8_t ix=indexForId(id); if(ix<0) return false;
  if(!isfinite(tau)) { numericFault[uint8_t(ix)]=true; tau=0; }
  const uint32_t pi=encode(0,POSITION_MIN,POSITION_MAX,16),vi=encode(0,VELOCITY_MIN,VELOCITY_MAX,12);
  const uint32_t kpi=encode(0,0,MIT_KP_MAX,12),kdi=encode(0,0,MIT_KD_MAX,12),ti=encode(tau,TORQUE_MIN,TORQUE_MAX,12);
  CAN_message_t m={}; m.id=id; m.len=8;
  m.buf[0]=pi>>8; m.buf[1]=pi; m.buf[2]=vi>>4; m.buf[3]=((vi&15)<<4)|(kpi>>8); m.buf[4]=kpi;
  m.buf[5]=kdi>>4; m.buf[6]=((kdi&15)<<4)|(ti>>8); m.buf[7]=ti;
  return writeDiagnosed(m,tau,0,id);
}
struct ReceivedFrame { CAN_message_t frame; uint32_t rxUs; };
static ReceivedFrame received[2][ControlConfig::RX_QUEUE_ROWS];
static volatile uint32_t rxHead[2]={0,0},rxTail[2]={0,0};
static bool capture=false,haveRxTime[2]={false,false};
static uint32_t previousRxUs[2]={0,0},rxGapMin[2]={UINT32_MAX,UINT32_MAX},rxGapMax[2]={0,0},rxLastIntervalUs[2]={0,0};
inline void resetRx(bool enableCapture) {
  capture=enableCapture;
  for(uint8_t i=0;i<2;++i){rxHead[i]=rxTail[i]=0;haveRxTime[i]=false;previousRxUs[i]=0;rxGapMin[i]=UINT32_MAX;rxGapMax[i]=rxLastIntervalUs[i]=0;}
}
inline void receiveFeedbackForBus(uint8_t index,const CAN_message_t &m) {
  const uint32_t now=micros(); if(!capture)return;
  using namespace CanDiagnostics; ++rxFrames[index]; if(m.flags.overrun)++rxHardwareOverruns[index];
  if(m.flags.remote||m.flags.extended||m.len!=8||m.buf[0]!=ControlConfig::MOTOR_IDS[index]||!ControlConfig::selected(index))++rxRejected[index];
  else {
    ++rxAccepted[index];
    if(haveRxTime[index]) { const uint32_t gap=now-previousRxUs[index]; rxLastIntervalUs[index]=gap;rxGapMin[index]=min(rxGapMin[index],gap);rxGapMax[index]=max(rxGapMax[index],gap);if(gap<ControlConfig::RX_INTERVAL_MIN_US||gap>ControlConfig::RX_INTERVAL_MAX_US)++rxAbnormal[index]; }
    haveRxTime[index]=true;previousRxUs[index]=now;
    if(rxHead[index]-rxTail[index]>=ControlConfig::RX_QUEUE_ROWS){++rxOverflow[index];++rxOverflowTotal;}
    else { auto &r=received[index][rxHead[index]%ControlConfig::RX_QUEUE_ROWS];r.frame=m;r.rxUs=now;asm volatile("" ::: "memory");++rxHead[index];rxQueueMax[index]=max(rxQueueMax[index],uint32_t(rxHead[index]-rxTail[index])); }
  }
  rxIsrMax[index]=max(rxIsrMax[index],uint32_t(micros()-now));
}
inline void receiveCAN1(const CAN_message_t &m) { receiveFeedbackForBus(0,m); }
inline void receiveCAN2(const CAN_message_t &m) { receiveFeedbackForBus(1,m); }
inline bool processFeedback(uint8_t index,uint32_t now) {
  if(rxTail[index]==rxHead[index])return false;
  const ReceivedFrame r=received[index][rxTail[index]%ControlConfig::RX_QUEUE_ROWS];asm volatile("" ::: "memory");++rxTail[index];
  const auto &m=r.frame;using namespace ControlConfig;auto &f=feedback[index];
  if(uint32_t(now-r.rxUs)>RX_STALE_US)++CanDiagnostics::rxStale[index];
  if(f.valid&&uint32_t(r.rxUs-f.rxUs)>FEEDBACK_TIMEOUT_US)communicationFault[index]=true;
  f.position=decode((uint32_t(m.buf[1])<<8)|m.buf[2],POSITION_MIN,POSITION_MAX,16);
  f.velocity=decode((uint32_t(m.buf[3])<<4)|(m.buf[4]>>4),VELOCITY_MIN,VELOCITY_MAX,12);
  f.torque=decode(((uint32_t(m.buf[4])&15)<<8)|m.buf[5],TORQUE_MIN,TORQUE_MAX,12);
  f.error=m.buf[7];motorFault[index]=motorFault[index]||f.error!=0;
  numericFault[index]=numericFault[index]||!isfinite(f.position)||!isfinite(f.velocity)||!isfinite(f.torque);
  f.rxUs=r.rxUs;++f.sequence;f.valid=true;++CanDiagnostics::rxProcessed[index];
  if(feedbackHandler)feedbackHandler(MOTOR_IDS[index],f);return true;
}
template<typename Bus> inline void configureBus(Bus &can) {
  can.begin();can.setBaudRate(ControlConfig::CAN_BAUD);can.setMaxMB(16);can.setMB(MB0,RX,STD);
  can.setMBFilter(REJECT_ALL);can.setMBFilter(MB0,ACCEPT_ALL);
}
inline void begin() {
  configureBus(can1);configureBus(can2);
  can1.onReceive(MB0,receiveCAN1);can1.enableMBInterrupt(MB0);
  can2.onReceive(MB0,receiveCAN2);can2.enableMBInterrupt(MB0);
  NVIC_SET_PRIORITY(IRQ_CAN1,ControlConfig::ISR_PRIORITY);NVIC_SET_PRIORITY(IRQ_CAN2,ControlConfig::ISR_PRIORITY);
}
}
