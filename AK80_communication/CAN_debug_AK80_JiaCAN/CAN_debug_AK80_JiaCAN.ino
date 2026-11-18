
#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// CAN_debug_AK80_JiaCAN - Jia Mode1 polling CAN + start-up error timeline
// Teensy 4.1: CAN1 -> AK80 ID1, CAN2 -> AK80 ID2 (separate 1-Mbps buses).
// Exactly ONE sketch file. No IntervalTimer, callbacks, events(), or MB changes.
// All motor commands have Kp=Kd=torque=0; moving a loaded mechanism can be unsafe.
// ============================================================================

// Editable defaults. Can also change PERIOD / PRE / POST from Serial while idle.
constexpr uint32_t DEFAULT_PERIOD_US = 400;    // 400=2.5 kHz, 500=2 kHz
constexpr uint32_t TEST_DURATION_MS = 10000;  // RUNNING duration, excluding waits
constexpr uint32_t DEFAULT_PRE_WAIT_MS = 0;   // S -> MIT 0xFC: tests delay BEFORE enable
constexpr uint32_t DEFAULT_POST_WAIT_MS = 0;  // MIT 0xFC -> periodic zero: test separately
constexpr uint32_t CAN_BAUD = 1000000;
constexpr uint32_t FEEDBACK_TIMEOUT_US = 100000;
constexpr uint32_t PRINT_INTERVAL_MS = 1000;
constexpr uint32_t CAN_POLL_US = 250;          // ECR/ESR polling; NOT CAN TX period
constexpr uint32_t TRACE_FAST_UNTIL_US = 2000000;
constexpr uint32_t TRACE_FAST_STEP_US = 1000; // 1-ms history for first 2 s after S
constexpr uint32_t TRACE_SLOW_STEP_US = 20000;// 20-ms history thereafter
constexpr uint16_t TRACE_CAPACITY = 3200;     // ~50 KB on Teensy 4.1
constexpr uint16_t EVENT_CAPACITY = 256;
constexpr uint8_t MOTOR_ID[2] = {1, 2};

// Jia Mode1 AK80-8 MIT ranges; changing these changes the wire protocol.
constexpr float P_MIN=-12.5f, P_MAX=12.5f;
constexpr float V_MIN=-45.0f, V_MAX=45.0f;
constexpr float KP_MIN=0.0f, KP_MAX=500.0f;
constexpr float KD_MIN=0.0f, KD_MAX=5.0f;
constexpr float T_MIN=-32.0f, T_MAX=32.0f;

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can1;
FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> Can2;
elapsedMicros txTimer1, txTimer2;

enum Phase : uint8_t { IDLE, PRE_WAIT, POST_WAIT, RUNNING };
Phase phase = IDLE;
uint32_t periodUs=DEFAULT_PERIOD_US, preWaitMs=DEFAULT_PRE_WAIT_MS;
uint32_t postWaitMs=DEFAULT_POST_WAIT_MS;
uint32_t bootUs=0, initBeginUs=0, initEndUs=0;
uint32_t startUs=0, phaseBeginUs=0, runBeginUs=0, firstPeriodicTxUs=0;
uint32_t lastPrintMs=0, lastPollUs=0, lastTraceUs=0;
bool dataAvailable=false, mitEnableIssued=false;
bool firstPeriodicMotorTx[2]={false,false};

const uint32_t CAN_ERROR_BITS = FLEXCAN_ESR_ACK_ERR | FLEXCAN_ESR_BIT0_ERR |
    FLEXCAN_ESR_BIT1_ERR | FLEXCAN_ESR_STF_ERR | FLEXCAN_ESR_FRM_ERR |
    FLEXCAN_ESR_CRC_ERR;
const uint32_t CAN_WARNING_BITS = FLEXCAN_ESR_TX_WRN | FLEXCAN_ESR_RX_WRN;

