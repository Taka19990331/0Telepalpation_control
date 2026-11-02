#include <Arduino.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include "can_protocol.h"
#include "control_algorithm.h"
#include "save_data.h"
#if !defined(ARDUINO_TEENSY41)
#error "Select Teensy 4.1 with built-in SD socket"
#endif
// Single source of truth for applied runtime settings; configuration contains only defaults.
struct RuntimeSettings {
  bool enabled=ControlConfig::INITIAL_ENABLED;
  float Kr=ControlConfig::INITIAL_K_R, Kf=ControlConfig::INITIAL_K_F;
};
static volatile RuntimeSettings runtime;
volatile float tau_cmd=0;
volatile bool active=false, startRequest=false;
volatile uint32_t runStart=0, statusCode=0;
static uint32_t logTick=0;
static bool timerReady=false;
static bool awaitingFirstFeedback=true;
static bool startupReported=false, activationReported=false, stopReported=false;
static bool serialInitialized=false;
static uint32_t startupBeginUs=0, startupSerialUs=0, startupCanUs=0, startupSdUs=0, startupTimerUs=0;
static uint32_t startupReadyUs=0;
static uint32_t startCommandUs=0, startSaveUs=0, startCanResetUs=0, startEnableUs=0, startZeroUs=0, startRequestUs=0;
static volatile uint32_t startActivatedUs=0;
ControlAlgorithm::Controller controller;
IntervalTimer controlTimer;

