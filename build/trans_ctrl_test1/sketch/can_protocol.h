#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_test1\\can_protocol.h"
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
static Feedback feedback;
constexpr uint32_t busBase=ControlConfig::CAN_BUS==1 ? CAN1 : (ControlConfig::CAN_BUS==2 ? CAN2 : CAN3);
inline CanDiagnostics::Snapshot snapshot(bool mailboxes=false) {
  CanDiagnostics::Snapshot s;
  s.us=micros()-CanDiagnostics::origin; s.queue=can.getTXQueueCount();
  s.esr=FLEXCANb_ESR1(busBase); s.ecr=FLEXCANb_ECR(busBase);
  // FIFO disabled with 16 MB: only MB8..15 are TX. Never lock RX CS registers.
  if(mailboxes) for(unsigned i=8;i<16;++i) {
    const unsigned code=FLEXCAN_get_code(FLEXCANb_MBn_CS(busBase,i));
    if(code==FLEXCAN_MB_CODE_TX_INACTIVE) ++s.freeMB;
    if(code==FLEXCAN_MB_CODE_TX_ONCE) ++s.pendingMB;
  }
  CanDiagnostics::queueMax=max(CanDiagnostics::queueMax,s.queue);
  CanDiagnostics::observe(s.esr,s.ecr); return s;
}
inline bool writeDiagnosed(const CAN_message_t &m,float torque,uint8_t kind) {
  using namespace CanDiagnostics;
  const uint32_t q=can.getTXQueueCount(); queueMax=max(queueMax,q);
  const uint32_t toWrite=measuring ? uint32_t(micros()-entry):0;
  const int result=can.write(m);
  queueMax=max(queueMax,can.getTXQueueCount());
  if(result) ++success; else {
    ++failures;
    if(!firstFailure.valid) {
      failureInIsr=measuring; firstFailure.valid=true; firstFailure.kind=kind; firstFailure.torque=torque;
      firstFailure.bus=snapshot(true); firstFailure.failures=failures;
      firstFailure.age=feedback.valid ? uint32_t(micros()-feedback.rxUs):UINT32_MAX;
      firstFailure.interval=interval; firstFailure.previousDuration=duration;
      firstFailure.toWrite=toWrite;
    }
  }
  return result!=0;
}
// error() decodes strings and masks CAN IRQ internally: loop only, bounded work.
inline void serviceDiagnostics() {
  CAN_error_t e;
  for(unsigned i=0;i<4 && can.error(e,false);++i) {
    noInterrupts(); ++CanDiagnostics::errorRecords;
    CanDiagnostics::observe(e.ESR1,e.ECR); interrupts();
  }
}


static bool motorFault=false, communicationFault=false, numericFault=false;
static void (*feedbackHandler)(const Feedback &)=nullptr;
inline uint32_t encode(float x,float lo,float hi,uint8_t bits) {
  x=constrain(x,lo,hi); return uint32_t((x-lo)*float((1UL<<bits)-1)/(hi-lo));
}
inline float decode(uint32_t x,float lo,float hi,uint8_t bits) { return float(x)*(hi-lo)/float((1UL<<bits)-1)+lo; }
inline bool sendSpecial(uint8_t code) {
  CAN_message_t m={}; m.id=ControlConfig::MOTOR_ID; m.len=8;
  for(uint8_t i=0;i<7;++i) m.buf[i]=0xff;
  m.buf[7]=code; return writeDiagnosed(m,0,code);
}
inline bool enable() { return sendSpecial(0xfc); }
inline bool disable() { return sendSpecial(0xfd); }
// Only torque is configurable at runtime. MIT position/velocity/Kp/Kd stay zero.
inline bool command(float tau) {
  using namespace ControlConfig;
  if(!isfinite(tau)) { numericFault=true; tau=0; }
  const uint32_t pi=encode(0,POSITION_MIN,POSITION_MAX,16);
  const uint32_t vi=encode(0,VELOCITY_MIN,VELOCITY_MAX,12);
  const uint32_t kpi=encode(0,0,MIT_KP_MAX,12), kdi=encode(0,0,MIT_KD_MAX,12);
  const uint32_t ti=encode(tau,TORQUE_MIN,TORQUE_MAX,12);
  CAN_message_t m={}; m.id=MOTOR_ID; m.len=8;
  m.buf[0]=pi>>8; m.buf[1]=pi; m.buf[2]=vi>>4;
  m.buf[3]=((vi&15)<<4)|(kpi>>8); m.buf[4]=kpi;
  m.buf[5]=kdi>>4; m.buf[6]=((kdi&15)<<4)|(ti>>8); m.buf[7]=ti;
  return writeDiagnosed(m,tau,0);
}
inline void receiveFeedback(const CAN_message_t &m) {
  using namespace ControlConfig;
  if(m.flags.remote || m.flags.extended || m.len!=8 || m.buf[0]!=MOTOR_ID) return;
  const uint32_t now=micros();
  if(feedback.valid) CanDiagnostics::rxGapMax=max(CanDiagnostics::rxGapMax,uint32_t(now-feedback.rxUs));
  if(feedback.valid && uint32_t(now-feedback.rxUs)>FEEDBACK_TIMEOUT_US) communicationFault=true;
  feedback.position=decode((uint32_t(m.buf[1])<<8)|m.buf[2],POSITION_MIN,POSITION_MAX,16);
  feedback.velocity=decode((uint32_t(m.buf[3])<<4)|(m.buf[4]>>4),VELOCITY_MIN,VELOCITY_MAX,12);
  feedback.torque=decode(((uint32_t(m.buf[4])&15)<<8)|m.buf[5],TORQUE_MIN,TORQUE_MAX,12);
  feedback.error=m.buf[7]; motorFault=motorFault || feedback.error!=0;
  numericFault=numericFault || !isfinite(feedback.position) || !isfinite(feedback.velocity) || !isfinite(feedback.torque);
  feedback.rxUs=now; ++feedback.sequence; feedback.valid=true;
  if(feedbackHandler) feedbackHandler(feedback);
}
inline void begin() {
  can.begin(); can.setBaudRate(ControlConfig::CAN_BAUD);
  can.setMaxMB(16); can.setMB(MB0,RX,STD);
  can.setMBFilter(REJECT_ALL); can.setMBFilter(MB0,ACCEPT_ALL);
  can.onReceive(MB0,receiveFeedback); can.enableMBInterrupt(MB0);
  NVIC_SET_PRIORITY(ControlConfig::CAN_BUS==1 ? IRQ_CAN1 : (ControlConfig::CAN_BUS==2 ? IRQ_CAN2 : IRQ_CAN3),ControlConfig::ISR_PRIORITY);
}
}