struct MotorStats {
  uint32_t txOk=0, txFailed=0, rxTotal=0, rxValid=0, rxAfterRun=0;
  uint32_t lastRxUs=0, maxRxIntervalUs=0;
  bool hasFeedback=false, hasFeedbackAfterRun=false;
  uint8_t motorError=0;
  float posRad=0, velRadS=0, torqueNm=0;
  uint8_t tec=0, rec=0, maxTec=0, maxRec=0;
  uint8_t previousTec=0, previousRec=0;
  uint32_t esr1=0, errorBitsSeen=0, warningBitsSeen=0;
  uint32_t firstTecAt=UINT32_MAX, firstRecAt=UINT32_MAX;
  uint32_t lastTecIncreaseAt=UINT32_MAX, lastRecIncreaseAt=UINT32_MAX;
  uint32_t maxTecAt=UINT32_MAX, maxRecAt=UINT32_MAX;
  uint32_t lastZeroRecoveryAt=UINT32_MAX;
  uint8_t thresholds=0; // TEC32/96/128 and REC32/96/128
  bool counterActive=false, passiveSeen=false, busOffSeen=false;
  uint32_t episodes=0;
};
MotorStats motor[2];

// A trace row captures current counters AND the largest values between rows.
// This avoids hiding short spikes in the down-sampled CSV plot.
struct TraceRow {
  uint32_t tUs;
  uint8_t phase;
  uint8_t tec[2], rec[2], peakTec[2], peakRec[2];
};
TraceRow traceRows[TRACE_CAPACITY];
uint16_t traceCount=0;
uint32_t traceDropped=0;
uint8_t pendingPeakTec[2]={0,0}, pendingPeakRec[2]={0,0};

struct EventRow {
  uint32_t tUs, esr1;
  const char *name;
  uint8_t motor, phase, tec, rec; // motor=0 denotes both / global
};
EventRow events[EVENT_CAPACITY];
uint16_t eventCount=0;
uint32_t eventDropped=0;

const char* phaseName(Phase p) {
  switch(p) {
    case PRE_WAIT: return "PRE_WAIT";
    case POST_WAIT: return "POST_WAIT";
    case RUNNING: return "RUNNING";
    default: return "IDLE";
  }
}

void recordEvent(const char *name, uint8_t bus=0) {
  if (!dataAvailable) return;
  if (eventCount >= EVENT_CAPACITY) { ++eventDropped; return; }
  EventRow &e=events[eventCount++];
  e.tUs=micros()-startUs;
  e.name=name; e.motor=bus; e.phase=(uint8_t)phase;
  e.tec=bus ? motor[bus-1].tec : 0;
  e.rec=bus ? motor[bus-1].rec : 0;
  e.esr1=bus ? motor[bus-1].esr1 : 0;
}

uint16_t floatToUint(float x, float lo, float hi, uint8_t bits) {
  x=constrain(x,lo,hi);
  return (uint16_t)((x-lo)*(float)((1UL<<bits)-1)/(hi-lo));
}
float uintToFloat(uint16_t x, float lo, float hi, uint8_t bits) {
  return (float)x*(hi-lo)/(float)((1UL<<bits)-1)+lo;
}
bool writeCan(uint8_t i, const CAN_message_t &msg) {
  return i==0 ? Can1.write(msg)!=0 : Can2.write(msg)!=0;
}

// This is the standard 8-byte AK80 MIT packet used in Jia Mode1 Motor1/Motor2.
bool sendZeroTorque(uint8_t i) {
  const uint16_t p=floatToUint(0,P_MIN,P_MAX,16);
  const uint16_t v=floatToUint(0,V_MIN,V_MAX,12);
  const uint16_t kp=floatToUint(0,KP_MIN,KP_MAX,12);
  const uint16_t kd=floatToUint(0,KD_MIN,KD_MAX,12);
  const uint16_t t=floatToUint(0,T_MIN,T_MAX,12);
  CAN_message_t msg{};
  msg.id=MOTOR_ID[i]; msg.len=8;
  msg.flags.extended=0; msg.flags.remote=0;
  msg.buf[0]=p>>8; msg.buf[1]=p&0xFF;
  msg.buf[2]=v>>4; msg.buf[3]=((v&0xF)<<4)|(kp>>8);
  msg.buf[4]=kp&0xFF; msg.buf[5]=kd>>4;
  msg.buf[6]=((kd&0xF)<<4)|(t>>8); msg.buf[7]=t&0xFF;
  return writeCan(i,msg);
}
bool sendSpecial(uint8_t i, uint8_t code) {
  CAN_message_t msg{};
  msg.id=MOTOR_ID[i]; msg.len=8;
  msg.flags.extended=0; msg.flags.remote=0;
  for(uint8_t k=0;k<7;++k) msg.buf[k]=0xFF;
  msg.buf[7]=code; // 0xFC enter MIT, 0xFD exit MIT
  return writeCan(i,msg);
}

