#include <Arduino.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include "can_protocol.h"
#include "control_algorithm.h"
#include "control_log.h"
#if !defined(ARDUINO_TEENSY41)
#error "Select Teensy 4.1"
#endif
struct RuntimeSettings {
  bool enabled=ControlConfig::INITIAL_ENABLED;
  float Kr[2]={ControlConfig::INITIAL_K_R1,ControlConfig::INITIAL_K_R2};
  float Kf[2]={ControlConfig::INITIAL_K_F1,ControlConfig::INITIAL_K_F2};
};
static volatile RuntimeSettings runtime;
static volatile float tau_cmd[2]={0,0};
static volatile bool active=false,startRequest=false;
static volatile uint32_t runStart=0,statusCode=0;
static bool timerReady=false, awaitingFirstFeedback[2]={true,true};
static ControlAlgorithm::Controller controllers[2]={
  ControlAlgorithm::Controller(ControlConfig::MOTORS[0],ControlConfig::CONTROL_PERIOD_US),
  ControlAlgorithm::Controller(ControlConfig::MOTORS[1],ControlConfig::CONTROL_PERIOD_US)
};
static IntervalTimer controlTimer;
static uint32_t commandTicks[2]={0,0},lastTxUs[2]={0,0},txIntervalUs[2]={0,0};
static bool stopZeroResult[2]={false,false},stopDisableResult[2]={false,false};
void stopRun(uint32_t reason) {
  CanProtocol::capture=false; active=false; startRequest=false; runtime.enabled=false; tau_cmd[0]=tau_cmd[1]=0; statusCode=reason;
  for(uint8_t i=0;i<2;++i) if(ControlConfig::selected(i)) {
    controllers[i].zeroOutput(); stopZeroResult[i]=CanProtocol::command(ControlConfig::MOTOR_IDS[i],0);
    stopDisableResult[i]=CanProtocol::disable(ControlConfig::MOTOR_IDS[i]);
  }
  for(uint8_t i=0;i<2;++i) { CanDiagnostics::stopZero[i]=stopZeroResult[i];CanDiagnostics::stopDisable[i]=stopDisableResult[i];CanDiagnostics::latest[i]=CanProtocol::snapshot(i,true); }
  CanDiagnostics::stopUs=micros()-CanDiagnostics::origin; CanDiagnostics::stopReason=reason;
}
void handleFeedback(uint8_t id,const CanProtocol::Feedback &f) {
  const int8_t ix=ControlConfig::indexForId(id); if(ix<0) return; const uint8_t i=uint8_t(ix);
  if(!ControlConfig::selected(i)||!active||CanProtocol::motorFault[i]||CanProtocol::communicationFault[i]) return;
  if(awaitingFirstFeedback[i]&&uint32_t(f.rxUs-runStart)>ControlConfig::FEEDBACK_TIMEOUT_US) { CanProtocol::communicationFault[i]=true; return; }
  awaitingFirstFeedback[i]=false;
  if(CanProtocol::numericFault[i]||!isfinite(f.position)) { controllers[i].trip(); return; }
  ControlLog::append(id,uint32_t(f.rxUs-runStart),f.position,f.velocity);
  controllers[i].feedback(f.position,f.velocity,f.torque,f.rxUs);
}
void controlISR() {
  const uint32_t now=micros(); if(startRequest){startRequest=false;active=true;} if(!active) return;
  CanDiagnostics::enter(now); struct TimingGuard{~TimingGuard(){CanDiagnostics::leave();}} timingGuard;
  for(uint8_t i=0;i<2;++i) if(ControlConfig::selected(i)) {
    for(uint32_t n=0;n<ControlConfig::RX_BUDGET&&CanProtocol::processFeedback(i,now);++n){}
    if(CanProtocol::rxHead[i]!=CanProtocol::rxTail[i])++CanDiagnostics::rxBudgetHits[i];
  }
  uint32_t fault=0;
  if(CanDiagnostics::rxOverflowTotal||CanDiagnostics::rxHardwareOverruns[0]||CanDiagnostics::rxHardwareOverruns[1]) fault=9;
  for(uint8_t i=0;i<2;++i) if(ControlConfig::selected(i)) {
    const auto &f=CanProtocol::feedback[i]; const uint32_t age=f.valid?uint32_t(now-f.rxUs):uint32_t(now-runStart);
    if(awaitingFirstFeedback[i]&&age>ControlConfig::FEEDBACK_TIMEOUT_US) CanProtocol::communicationFault[i]=true;
    if(f.valid&&age>ControlConfig::FEEDBACK_TIMEOUT_US) CanProtocol::communicationFault[i]=true;
    if(CanProtocol::motorFault[i]) fault=7;
    else if(CanProtocol::communicationFault[i]) fault=6;
    else if(CanProtocol::numericFault[i]||controllers[i].faulted()||
      (f.valid&&(!isfinite(f.position)||!isfinite(f.velocity)||!isfinite(f.torque)))) { controllers[i].trip(); fault=8; }
    tau_cmd[i]=controllers[i].command(runtime.enabled&&f.valid&&!fault,runtime.Kr[i],runtime.Kf[i]);
    if(controllers[i].faulted()||!isfinite(tau_cmd[i])) { controllers[i].trip();tau_cmd[i]=0;fault=8; }
  }
  for(uint8_t i=0;i<2;++i) if(ControlConfig::selected(i)) {
    ++commandTicks[i];if(lastTxUs[i])txIntervalUs[i]=now-lastTxUs[i];lastTxUs[i]=now;
    const bool txOK=CanProtocol::command(ControlConfig::MOTOR_IDS[i],fault?0:tau_cmd[i]);
    if(!txOK&&!fault)fault=2;
  }
  if(fault) stopRun(fault);
}
void startRun() {
  if(!ControlConfig::ANY_MOTOR_ON) { Serial.println("ERROR,S requires at least one selected motor"); return; }
  if(!timerReady||active||startRequest) return;
  noInterrupts(); CanDiagnostics::reset(micros());ControlLog::reset();CanProtocol::resetRx(true);runStart=micros();
  for(uint8_t i=0;i<2;++i) { if(ControlConfig::selected(i)) controllers[i].reset(); awaitingFirstFeedback[i]=ControlConfig::selected(i);tau_cmd[i]=0;commandTicks[i]=0;lastTxUs[i]=txIntervalUs[i]=0;CanProtocol::txAttempts[i]=CanProtocol::txFailures[i]=0;CanProtocol::feedback[i]=CanProtocol::Feedback{};CanProtocol::motorFault[i]=CanProtocol::communicationFault[i]=CanProtocol::numericFault[i]=false; }
  runtime.Kr[0]=ControlConfig::INITIAL_K_R1;runtime.Kr[1]=ControlConfig::INITIAL_K_R2;
  runtime.Kf[0]=ControlConfig::INITIAL_K_F1;runtime.Kf[1]=ControlConfig::INITIAL_K_F2;
  runtime.enabled=ControlConfig::INITIAL_ENABLED;statusCode=0;
  bool ok=true; for(uint8_t i=0;i<2;++i) if(ControlConfig::selected(i)) { ok=CanProtocol::enable(ControlConfig::MOTOR_IDS[i])&&ok;ok=CanProtocol::command(ControlConfig::MOTOR_IDS[i],0)&&ok; }
  if(!ok) stopRun(2); else startRequest=true; interrupts();
}
void printStatus() {
  noInterrupts(); const bool enabled=runtime.enabled,running=active;const uint32_t fault=statusCode;interrupts();
  Serial.print("STATUS,ON,");Serial.print(enabled?1:0);
  Serial.print(",RUNNING,");Serial.print(running?1:0);Serial.print(",MOTOR1_SELECTED,");Serial.print(ControlConfig::MOTOR1_ON?1:0);Serial.print(",MOTOR2_SELECTED,");Serial.print(ControlConfig::MOTOR2_ON?1:0);Serial.print(",FAULT,");Serial.println(fault);
  for(uint8_t i=0;i<2;++i) {
    noInterrupts();const auto f=CanProtocol::feedback[i];const auto busDiag=CanProtocol::snapshot(i,true);const bool ready=controllers[i].ready(),numeric=controllers[i].faulted()||CanProtocol::numericFault[i];const float tau=tau_cmd[i],bias=controllers[i].torqueBias,kr=runtime.Kr[i],kf=runtime.Kf[i];const uint32_t rxAge=f.valid?uint32_t(micros()-f.rxUs):UINT32_MAX;const bool mf=CanProtocol::motorFault[i],cf=CanProtocol::communicationFault[i];const float as=controllers[i].angleStartDeg(),af=controllers[i].angleFullDeg();const uint8_t nm=uint8_t(controllers[i].notchMode());interrupts();
    Serial.print("MOTOR,ID,");Serial.print(unsigned(ControlConfig::MOTOR_IDS[i]));Serial.print(",CAN,");Serial.print(i+1);Serial.print(",SELECTED,");Serial.print(ControlConfig::selected(i)?1:0);Serial.print(",KR,");Serial.print(kr,7);Serial.print(",KF,");Serial.print(kf,7);Serial.print(",FB_VALID,");Serial.print(f.valid?1:0);Serial.print(",RX_AGE_us,");Serial.print(rxAge);Serial.print(",RX_INTERVAL_us,");Serial.print(CanProtocol::rxLastIntervalUs[i]);Serial.print(",RX_ISR_MAX_us,");Serial.print(CanDiagnostics::rxIsrMax[i]);Serial.print(",RX_QUEUE_MAX,");Serial.print(CanDiagnostics::rxQueueMax[i]);Serial.print(",RX_OVERFLOW,");Serial.print(CanDiagnostics::rxOverflow[i]);Serial.print(",RFOB_READY,");Serial.print(ready?1:0);Serial.print(",RFOB_FAULT,");Serial.print(numeric?1:0);Serial.print(",MOTOR_ERROR,");Serial.print(mf?1:0);Serial.print(",TIMEOUT,");Serial.print(cf?1:0);Serial.print(",TAU_CMD_Nm,");Serial.print(tau,7);Serial.print(",BIAS_Nm,");Serial.print(bias,7);Serial.print(",TX_COUNT,");Serial.print(commandTicks[i]);Serial.print(",TX_INTERVAL_us,");Serial.print(txIntervalUs[i]);Serial.print(",TX_QUEUE,");Serial.print(busDiag.queue);Serial.print(",TX_QUEUE_MAX,");Serial.print(CanDiagnostics::queueMax[i]);Serial.print(",TX_FREE_MB,");Serial.print(busDiag.freeMB);Serial.print(",TX_PENDING_MB,");Serial.print(busDiag.pendingMB);Serial.print(",CAN_ESR,");Serial.print(busDiag.esr,HEX);Serial.print(",CAN_ECR,");Serial.print(busDiag.ecr,HEX);Serial.print(",CAN_ERROR_RECORDS,");Serial.print(CanDiagnostics::errorRecords[i]);Serial.print(",TX_ERRORS,");Serial.print(CanProtocol::txFailures[i]);Serial.print(",STOP_ZERO_QUEUE_ACCEPTED,");Serial.print(stopZeroResult[i]?1:0);Serial.print(",STOP_DISABLE_QUEUE_ACCEPTED,");Serial.print(stopDisableResult[i]?1:0);Serial.print(",NOTCH_MODE,");Serial.print(unsigned(nm));Serial.print(",ANGLE_START_deg_RMS,");Serial.print(as,5);Serial.print(",ANGLE_FULL_deg_RMS,");Serial.println(af,5);
  }
  Serial.print("CONTROL,ISR_MAX_us,");Serial.print(CanDiagnostics::durationMax);Serial.print(",ISR_INTERVAL_MAX_us,");Serial.print(CanDiagnostics::intervalMax);Serial.print(",ISR_OVERRUNS,");Serial.println(CanDiagnostics::executionOverruns);
}
void processCommand(char *line) {
  while(isspace((unsigned char)*line))++line;char *endLine=line+strlen(line);while(endLine>line&&isspace((unsigned char)endLine[-1]))*--endLine=0;for(char *p=line;*p;++p)*p=toupper((unsigned char)*p);
  if(!strcmp(line,"S")) startRun();
  else if(!strcmp(line,"STATUS")) printStatus();
  else if(!strcmp(line,"D")){noInterrupts();const bool allowed=!active&&!startRequest;interrupts();if(allowed)ControlLog::dump();else Serial.println("ERROR,D requires a stopped control session");}
  else if(!strcmp(line,"ON")) { if(!ControlConfig::ANY_MOTOR_ON){Serial.println("ERROR,ON requires at least one selected motor");return;}noInterrupts();const bool allowed=active&&!statusCode&&!CanProtocol::anyMotorFault();if(allowed)runtime.enabled=true;interrupts();if(!allowed)Serial.println("ERROR,ON requires healthy running session; use S to initialize"); }
  else if(!strcmp(line,"OFF")){noInterrupts();runtime.enabled=false;tau_cmd[0]=tau_cmd[1]=0;for(uint8_t i=0;i<2;++i)if(ControlConfig::selected(i)){controllers[i].zeroOutput();if(active&&!CanProtocol::command(ControlConfig::MOTOR_IDS[i],0))stopRun(2);}interrupts();}
  else if(!strcmp(line,"BIAS")){noInterrupts();bool allowed=active&&!runtime.enabled&&!CanProtocol::anyMotorFault();for(uint8_t i=0;i<2;++i)if(ControlConfig::selected(i)){const auto &f=CanProtocol::feedback[i];allowed=allowed&&f.valid&&uint32_t(micros()-f.rxUs)<=ControlConfig::FEEDBACK_TIMEOUT_US&&isfinite(f.torque)&&fabsf(f.velocity)<ControlConfig::BIAS_STATIONARY_SPEED&&!controllers[i].faulted();}if(allowed)for(uint8_t i=0;i<2;++i)if(ControlConfig::selected(i)){controllers[i].torqueBias=CanProtocol::feedback[i].torque;controllers[i].reset();}interrupts();if(!allowed)Serial.println("ERROR,BIAS requires all selected motors OFF, healthy, and stationary");}
  else if(!strncmp(line,"NOTCH ",6)){ControlConfig::NotchMode mode;if(!strcmp(line+6,"OFF"))mode=ControlConfig::NOTCH_OFF;else if(!strcmp(line+6,"FIXED"))mode=ControlConfig::NOTCH_FIXED;else if(!strcmp(line+6,"ADAPTIVE"))mode=ControlConfig::NOTCH_ADAPTIVE;else{Serial.println("ERROR,NOTCH expects OFF, FIXED, or ADAPTIVE");return;}noInterrupts();const bool allowed=!runtime.enabled;if(allowed)for(uint8_t i=0;i<2;++i)if(ControlConfig::selected(i))controllers[i].setNotchMode(mode);interrupts();if(!allowed)Serial.println("ERROR,NOTCH mode can only change while control is OFF");}
  else if(!strncmp(line,"ANGLE_START ",12)||!strncmp(line,"ANGLE_FULL ",11)){const bool isStart=line[6]=='S';const char *arg=line+(isStart?12:11);char *end=nullptr;errno=0;const float value=strtof(arg,&end);while(end&&isspace((unsigned char)*end))++end;if(end==arg||!end||*end||errno==ERANGE||!isfinite(value)){Serial.println("ERROR,invalid angle threshold");return;}noInterrupts();bool changed=true;for(uint8_t i=0;i<2;++i)if(ControlConfig::selected(i)){const float st=isStart?value:controllers[i].angleStartDeg(),fn=isStart?controllers[i].angleFullDeg():value;changed=controllers[i].setAngleThresholds(st,fn)&&changed;}interrupts();if(!changed)Serial.println("ERROR,require 0 < ANGLE_START < ANGLE_FULL");}
  else if(!strncmp(line,"KR",2)||!strncmp(line,"KF",2)) {
    const bool isKr=line[1]=='R'; const char *arg=line+2;
    if(*arg!='1'&&*arg!='2'){Serial.println("ERROR,use KR1, KR2, KF1, or KF2 <finite value>");return;}
    const uint8_t target=uint8_t(*arg-'1');++arg;
    if(!isspace((unsigned char)*arg)){Serial.println("ERROR,gain syntax: KR1|KR2|KF1|KF2 <finite value>");return;}
    while(isspace((unsigned char)*arg))++arg;
    char *end=nullptr;errno=0;const float value=strtof(arg,&end);
    while(end&&isspace((unsigned char)*end))++end;
    if(end==arg||!end||*end||errno==ERANGE||!isfinite(value)){Serial.println("ERROR,invalid gain; finite float required");return;}
    if(!ControlConfig::selected(target)){Serial.println("ERROR,target motor is not selected");return;}
    noInterrupts();if(isKr)runtime.Kr[target]=value;else runtime.Kf[target]=value;interrupts();
  }
  else Serial.println("ERROR,unknown command");
}
void setup(){Serial.begin(ControlConfig::SERIAL_BAUD);CanProtocol::feedbackHandler=handleFeedback;CanProtocol::begin();controlTimer.priority(ControlConfig::ISR_PRIORITY);timerReady=controlTimer.begin(controlISR,ControlConfig::CONTROL_PERIOD_US);Serial.print("READY,CAN1,1,M1,");Serial.print(ControlConfig::MOTOR1_ON?1:0);Serial.print(",CAN2,2,M2,");Serial.print(ControlConfig::MOTOR2_ON?1:0);Serial.print(",TIMER,");Serial.println(timerReady?"OK":"ERROR");Serial.println("Commands: S, ON, OFF, KR1 <value>, KR2 <value>, KF1 <value>, KF2 <value>, NOTCH OFF|FIXED|ADAPTIVE, ANGLE_START <deg_RMS>, ANGLE_FULL <deg_RMS>, STATUS, BIAS, D, X");}
void loop(){static char line[ControlConfig::COMMAND_BUFFER_BYTES];static size_t used=0;static bool overflow=false;while(Serial.available()){const char c=Serial.read();if((c=='x'||c=='X')&&used==0&&!overflow){noInterrupts();stopRun(1);interrupts();}else if(c=='\r'||c=='\n'){if(overflow)Serial.println("ERROR,command too long");else if(used){line[used]=0;processCommand(line);}used=0;overflow=false;}else if(used<sizeof(line)-1&&!overflow)line[used++]=c;else overflow=true;}static uint32_t lastDiagnosticUs=0;if(active&&uint32_t(micros()-lastDiagnosticUs)>=100000){noInterrupts();for(uint8_t i=0;i<2;++i)CanDiagnostics::latest[i]=CanProtocol::snapshot(i);interrupts();lastDiagnosticUs=micros();}CanProtocol::serviceDiagnostics();}