void stopRun(uint32_t reason) {
  CanProtocol::capture=false;
  active=false; startRequest=false; runtime.enabled=false; tau_cmd=0; statusCode=reason;
  controller.zeroOutput(); // Preserve observer/numeric fault until explicit S restart.
  CanDiagnostics::stopUs=micros()-CanDiagnostics::origin; CanDiagnostics::stopReason=reason;
  CanDiagnostics::stopZero=CanProtocol::command(0); CanDiagnostics::stopDisable=CanProtocol::disable();
  CanDiagnostics::latest=CanProtocol::snapshot(true);
  if(SaveData::recording) SaveData::finish(reason);
}
// CAN and timer have the same priority: each frame is processed exactly once,
// without overwriting an unprocessed frame when several arrive in one timer period.
void handleFeedback(const CanProtocol::Feedback &f) {
  if(!active || CanProtocol::motorFault || CanProtocol::communicationFault) return;
  // A first reply arriving after the startup deadline must not hide a timeout
  // by replacing the feedback timestamp before the next control interrupt.
  if(awaitingFirstFeedback && uint32_t(f.rxUs-runStart)>ControlConfig::FEEDBACK_TIMEOUT_US) {
    CanProtocol::communicationFault=true; return;
  }
  awaitingFirstFeedback=false;
  if(CanProtocol::numericFault || !isfinite(f.position)) { controller.trip(); return; }
  controller.feedback(f.velocity,f.torque,f.rxUs,f.position); // Also while runtime.enabled==false.
}
void controlISR() {
  const uint32_t now=micros();
  if(startRequest) {
    startRequest=false; runStart=now;
    // Before active, the original observer ignored startup replies. Do not feed
    // their pre-origin timestamps to the unsigned startup-deadline check.
    CanDiagnostics::rxStartupDiscarded=CanProtocol::rxHead-CanProtocol::rxTail;
    CanProtocol::rxTail=CanProtocol::rxHead;
    active=true; startActivatedUs=now;
  }
  if(!active) return;
  CanDiagnostics::enter(now);
  struct TimingGuard { ~TimingGuard() { CanDiagnostics::leave(); } } timingGuard;
  if(!SaveData::recording) { stopRun(4); return; }
  for(uint32_t n=0;n<ControlConfig::RX_BUDGET && CanProtocol::processFeedback(now);++n) {}
  if(CanProtocol::rxHead!=CanProtocol::rxTail) ++CanDiagnostics::rxBudgetHits;
  const auto &f=CanProtocol::feedback;
  const uint32_t age=f.valid ? uint32_t(now-f.rxUs) : uint32_t(now-runStart);
  uint32_t fault=0;
  if(CanProtocol::motorFault) fault=7;
  else if(CanDiagnostics::rxOverflow || CanDiagnostics::rxHardwareOverruns) fault=9;
  else if(CanProtocol::communicationFault || age>ControlConfig::FEEDBACK_TIMEOUT_US) fault=6;
  if(CanProtocol::numericFault || controller.faulted() ||
     (f.valid && (!isfinite(f.position)||!isfinite(f.velocity)||!isfinite(f.torque)))) {
    controller.trip(); fault=8;
  }
  tau_cmd=controller.command(runtime.enabled && f.valid && !fault,runtime.Kr,runtime.Kf);
  if(controller.faulted() || !isfinite(tau_cmd)) { controller.trip(); tau_cmd=0; fault=8; }
  const bool txOK=CanProtocol::command(tau_cmd);
  if(!txOK && CanDiagnostics::firstFailure.valid) {
    auto &fail=CanDiagnostics::firstFailure;
    fail.age=age; fail.rxPending=CanProtocol::rxHead-CanProtocol::rxTail;
    fail.rxOverflow=CanDiagnostics::rxOverflow; fail.rxMax=CanDiagnostics::rxQueueMax;
    fail.logPending=SaveData::head-SaveData::tail; fail.logDropped=CanDiagnostics::logDropped;
  }
  if(!txOK && !fault) fault=2;
  if(ControlConfig::SD_LOGGING && (logTick++%ControlConfig::LOG_DECIMATION==0 || fault)) {
    SaveData::Sample s={}; s.timestamp=now-runStart; s.age=f.valid?age:UINT32_MAX;
    s.position=f.position; s.rawVelocity=f.velocity; s.torque=f.torque; s.bias=controller.torqueBias;
    s.signals=controller.signals; s.Kr=runtime.Kr; s.Kf=runtime.Kf;
    s.txQueue=CanProtocol::can.getTXQueueCount(); s.isrInterval=CanDiagnostics::interval; s.previousIsrUs=CanDiagnostics::duration;
    s.diagUs=CanDiagnostics::latest.us; s.esr=CanDiagnostics::latest.esr; s.ecr=CanDiagnostics::latest.ecr;
    s.txSuccess=CanDiagnostics::success; s.txFailure=CanDiagnostics::failures;
    s.enabled=runtime.enabled; s.error=f.error; s.fault=fault;
    if(!SaveData::record(s) && !fault) fault=3;
  }
  if(fault) stopRun(fault);
}
void startRun() {
  const uint32_t startUs=micros();
  if(!timerReady) { Serial.println("START_REJECT,TIMER_NOT_READY"); return; }
  if(active || startRequest) { Serial.println("START_REJECT,SESSION_ALREADY_ACTIVE"); return; }
  if(SaveData::recording) { Serial.println("START_REJECT,ALREADY_RECORDING"); return; }
  const uint32_t saveUs=micros(); const bool saveOK=SaveData::start(); startSaveUs=micros()-saveUs;
  if(!saveOK) {
#if TRANS_ENABLE_SD_LOGGING
    Serial.print("START_REJECT,"); Serial.println(SaveData::lastStartFailure);
    Serial.print("START_PHASE,search_us,"); Serial.print(SaveData::lastStartSearchUs);
    Serial.print(",create_us,"); Serial.print(SaveData::lastStartCreateUs);
    Serial.print(",metadata_us,"); Serial.print(SaveData::lastStartMetadataUs);
    Serial.print(",sync_us,"); Serial.println(SaveData::lastStartSyncUs);
#else
    Serial.println("START_REJECT,LOGGING_DISABLED");
#endif
    return;
  }
  noInterrupts();
  const uint32_t canResetStart=micros(); CanProtocol::resetForSession(); startCanResetUs=micros()-canResetStart;
  CanDiagnostics::reset(micros());
  CanProtocol::resetRx(true);
  controller.reset(); runtime.Kr=ControlConfig::INITIAL_K_R; runtime.Kf=ControlConfig::INITIAL_K_F;
  runtime.enabled=ControlConfig::INITIAL_ENABLED; tau_cmd=0; statusCode=0; logTick=0;
  awaitingFirstFeedback=true;
  CanProtocol::feedback=CanProtocol::Feedback{};
  CanProtocol::motorFault=CanProtocol::communicationFault=CanProtocol::numericFault=false;
  const uint32_t enUs=micros(); const bool enabled=CanProtocol::enable(); startEnableUs=micros()-enUs;
  const uint32_t zeroUs=micros(); const bool zero=CanProtocol::command(0); startZeroUs=micros()-zeroUs;
  if(!enabled || !zero) stopRun(2);
  else { startActivatedUs=0; activationReported=false; stopReported=false; startRequest=true; startRequestUs=micros()-startUs; }
  interrupts();
  if(!enabled || !zero) Serial.println("START_FAIL,CAN_ENABLE_OR_ZERO_TX_REJECTED");
  else {
    Serial.print("START_ACCEPT,save_us,"); Serial.print(startSaveUs);
    Serial.print(",can_reset_us,"); Serial.print(startCanResetUs);
    Serial.print(",enable_us,"); Serial.print(startEnableUs);
    Serial.print(",zero_us,"); Serial.print(startZeroUs);
    Serial.print(",request_us,"); Serial.println(startRequestUs);
#if TRANS_ENABLE_SD_LOGGING
    Serial.print("START_PHASE,search_us,"); Serial.print(SaveData::lastStartSearchUs);
    Serial.print(",create_us,"); Serial.print(SaveData::lastStartCreateUs);
    Serial.print(",metadata_us,"); Serial.print(SaveData::lastStartMetadataUs);
    Serial.print(",sync_us,"); Serial.println(SaveData::lastStartSyncUs);
#endif
  }
}
void printStatus() {
  noInterrupts();
  const bool enabled=runtime.enabled, running=active, ready=active && controller.ready(), numeric=controller.faulted()||CanProtocol::numericFault;
  const float Kr=runtime.Kr, Kf=runtime.Kf, torque=tau_cmd;
  const uint32_t fault=statusCode;
  interrupts();
  Serial.print("STATUS,ON,"); Serial.print(enabled?1:0);
  Serial.print(",Kr,"); Serial.print(Kr,7); Serial.print(",Kf,"); Serial.print(Kf,7);
  Serial.print(",RUNNING,"); Serial.print(running?1:0); Serial.print(",OBSERVER_READY,"); Serial.print(ready?1:0);
  Serial.print(",NUMERIC_FAULT,"); Serial.print(numeric?1:0); Serial.print(",FAULT,"); Serial.print(fault);
  Serial.print(",tau_cmd_Nm,"); Serial.println(torque,7);
  if(!running && !startRequest) SaveData::writeDiagnostics(Serial);
}
void processCommand(char *line) {
  while(isspace((unsigned char)*line)) ++line;
  char *endLine=line+strlen(line);
  while(endLine>line && isspace((unsigned char)endLine[-1])) *--endLine=0;
  for(char *p=line;*p;++p) *p=toupper((unsigned char)*p);
  if(!strcmp(line,"S")) { startCommandUs=micros(); Serial.println("COMMAND,S,RECEIVED"); startRun(); } // Explicit reinitialization/recovery after stop.
  else if(!strcmp(line,"D")) { if(!active && !startRequest) SaveData::dumpCsv(); }
  else if(!strcmp(line,"STATUS")) printStatus();
  else if(!strcmp(line,"ON")) {
    noInterrupts();
    const bool allowed=(active || startRequest) && !statusCode && !controller.faulted() && !CanProtocol::numericFault &&
      !CanProtocol::motorFault && !CanProtocol::communicationFault;
    if(allowed) runtime.enabled=true; // Observer is NOT reset; readiness gates output in ISR.
    interrupts();
    if(!allowed) Serial.println("ERROR,ON requires accepted S request; use S to initialize");
    else Serial.println("ON_ACCEPT,enabled,1");
  } else if(!strcmp(line,"OFF")) {
    noInterrupts(); runtime.enabled=false; tau_cmd=0; controller.zeroOutput();
    if(active && !CanProtocol::command(0)) stopRun(2);
    interrupts(); // Keep observer running while OFF.
  } else if(!strcmp(line,"BIAS")) {
    noInterrupts();
    const auto &f=CanProtocol::feedback;
    const bool allowed=active && !runtime.enabled && !controller.faulted() && f.valid &&
      !CanProtocol::motorFault && !CanProtocol::communicationFault && !CanProtocol::numericFault &&
      uint32_t(micros()-f.rxUs)<=ControlConfig::FEEDBACK_TIMEOUT_US && isfinite(f.torque) &&
      fabsf(f.velocity)<ControlConfig::BIAS_STATIONARY_SPEED;
    if(allowed) { controller.torqueBias=f.torque; controller.reset(); }
    interrupts();
    if(!allowed) Serial.println("ERROR,BIAS requires healthy OFF stationary feedback");
  } else if((!strncmp(line,"KR",2)||!strncmp(line,"KF",2)) && isspace((unsigned char)line[2])) {
    char *end=nullptr; errno=0; const float value=strtof(line+3,&end);
    while(end && isspace((unsigned char)*end)) ++end;
    if(end==line+3 || !end || *end || errno==ERANGE || !isfinite(value)) {
      Serial.println("ERROR,invalid gain; finite float required"); return;
    }
    noInterrupts(); if(line[1]=='R') runtime.Kr=value; else runtime.Kf=value; interrupts();
  } else Serial.println("ERROR,unknown command");
}
void setup() {
  startupBeginUs=micros();
  uint32_t phase=micros();
  CanProtocol::feedbackHandler=handleFeedback; CanProtocol::begin(); startupCanUs=micros()-phase;
  phase=micros();
  SaveData::begin();
  startupSdUs=micros()-phase;
  phase=micros();
  controlTimer.priority(ControlConfig::ISR_PRIORITY);
  timerReady=controlTimer.begin(controlISR,ControlConfig::CONTROL_PERIOD_US);
  startupTimerUs=micros()-phase;
  startupReadyUs=micros()-startupBeginUs;
}
void reportStartup() {
  Serial.print("BOOT_SD,mount_us,"); Serial.print(SaveData::mountTimeUs());
  Serial.print(",search_us,"); Serial.print(SaveData::fileSearchTimeUs());
  Serial.print(",latest_file,"); Serial.println(SaveData::latestName());
  Serial.print("BOOT_TIMING,serial_begin_us,"); Serial.print(startupSerialUs);
  Serial.print(",can_begin_us,"); Serial.print(startupCanUs);
  Serial.print(",sd_init_and_search_us,"); Serial.print(startupSdUs);
  Serial.print(",timer_init_us,"); Serial.print(startupTimerUs);
  Serial.print(",ready_elapsed_us,"); Serial.println(startupReadyUs);
  Serial.print("READY,ID,"); Serial.print(ControlConfig::MOTOR_ID); Serial.print(",CAN,"); Serial.print(ControlConfig::CAN_BUS);
  Serial.print(",SD,"); Serial.print(!ControlConfig::SD_LOGGING?"DISABLED":(SaveData::isReady()?"OK":"ERROR")); Serial.print(",TIMER,"); Serial.println(timerReady?"OK":"ERROR");
  Serial.println("Commands: S, ON, OFF, KR <value>, KF <value>, STATUS, BIAS, X, D (newline; X immediate)");
}
void loop() {
  if(!serialInitialized && Serial) {
    const uint32_t phase=micros(); Serial.begin(ControlConfig::SERIAL_BAUD);
    startupSerialUs=micros()-phase; serialInitialized=true;
  }
  if(!startupReported && serialInitialized) { reportStartup(); startupReported=true; }
  if(startActivatedUs && !activationReported) { activationReported=true; Serial.print("START_ACTIVE,elapsed_us,"); Serial.println(uint32_t(startActivatedUs-startCommandUs)); }
  if(!active && statusCode && !stopReported) { stopReported=true; Serial.print("STOP,reason,"); Serial.println(statusCode); }
  static char line[ControlConfig::COMMAND_BUFFER_BYTES]; static size_t used=0; static bool overflow=false;
  while(Serial.available()) {
    const char c=Serial.read();
    // Immediate stop only as a standalone command, not inside an invalid gain token.
    if((c=='x'||c=='X') && used==0 && !overflow) { noInterrupts(); stopRun(1); interrupts(); }
    else if(c=='\r'||c=='\n') {
      if(overflow) Serial.println("ERROR,command too long");
      else if(used) { line[used]=0; processCommand(line); }
      used=0; overflow=false;
    } else if(used<sizeof(line)-1 && !overflow) line[used++]=c;
    else overflow=true;
  }
  if(active && !SaveData::recording) { noInterrupts(); stopRun(4); interrupts(); }
  static uint32_t lastDiagnosticUs=0;
  if(active && uint32_t(micros()-lastDiagnosticUs)>=100000) {
    noInterrupts(); CanDiagnostics::latest=CanProtocol::snapshot(); interrupts();
    lastDiagnosticUs=micros();
  }
  CanProtocol::serviceDiagnostics();
  const uint32_t sdStart=micros();
  SaveData::service();
  CanDiagnostics::sdMaxUs=max(CanDiagnostics::sdMaxUs,uint32_t(micros()-sdStart));
}