// Jia Mode1 reception: deliberately retain its len and ID acceptance rules.
void parseFeedback(uint8_t i, const CAN_message_t &msg) {
  MotorStats &d=motor[i];
  ++d.rxTotal;
  if (msg.len<6) return;
  if (msg.id!=MOTOR_ID[i] && msg.buf[0]!=MOTOR_ID[i]) return;
  const uint16_t p=((uint16_t)msg.buf[1]<<8)|msg.buf[2];
  const uint16_t v=((uint16_t)msg.buf[3]<<4)|(msg.buf[4]>>4);
  const uint16_t t=(((uint16_t)msg.buf[4]&15)<<8)|msg.buf[5];
  d.posRad=uintToFloat(p,P_MIN,P_MAX,16);
  d.velRadS=uintToFloat(v,V_MIN,V_MAX,12);
  d.torqueNm=uintToFloat(t,T_MIN,T_MAX,12);
  d.motorError=msg.len>=8 ? msg.buf[7] : 0;
  const uint32_t now=micros();
  if(d.hasFeedback) {
    const uint32_t gap=now-d.lastRxUs;
    if(gap>d.maxRxIntervalUs) d.maxRxIntervalUs=gap;
  } else recordEvent(i==0?"FIRST_RX_M1":"FIRST_RX_M2",i+1);
  d.lastRxUs=now; d.hasFeedback=true; ++d.rxValid;
  if(phase==RUNNING && firstPeriodicTxUs!=0 && now>=firstPeriodicTxUs) {
    if(!d.hasFeedbackAfterRun) recordEvent("FIRST_RX_AFTER_RUN",i+1);
    d.hasFeedbackAfterRun=true; ++d.rxAfterRun;
  }
}
void readMotor(uint8_t i) {
  CAN_message_t msg;
  if(i==0) { while(Can1.read(msg)) parseFeedback(i,msg); }
  else { while(Can2.read(msg)) parseFeedback(i,msg); }
}

// Read-only CAN hardware observation; does not reconfigure/clear ESR1.
// Time is sampled within CAN_POLL_US, NOT the exact time of an error frame.
void sampleCan(uint8_t i) {
  MotorStats &d=motor[i];
  const uint32_t base=i==0?CAN1:CAN2;
  const uint32_t ecr=FLEXCANb_ECR(base);
  const uint32_t esr=FLEXCANb_ESR1(base);
  const uint32_t t=micros()-startUs;
  const uint8_t tec=(uint8_t)(ecr&255U);
  const uint8_t rec=(uint8_t)((ecr>>8)&255U);
  d.tec=tec; d.rec=rec; d.esr1=esr;
  if(tec>pendingPeakTec[i]) pendingPeakTec[i]=tec;
  if(rec>pendingPeakRec[i]) pendingPeakRec[i]=rec;
  if(tec>d.maxTec) {d.maxTec=tec; d.maxTecAt=t;}
  if(rec>d.maxRec) {d.maxRec=rec; d.maxRecAt=t;}
  if(tec && d.firstTecAt==UINT32_MAX) {d.firstTecAt=t;recordEvent("FIRST_TEC_NONZERO",i+1);}
  if(rec && d.firstRecAt==UINT32_MAX) {d.firstRecAt=t;recordEvent("FIRST_REC_NONZERO",i+1);}
  if(tec>d.previousTec) d.lastTecIncreaseAt=t;
  if(rec>d.previousRec) d.lastRecIncreaseAt=t;
  const bool active=tec!=0||rec!=0;
  if(active && !d.counterActive) {++d.episodes;recordEvent("COUNTER_EPISODE_START",i+1);}
  if(!active && d.counterActive) {d.lastZeroRecoveryAt=t;recordEvent("COUNTERS_RETURNED_ZERO",i+1);}
  d.counterActive=active;
  if(tec>=32 && !(d.thresholds&1)) {d.thresholds|=1;recordEvent("TEC_GE_32",i+1);}
  if(tec>=96 && !(d.thresholds&2)) {d.thresholds|=2;recordEvent("TEC_GE_96",i+1);}
  if(tec>=128 && !(d.thresholds&4)) {d.thresholds|=4;recordEvent("TEC_GE_128",i+1);}
  if(rec>=32 && !(d.thresholds&8)) {d.thresholds|=8;recordEvent("REC_GE_32",i+1);}
  if(rec>=96 && !(d.thresholds&16)) {d.thresholds|=16;recordEvent("REC_GE_96",i+1);}
  if(rec>=128 && !(d.thresholds&32)) {d.thresholds|=32;recordEvent("REC_GE_128",i+1);}
  d.previousTec=tec; d.previousRec=rec;
  const uint32_t newErrors=(esr & CAN_ERROR_BITS) & ~d.errorBitsSeen;
  if(newErrors) {d.errorBitsSeen|=newErrors;recordEvent("ESR_ERROR_FLAG_FIRST_SEEN",i+1);}
  const uint32_t newWarnings=(esr & CAN_WARNING_BITS) & ~d.warningBitsSeen;
  if(newWarnings) {d.warningBitsSeen|=newWarnings;recordEvent("ESR_WARNING_FIRST_SEEN",i+1);}
  const uint32_t fault=FLEXCAN_ESR_get_fault_code(esr);
  if(fault==CAN_ERROR_PASSIVE && !d.passiveSeen) {
    d.passiveSeen=true; recordEvent("ERROR_PASSIVE",i+1);
  }
  if(fault==CAN_ERROR_BUS_OFF && !d.busOffSeen) {
    d.busOffSeen=true; recordEvent("BUS_OFF",i+1);
  }
}

void saveTrace(bool force=false) {
  if(!dataAvailable) return;
  const uint32_t now=micros(),t=now-startUs;
  const uint32_t step=t<TRACE_FAST_UNTIL_US ? TRACE_FAST_STEP_US : TRACE_SLOW_STEP_US;
  if(!force && t-lastTraceUs<step) return;
  lastTraceUs=t;
  if(traceCount>=TRACE_CAPACITY) {++traceDropped;return;}
  TraceRow &r=traceRows[traceCount++];
  r.tUs=t; r.phase=(uint8_t)phase;
  for(uint8_t i=0;i<2;++i) {
    r.tec[i]=motor[i].tec; r.rec[i]=motor[i].rec;
    r.peakTec[i]=pendingPeakTec[i]; r.peakRec[i]=pendingPeakRec[i];
    pendingPeakTec[i]=motor[i].tec; pendingPeakRec[i]=motor[i].rec;
  }
}
void pollCanAndTrace() {
  const uint32_t now=micros();
  if((uint32_t)(now-lastPollUs)<CAN_POLL_US) return;
  lastPollUs=now;
  sampleCan(0); sampleCan(1);
  saveTrace();
}

void printErrorFlags(uint32_t flags) {
  if(!flags) {Serial.print('-');return;}
  if(flags&FLEXCAN_ESR_ACK_ERR) Serial.print("ACK|");
  if(flags&FLEXCAN_ESR_BIT0_ERR) Serial.print("BIT0|");
  if(flags&FLEXCAN_ESR_BIT1_ERR) Serial.print("BIT1|");
  if(flags&FLEXCAN_ESR_STF_ERR) Serial.print("STUFF|");
  if(flags&FLEXCAN_ESR_FRM_ERR) Serial.print("FORM|");
  if(flags&FLEXCAN_ESR_CRC_ERR) Serial.print("CRC|");
}
void printTimeOrNA(uint32_t t) {
  if(t==UINT32_MAX) Serial.print("NA");
  else Serial.print(t/1000.0f,3);
}
void printStatus() {
  const uint32_t now=micros();
  Serial.print("STATE,");Serial.println(phaseName(phase));
  for(uint8_t i=0;i<2;++i) {
    const MotorStats &d=motor[i];
    Serial.print('M');Serial.print(i+1);
    Serial.print(" TX:");Serial.print(d.txOk);
    Serial.print(" RX:");Serial.print(d.rxValid);
    Serial.print(" RX_RAW:");Serial.print(d.rxTotal);
    Serial.print(" TX_FAIL:");Serial.print(d.txFailed);
    Serial.print(" AGE_ms:");
    if(d.hasFeedback) Serial.print((now-d.lastRxUs)/1000.0f,2);
    else Serial.print("NEVER");
    Serial.print(" RX_GAP_MAX_us:");Serial.print(d.maxRxIntervalUs);
    Serial.print(" TEC/REC:");Serial.print(d.tec);Serial.print('/');Serial.print(d.rec);
    Serial.print(" MAX:");Serial.print(d.maxTec);Serial.print('/');Serial.print(d.maxRec);
    Serial.print(" MOTOR_ERROR:");Serial.print(d.motorError);
    Serial.print(" ESR1:0x");Serial.print(d.esr1,HEX);
    Serial.print(" ERROR_SEEN:");printErrorFlags(d.errorBitsSeen);
    Serial.print(" EPISODES:");Serial.print(d.episodes);
    Serial.println();
  }
}
void printSummary() {
  Serial.print("SUMMARY,PERIOD_US,");Serial.print(periodUs);
  Serial.print(",PRE_MS,");Serial.print(preWaitMs);
  Serial.print(",POST_MS,");Serial.print(postWaitMs);
  Serial.print(",TRACE_ROWS,");Serial.print(traceCount);
  Serial.print(",TRACE_DROPPED,");Serial.print(traceDropped);
  Serial.print(",EVENTS,");Serial.print(eventCount);
  Serial.print(",EVENT_DROPPED,");Serial.println(eventDropped);
  for(uint8_t i=0;i<2;++i) {
    const MotorStats &d=motor[i];
    Serial.print("SUMMARY_M");Serial.print(i+1);
    Serial.print(",FIRST_TEC_ms,");printTimeOrNA(d.firstTecAt);
    Serial.print(",FIRST_REC_ms,");printTimeOrNA(d.firstRecAt);
    Serial.print(",MAX_TEC,");Serial.print(d.maxTec);
    Serial.print(",MAX_TEC_AT_ms,");printTimeOrNA(d.maxTecAt);
    Serial.print(",MAX_REC,");Serial.print(d.maxRec);
    Serial.print(",MAX_REC_AT_ms,");printTimeOrNA(d.maxRecAt);
    Serial.print(",LAST_TEC_INCREASE_ms,");printTimeOrNA(d.lastTecIncreaseAt);
    Serial.print(",LAST_REC_INCREASE_ms,");printTimeOrNA(d.lastRecIncreaseAt);
    Serial.print(",LAST_RECOVERY_ms,");printTimeOrNA(d.lastZeroRecoveryAt);
    Serial.print(",EPISODES,");Serial.println(d.episodes);
  }
}

// STOP is best effort if CAN is broken; FlexCAN write acceptance != motor ACK.
void stopTest(const char *reason) {
  if(phase==IDLE) return;
  recordEvent("STOP_REQUEST");
  bool z1=true,z2=true,e1=true,e2=true;
  if(mitEnableIssued) {
    z1=sendZeroTorque(0);z2=sendZeroTorque(1);
    e1=sendSpecial(0,0xFD);e2=sendSpecial(1,0xFD);
  }
  phase=IDLE;
  mitEnableIssued=false;
  sampleCan(0);sampleCan(1);saveTrace(true);
  recordEvent("STOP_COMPLETE");
  Serial.print("STOP,");Serial.println(reason);
  Serial.print("STOP_TX_ACCEPTED,ZERO_M1,");Serial.print(z1);
  Serial.print(",ZERO_M2,");Serial.print(z2);
  Serial.print(",EXIT_M1,");Serial.print(e1);
  Serial.print(",EXIT_M2,");Serial.println(e2);
  printStatus();printSummary();
  Serial.println("TIP: EVENTS=milestones, PLOT=first-2s ASCII, DUMP=full CSV after stop");
}

void beginRunning() {
  phase=RUNNING;
  phaseBeginUs=runBeginUs=micros();
  firstPeriodicTxUs=0;
  firstPeriodicMotorTx[0]=firstPeriodicMotorTx[1]=false;
  motor[0].hasFeedbackAfterRun=false;
  motor[1].hasFeedbackAfterRun=false;
  txTimer1=0;txTimer2=0;
  lastPrintMs=millis();
  recordEvent("PERIODIC_PHASE_START");
}
void beginMit() {
  if(phase!=PRE_WAIT) return;
  recordEvent("MIT_ENTER_REQUEST");
  mitEnableIssued=true;
  const bool e1=sendSpecial(0,0xFC);
  recordEvent(e1?"MIT_ENTER_M1_TX_ACCEPTED":"MIT_ENTER_M1_TX_FAILED",1);
  const bool e2=sendSpecial(1,0xFC);
  recordEvent(e2?"MIT_ENTER_M2_TX_ACCEPTED":"MIT_ENTER_M2_TX_FAILED",2);
  if(!e1||!e2) {stopTest("MIT_ENABLE_TX_FAILED");return;}
  phase=POST_WAIT;
  phaseBeginUs=micros();
  recordEvent("POST_WAIT_START");
  if(postWaitMs==0) beginRunning();
}
void startTest() {
  if(phase!=IDLE) {Serial.println("ERROR,ALREADY_ACTIVE");return;}
  motor[0]=MotorStats{};motor[1]=MotorStats{};
  eventCount=0;eventDropped=0;traceCount=0;traceDropped=0;
  pendingPeakTec[0]=pendingPeakTec[1]=0;
  pendingPeakRec[0]=pendingPeakRec[1]=0;
  startUs=micros();phaseBeginUs=startUs;
  lastPollUs=startUs;lastTraceUs=0;
  firstPeriodicTxUs=0;runBeginUs=0;mitEnableIssued=false;
  phase=PRE_WAIT;dataAvailable=true;
  sampleCan(0);sampleCan(1);saveTrace(true);
  recordEvent("S_COMMAND");
  if(preWaitMs==0) beginMit();
  Serial.print("START,S_SINCE_BOOT_ms,");Serial.print((startUs-bootUs)/1000.0f,3);
  Serial.print(",PERIOD_US,");Serial.print(periodUs);
  Serial.print(",PRE_MS,");Serial.print(preWaitMs);
  Serial.print(",POST_MS,");Serial.println(postWaitMs);
}

// Jia's service order: receive first, then elapsedMicros TX in loop().
void serviceMotor(uint8_t i) {
  readMotor(i);
  if(phase!=RUNNING) return;
  if(i==0) {
    if(txTimer1<periodUs) return;
    txTimer1=0;
  } else {
    if(txTimer2<periodUs) return;
    txTimer2=0;
  }
  if(!firstPeriodicTxUs) {firstPeriodicTxUs=micros();recordEvent("FIRST_PERIODIC_TX");}
  if(!firstPeriodicMotorTx[i]) {
    firstPeriodicMotorTx[i]=true;
    recordEvent("FIRST_PERIODIC_TX_MOTOR",i+1);
  }
  const bool ok=sendZeroTorque(i);
  if(ok) ++motor[i].txOk;
  else {++motor[i].txFailed;recordEvent("PERIODIC_TX_FAILED",i+1);}
}

void printEvents() {
  if(!dataAvailable) {Serial.println("NO_DATA");return;}
  Serial.println("EVENTS_BEGIN");
  Serial.println("EVENT,time_ms,phase,bus,name,tec,rec,esr1_hex");
  for(uint16_t n=0;n<eventCount;++n) {
    const EventRow &e=events[n];
    Serial.print("EVENT,");Serial.print(e.tUs/1000.0f,3);
    Serial.print(',');Serial.print(phaseName((Phase)e.phase));
    Serial.print(',');Serial.print(e.motor);
    Serial.print(',');Serial.print(e.name);
    Serial.print(',');Serial.print(e.tec);
    Serial.print(',');Serial.print(e.rec);
    Serial.print(",0x");Serial.println(e.esr1,HEX);
  }
  Serial.print("EVENTS_END,DROPPED,");Serial.println(eventDropped);
}

// Human-readable overview of the first 2 seconds; maxima are measured from
// 250-us hardware polls, NOT just from the 1-ms trace-row timestamps.
void printAsciiPlot() {
  if(!dataAvailable) {Serial.println("NO_DATA");return;}
  Serial.println("PLOT_BEGIN,0_TO_2000_MS,BIN_25_MS");
  Serial.println("BIN_ms,M1_TEC_PEAK,M1_REC_PEAK,M2_TEC_PEAK,M2_REC_PEAK");
  for(uint32_t bin=0;bin<2000;++bin) {
    if(bin%25)continue;
    uint8_t peaks[4]={0,0,0,0};
    for(uint16_t k=0;k<traceCount;++k) {
      const TraceRow &r=traceRows[k];
      if(r.tUs/1000<bin || r.tUs/1000>=bin+25) continue;
      peaks[0]=max(peaks[0],r.peakTec[0]);
      peaks[1]=max(peaks[1],r.peakRec[0]);
      peaks[2]=max(peaks[2],r.peakTec[1]);
      peaks[3]=max(peaks[3],r.peakRec[1]);
    }
    Serial.print(bin);Serial.print(',');
    Serial.print(peaks[0]);Serial.print(',');Serial.print(peaks[1]);
    Serial.print(',');Serial.print(peaks[2]);Serial.print(',');Serial.println(peaks[3]);
  }
  Serial.println("PLOT_END");
  Serial.println("PLOT_HEATMAP,each_symbol=25ms,0=none,1=1to31,2=32to95,3=96to127,4=128plus");
  for(uint8_t channel=0;channel<4;++channel) {
    Serial.print(channel==0?"M1_TEC ":channel==1?"M1_REC ":channel==2?"M2_TEC ":"M2_REC ");
    for(uint32_t bin=0;bin<2000;bin+=25) {
      uint8_t peak=0;
      for(uint16_t k=0;k<traceCount;++k) {
        const TraceRow &r=traceRows[k];
        if(r.tUs/1000<bin || r.tUs/1000>=bin+25) continue;
        const uint8_t value=channel==0?r.peakTec[0]:channel==1?r.peakRec[0]:
                            channel==2?r.peakTec[1]:r.peakRec[1];
        peak=max(peak,value);
      }
      Serial.print(peak>=128?'4':peak>=96?'3':peak>=32?'2':peak?'1':'.');
    }
    Serial.println();
  }
  Serial.println("TIP: Run EVENTS for event times; DUMP for time-series CSV");
}

// Dump after motor stops, to prevent Serial throughput affecting TX timing.
void printCsvDump() {
  if(!dataAvailable) {Serial.println("NO_DATA");return;}
  printSummary();printEvents();
  Serial.println("CSV_BEGIN,TRACE");
  Serial.println("time_ms,phase,M1_TEC,M1_REC,M2_TEC,M2_REC,M1_TEC_PEAK,M1_REC_PEAK,M2_TEC_PEAK,M2_REC_PEAK");
  for(uint16_t n=0;n<traceCount;++n) {
    const TraceRow &r=traceRows[n];
    Serial.print(r.tUs/1000.0f,3);Serial.print(',');
    Serial.print(phaseName((Phase)r.phase));Serial.print(',');
    Serial.print(r.tec[0]);Serial.print(',');Serial.print(r.rec[0]);Serial.print(',');
    Serial.print(r.tec[1]);Serial.print(',');Serial.print(r.rec[1]);Serial.print(',');
    Serial.print(r.peakTec[0]);Serial.print(',');Serial.print(r.peakRec[0]);Serial.print(',');
    Serial.print(r.peakTec[1]);Serial.print(',');Serial.println(r.peakRec[1]);
  }
  Serial.println("CSV_END,TRACE");
}

bool parseNumber(const char *s, uint32_t &v) {
  if(!s||!*s||*s=='-')return false;
  char *end;
  const unsigned long n=strtoul(s,&end,10);
  if(end==s||*end!='\0')return false;
  v=(uint32_t)n;return true;
}
void handleCommand(char *cmd) {
  for(char *p=cmd;*p;++p)*p=(char)toupper((unsigned char)*p);
  if(!strcmp(cmd,"S"))startTest();
  else if(!strcmp(cmd,"X"))stopTest("USER");
  else if(!strcmp(cmd,"STATUS")) {if(dataAvailable&&phase!=IDLE){sampleCan(0);sampleCan(1);}printStatus();}
  else if(!strcmp(cmd,"EVENTS") && phase==IDLE)printEvents();
  else if(!strcmp(cmd,"PLOT") && phase==IDLE)printAsciiPlot();
  else if(!strcmp(cmd,"DUMP") && phase==IDLE)printCsvDump();
  else if(!strncmp(cmd,"PRE ",4)||!strncmp(cmd,"POST ",5)||!strncmp(cmd,"PERIOD ",7)) {
    if(phase!=IDLE){Serial.println("ERROR,CHANGE_REQUIRES_IDLE");return;}
    const bool pre=!strncmp(cmd,"PRE ",4),post=!strncmp(cmd,"POST ",5);
    uint32_t value;
    if(!parseNumber(cmd+(pre?4:post?5:7),value)) {Serial.println("ERROR,BAD_NUMBER");return;}
    if((pre||post) && value>5000) {Serial.println("ERROR,WAIT_RANGE_0_TO_5000_MS");return;}
    if(!pre&&!post && (value<250||value>20000)) {Serial.println("ERROR,PERIOD_RANGE_250_TO_20000_US");return;}
    if(pre)preWaitMs=value;
    else if(post)postWaitMs=value;
    else periodUs=value;
    Serial.print("CONFIG,PERIOD_US,");Serial.print(periodUs);
    Serial.print(",PRE_MS,");Serial.print(preWaitMs);
    Serial.print(",POST_MS,");Serial.println(postWaitMs);
  } else Serial.println("COMMANDS: S, X, STATUS, PRE <ms>, POST <ms>, PERIOD <us>, EVENTS, PLOT, DUMP");
}
void readSerial() {
  static char line[40];static uint8_t used=0;
  while(Serial.available()) {
    const char c=(char)Serial.read();
    if(c=='\r'||c=='\n') {
      if(used){line[used]=0;handleCommand(line);used=0;}
    } else if(used<sizeof(line)-1) line[used++]=c;
  }
}

void setup() {
  bootUs=micros();
  Serial.begin(115200);
  initBeginUs=micros();
  Can1.begin();Can1.setBaudRate(CAN_BAUD);
  Can2.begin();Can2.setBaudRate(CAN_BAUD);
  initEndUs=micros();
  Serial.println("READY,CAN_debug_AK80_JiaCAN,TIMELINE_V2");
  Serial.print("BOOT,CAN_INIT_BEGIN_BOOT_ms,");Serial.print((initBeginUs-bootUs)/1000.0f,3);
  Serial.print(",CAN_INIT_END_BOOT_ms,");Serial.println((initEndUs-bootUs)/1000.0f,3);
  Serial.print("CONFIG,CAN_BAUD,");Serial.print(CAN_BAUD);
  Serial.print(",PERIOD_US,");Serial.print(periodUs);
  Serial.print(",PRE_MS,");Serial.print(preWaitMs);
  Serial.print(",POST_MS,");Serial.println(postWaitMs);
  Serial.println("M1=CAN1/ID1,M2=CAN2/ID2,Jia_Mode1_polling,MIT_ZERO_TORQUE");
  Serial.println("COMMANDS:S X STATUS PRE <ms> POST <ms> PERIOD <us> EVENTS PLOT DUMP");
  Serial.println("CAUTION: POST wait leaves MIT enabled without periodic commands; use only on unloaded safe bench.");
  Serial.println("CAUTION: CAN stop is best effort if the bus is broken.");
}

void loop() {
  readSerial();
  serviceMotor(0); // receive before TX, as in Jia's Mode1
  serviceMotor(1);
  if(phase==IDLE)return;

  pollCanAndTrace();
  if(motor[0].busOffSeen||motor[1].busOffSeen) {stopTest("CAN_BUS_OFF");return;}
  if(motor[0].motorError||motor[1].motorError) {stopTest("MOTOR_ERROR");return;}
  if(motor[0].txFailed||motor[1].txFailed) {stopTest("TX_FAILED");return;}
  const uint32_t now=micros();
  if(phase==PRE_WAIT) {
    if((uint32_t)(now-phaseBeginUs)>=preWaitMs*1000UL) beginMit();
    return;
  }
  if(phase==POST_WAIT) {
    if((uint32_t)(now-phaseBeginUs)>=postWaitMs*1000UL) beginRunning();
    return;
  }
  if(phase!=RUNNING)return;
  for(uint8_t i=0;i<2;++i) {
    const MotorStats &d=motor[i];
    const uint32_t age=d.hasFeedbackAfterRun ? now-d.lastRxUs : now-runBeginUs;
    if(age>=FEEDBACK_TIMEOUT_US) {stopTest(i==0?"M1_RX_TIMEOUT":"M2_RX_TIMEOUT");return;}
  }
  if((uint32_t)(now-runBeginUs)>=TEST_DURATION_MS*1000UL) {
    stopTest("TEST_COMPLETE");return;
  }
  if((uint32_t)(millis()-lastPrintMs)>=PRINT_INTERVAL_MS) {
    lastPrintMs=millis();
    Serial.print("TIME_s,");Serial.println((now-runBeginUs)/1000000.0f,2);
    printStatus();
  }
}
