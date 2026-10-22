#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <math.h>
#include <IntervalTimer.h>
// Select the wired motor here, then rebuild/upload.
constexpr char MOTOR_SELECT[] = "80B"; // "80A", "80B", "60"
constexpr bool sameText(const char *a, const char *b) {
  return *a == *b && (*a == '\0' || sameText(a+1,b+1));
}
constexpr bool IS_80A=sameText(MOTOR_SELECT,"80A");
constexpr bool IS_80B=sameText(MOTOR_SELECT,"80B");
constexpr bool IS_60=sameText(MOTOR_SELECT,"60");
static_assert(IS_80A || IS_80B || IS_60,"Select 80A, 80B or 60");
FlexCAN_T4<IS_60 ? CAN3 : (IS_80A ? CAN1 : CAN2), RX_SIZE_256, TX_SIZE_16> Can0;
constexpr uint32_t MOTOR_ID=IS_60 ? 3 : (IS_80A ? 1 : 2);
// Match sys_identification/chirp_simplified_test1 ranges.
const float P_MIN=IS_60 ? -12.56f : -12.5f, P_MAX=-P_MIN;
const float V_MIN=IS_60 ? -60.0f : -37.5f, V_MAX=-V_MIN;
const float T_MIN=IS_60 ? -12.0f : -32.0f, T_MAX=-T_MIN;
const float KP_MIN=0, KP_MAX=500, KD_MIN=0, KD_MAX=5;
const float TEST_TORQUE_NM = 0.5f;
const float SPEED_STOP_RAD_S = 40.0f;
const uint32_t RUN_MS = 1000, FEEDBACK_TIMEOUT_MS = 100;
uint32_t txCount=0, startedMs=0, lastRxMs=0, runMs=0;
uint32_t txFailed=0, rxFrames=0, rxAccepted=0;
CAN_message_t lastRxFrame = {};
IntervalTimer controlTimer;
constexpr uint8_t STAGE_COUNT=4;
constexpr uint32_t STAGE_HZ[STAGE_COUNT]={500,1000,2000,3000};
uint8_t stageIndex=0;
float controlPeriodUs=1000000.0f/STAGE_HZ[0];
struct StageDiagnostics {
  uint32_t unmatchedRx=0, rxQueueOverflow=0, hardwareOverruns=0;
  bool started=false, completed=false;
};
StageDiagnostics stageDiagnostics[STAGE_COUNT];
constexpr uint8_t RX_BUDGET = 32;
constexpr uint16_t TIMING_CAPACITY = 8192;
enum TimingStatus : uint8_t { WAITING, OK, TX_FAILED, DEADLINE, CANCELLED };
struct TimingRow {
  uint32_t tick, txUs, rxUs, rttUs, periodUs, executionUs;
  TimingStatus status;
  uint8_t stage;
};
TimingRow timingRows[TIMING_CAPACITY];
static_assert(sizeof(TimingRow)==28, "Check timing RAM budget");
static_assert(TIMING_CAPACITY >= 6500, "Store all four one-second stages");
volatile uint16_t timingCount=0;
volatile int16_t pendingTiming=-1;
volatile bool measurementStarted=false, reportReady=false, reporting=false;
volatile bool responseDeadlineMissed=false;
uint32_t currentPeriodUs=0;

struct ReceivedFrame { CAN_message_t frame; uint32_t receivedMs; };
ReceivedFrame receivedFrames[64];
volatile uint8_t receivedHead=0, receivedTail=0;
void receiveISR(const CAN_message_t &msg);
void printTimingReport();
bool popReceived(CAN_message_t &msg, uint32_t &receivedMs);
bool isMotorFeedback(const CAN_message_t &msg);
volatile uint8_t requestedCommand = 0;
volatile bool stopPending = false;
const char * volatile stopReason = "";
uint32_t tickCount=0, previousTickUs=0, periodMinUs=UINT32_MAX;
uint32_t periodMaxUs=0, isrMaxUs=0, overrunCount=0;
void controlISR();
void foregroundService();

bool active=false, running=false, received=false;
float speedRadS=0, positionRad=0;
float feedbackTorqueNm=NAN, feedbackCurrentA=NAN, feedbackTempC=NAN;
int feedbackRaw=0, feedbackError=-1;
float queuedCommandNm=0, queuedEncodedNm=0;
// queued = accepted by FlexCAN, not an acknowledgement from the motor.
int float_to_uint(float x, float x_min, float x_max, int bits) {
  float span = x_max - x_min;

  if (x > x_max) x = x_max;
  if (x < x_min) x = x_min;

  return (int)((x - x_min) * ((float)((1 << bits) - 1)) / span);
}

float uint_to_float(int x_int, float x_min, float x_max, int bits) {
  float span = x_max - x_min;
  return ((float)x_int) * span / ((float)((1 << bits) - 1)) + x_min;
}

float angleErrorDeg(float targetDeg, float currentDeg) {
  float error = targetDeg - currentDeg;

  while (error > 180.0f) {
    error -= 360.0f;
  }

  while (error < -180.0f) {
    error += 360.0f;
  }

  return error;
}

//=====================
// MIT Mode Commands
//=====================
void sendMitSpecial(uint8_t cmd) {
  if (IS_60) return;
  CAN_message_t msg = {};
  msg.id = MOTOR_ID;
  msg.len = 8;
  msg.flags.extended = 0;

  for (int i = 0; i < 7; i++) {
    msg.buf[i] = 0xFF;
  }
  msg.buf[7] = cmd;

  if (!Can0.write(msg)) txFailed++;
  txCount++;
}

void enterMotorMode() {
  sendMitSpecial(0xFC);
}

void exitMotorMode() {
  sendMitSpecial(0xFD);
}

void setZeroPosition() {
  sendMitSpecial(0xFE);
}

void sendMitCommand(float p_des, float v_des, float kp, float kd, float t_ff, bool measure=false) {
  p_des = constrain(p_des, P_MIN, P_MAX);
  v_des = constrain(v_des, V_MIN, V_MAX);
  kp    = constrain(kp, KP_MIN, KP_MAX);
  kd    = constrain(kd, KD_MIN, KD_MAX);
  t_ff  = constrain(t_ff, T_MIN, T_MAX);

  int p_int  = float_to_uint(p_des, P_MIN, P_MAX, 16);
  int v_int  = float_to_uint(v_des, V_MIN, V_MAX, 12);
  int kp_int = float_to_uint(kp, KP_MIN, KP_MAX, 12);
  int kd_int = float_to_uint(kd, KD_MIN, KD_MAX, 12);
  int t_int  = float_to_uint(t_ff, T_MIN, T_MAX, 12);

  CAN_message_t msg = {};
  msg.id = MOTOR_ID;
  msg.len = 8;
  msg.flags.extended = 0;

  msg.buf[0] = p_int >> 8;
  msg.buf[1] = p_int & 0xFF;
  msg.buf[2] = v_int >> 4;
  msg.buf[3] = ((v_int & 0xF) << 4) | (kp_int >> 8);
  msg.buf[4] = kp_int & 0xFF;
  msg.buf[5] = kd_int >> 4;
  msg.buf[6] = ((kd_int & 0xF) << 4) | (t_int >> 8);
  msg.buf[7] = t_int & 0xFF;
  if (IS_60) {
    msg.id = (8UL << 8) | MOTOR_ID;
    msg.flags.extended = 1;
    msg.buf[0] = kp_int >> 4;
    msg.buf[1] = ((kp_int & 0xF) << 4) | (kd_int >> 8);
    msg.buf[2] = kd_int & 0xFF;
    msg.buf[3] = p_int >> 8;
    msg.buf[4] = p_int & 0xFF;
    msg.buf[5] = v_int >> 4;
    msg.buf[6] = ((v_int & 0xF) << 4) | (t_int >> 8);
    msg.buf[7] = t_int & 0xFF;
  }

  int16_t index=-1;
  if (measure) {
    index=timingCount++;
    TimingRow &r=timingRows[index];
    r.stage=stageIndex; r.tick=tickCount; r.rxUs=0; r.rttUs=0;
    r.periodUs=currentPeriodUs; r.executionUs=0; r.status=WAITING;
    pendingTiming=index;
    // Both ISRs use priority 192: receive cannot preempt this write.
    r.txUs=micros();
  }
  if (!Can0.write(msg)) {
    txFailed++;
    if (index>=0) { timingRows[index].status=TX_FAILED; pendingTiming=-1; }
  }
  else {
    queuedCommandNm=t_ff;
    queuedEncodedNm=uint_to_float(t_int,T_MIN,T_MAX,12);
  }
  txCount++;
}

//=====================

void stopMotor(const char *reason) {
  const bool wasActive=active;
  active=false; running=false;
  if (pendingTiming>=0) {
    timingRows[pendingTiming].status=CANCELLED;
    pendingTiming=-1;
  }
  measurementStarted=false;
  if (wasActive) reportReady=true;
  sendMitCommand(0,0,0,0,0);
  exitMotorMode();
  stopReason=reason;
  stopPending=true;
}
void setup() {
  Serial.begin(115200);
  Can0.begin(); Can0.setBaudRate(1000000);
  // Direct interrupt callbacks: NEVER call Can0.events(), which defers them.
  NVIC_SET_PRIORITY(IS_60 ? IRQ_CAN3 : (IS_80A ? IRQ_CAN1 : IRQ_CAN2), 192);
  Can0.onReceive(receiveISR);
  Can0.enableMBInterrupts();
  sendMitCommand(0,0,0,0,0);
  Serial.print("MOTOR,"); Serial.print(MOTOR_SELECT);
  Serial.print(",CAN,"); Serial.print(IS_60 ? 3 : (IS_80A ? 1 : 2));
  Serial.print(",ID,"); Serial.println(MOTOR_ID);
  exitMotorMode();
  const uint32_t serialWaitMs = millis();
  while (!Serial && millis()-serialWaitMs < 5000) {}
  Serial.print("AK80_TIMER_SEQUENCE_V3,MOTOR,"); Serial.print(MOTOR_SELECT);
  Serial.print(",CAN,"); Serial.print(IS_60 ? 3 : (IS_80A ? 1 : 2));
  Serial.print(",ID,"); Serial.println(MOTOR_ID);
  Serial.print("READY: s=start ");
  Serial.print(TEST_TORQUE_NM, 4);
  Serial.print(" Nm for ");
  Serial.print(RUN_MS / 1000.0f, 3);
  Serial.println(" s/stage, sequence=500/1000/2000/3000 Hz, x=stop");
  Serial.println("MEASURE,RAM,8192,WARMUP_MS_PER_STAGE,30,DEADLINE,ONE_PERIOD");
  controlTimer.priority(192);
  if (!controlTimer.begin(controlISR, controlPeriodUs)) {
    Serial.println("ERROR,TIMER_BEGIN_FAILED");
    while (true) { yield(); }
  }
  while (true) { foregroundService(); yield(); }
}
void controlISR() {
  const uint32_t entryUs=micros();
  if (tickCount) {
    const uint32_t dt=entryUs-previousTickUs; currentPeriodUs=dt;
    if (dt<periodMinUs) periodMinUs=dt;
    if (dt>periodMaxUs) periodMaxUs=dt;
    if (dt>=2*controlPeriodUs) overrunCount++;
  }
  previousTickUs=entryUs; tickCount++;
  const uint8_t command=requestedCommand;
  requestedCommand=0;
  if (command==2) stopMotor("USER");
  if (command==1 && !active && !stopPending && !reportReady && !reporting) {
    // Bounded drain: never wait for incoming traffic inside the ISR.
    receivedTail=receivedHead;
    timingCount=0; pendingTiming=-1; measurementStarted=false;
    responseDeadlineMissed=false;
    stageIndex=0;
    for (uint8_t i=0; i<STAGE_COUNT; ++i) stageDiagnostics[i]=StageDiagnostics{};
    stageDiagnostics[0].started=true;
    controlPeriodUs=1000000.0f/STAGE_HZ[0];
    controlTimer.update(controlPeriodUs);
    feedbackTorqueNm=NAN; feedbackCurrentA=NAN; feedbackTempC=NAN;
    feedbackError=-1; received=false; running=false; active=true;
    startedMs=millis(); txCount=0; txFailed=0; rxFrames=0; rxAccepted=0;

    enterMotorMode();
    if (IS_60) sendMitCommand(0,0,0,0,0);
  }
  CAN_message_t m;
  uint32_t frameMs=0;
  for (uint8_t i=0; i<RX_BUDGET && popReceived(m, frameMs); ++i) {
    if (!active) continue;
    rxFrames++; lastRxFrame=m;
    if (m.flags.remote) continue;
    uint8_t error=0;
    if (IS_60) {
      if (!m.flags.extended || m.id!=((0x29UL<<8)|MOTOR_ID) || m.len!=8) continue;
      const int16_t p=(int16_t)(((uint16_t)m.buf[0]<<8)|m.buf[1]);
      const int16_t v=(int16_t)(((uint16_t)m.buf[2]<<8)|m.buf[3]);
      positionRad=p*0.1f*PI/180.0f;
      // Integrated controller: 14 pole pairs, 6:1 gear ratio.
      speedRadS=v*10.0f/14.0f/6.0f*(2.0f*PI/60.0f);
      feedbackRaw=(int16_t)(((uint16_t)m.buf[4]<<8)|m.buf[5]);
      feedbackCurrentA=feedbackRaw*0.01f;
      feedbackTorqueNm=0.5994f*feedbackCurrentA; // Mode1_Motor3.h KT_NOMINAL
      feedbackTempC=m.buf[6];
      error=m.buf[7];
    } else {
      // Same length/ID acceptance as Integrated Controller Mode1 Motor1/2.
      if (m.flags.extended || m.len < 6 || (m.id != MOTOR_ID && m.buf[0] != MOTOR_ID)) continue;
      positionRad=uint_to_float((m.buf[1]<<8)|m.buf[2],P_MIN,P_MAX,16);
      speedRadS=uint_to_float((m.buf[3]<<4)|(m.buf[4]>>4),V_MIN,V_MAX,12);
      feedbackRaw=((m.buf[4]&0x0F)<<8)|m.buf[5];
      feedbackTorqueNm=uint_to_float(feedbackRaw,T_MIN,T_MAX,12);
      // Same torque interpretation as Mode1_Motor1/2; no verified AK80 Nm/A conversion.
      feedbackCurrentA=NAN;
      feedbackTempC=m.len>=7 ? (float)m.buf[6] : NAN;
      error=m.len>=8 ? m.buf[7] : 0;
    }
    feedbackError=m.len>=8 ? error : -1; // -1: error field absent, not confirmed healthy.
    rxAccepted++; received=true; lastRxMs=frameMs;
    if (error) { stopMotor("MOTOR_ERROR"); break; }
    else if (fabsf(speedRadS)>=SPEED_STOP_RAD_S) { stopMotor("SPEED_LIMIT"); break; }
  }
  if (active && (stageDiagnostics[stageIndex].rxQueueOverflow || stageDiagnostics[stageIndex].hardwareOverruns)) stopMotor("RX_OVERFLOW");
  if (active && responseDeadlineMissed) stopMotor("RESPONSE_DEADLINE");
  if (active && pendingTiming>=0) {
    // Do not pair a late response with a newer command lacking a sequence ID.
    timingRows[pendingTiming].status=DEADLINE;
    pendingTiming=-1;
    stopMotor("RESPONSE_DEADLINE");
  }
  if (active) {
    if ((!received && millis()-startedMs>=FEEDBACK_TIMEOUT_MS) ||
        (received && millis()-lastRxMs>=FEEDBACK_TIMEOUT_MS)) stopMotor("FEEDBACK_TIMEOUT");
    else if (!running && received) {
      running=true; runMs=millis();
      // First nonzero command is sent on this timer tick.
    } else if (running && millis()-runMs>=RUN_MS) {
      stageDiagnostics[stageIndex].completed=true;
      if (stageIndex+1==STAGE_COUNT) stopMotor("SEQUENCE_COMPLETE");
      else {
        stageIndex++;
        stageDiagnostics[stageIndex].started=true;
        measurementStarted=false;
        runMs=millis();
        controlPeriodUs=1000000.0f/STAGE_HZ[stageIndex];
        controlTimer.update(controlPeriodUs);
      }
    }
  }
  // Exclude the first 30 ms of EACH stage, including timer transition.
  if (active && running && millis()-runMs>=30) measurementStarted=true;
  if (active && measurementStarted && timingCount>=TIMING_CAPACITY)
    stopMotor("TIMING_BUFFER_FULL");
  if (active || IS_60) {
    sendMitCommand(0,0,0,0,(active && running) ? TEST_TORQUE_NM : 0,
                   active && measurementStarted);
  }
  if (active && txFailed) stopMotor("TX_FAILED");
  // No periodic Serial logging: keep timing results in RAM until stopped.
  const uint32_t durationUs=micros()-entryUs;
  if (timingCount && timingRows[timingCount-1].tick==tickCount)
    timingRows[timingCount-1].executionUs=durationUs;
  if (durationUs>isrMaxUs) isrMaxUs=durationUs;
  if (durationUs>=controlPeriodUs) overrunCount++;
}
void foregroundService() {
  // STOP takes precedence over START even if both arrive before the next tick.
  for (uint8_t i=0; i<32 && Serial.available(); ++i) {
    const char c=Serial.read();
    noInterrupts();
    if (c=='x' || c=='X') requestedCommand=2;
    else if ((c=='s' || c=='S') && requestedCommand!=2) requestedCommand=1;
    interrupts();
  }
  const char *reason=nullptr;
  noInterrupts();
  if (stopPending) { reason=stopReason; stopPending=false; }
  interrupts();
  if (reason) { Serial.print("STOP,"); Serial.println(reason); }
  bool dump=false;
  noInterrupts();
  if (reportReady) { reportReady=false; reporting=true; dump=true; }
  interrupts();
  if (dump) {
    printTimingReport();
    noInterrupts(); reporting=false; interrupts();
  }
}
// Required by the Arduino runtime, intentionally unused.
void loop() {}

bool isMotorFeedback(const CAN_message_t &m) {
  if (m.flags.remote) return false;
  if (IS_60)
    return m.flags.extended && m.id==((0x29UL<<8)|MOTOR_ID) && m.len==8;
  return !m.flags.extended && m.len>=6 && m.len<=8 &&
         (m.id==MOTOR_ID || m.buf[0]==MOTOR_ID);
}

void receiveISR(const CAN_message_t &m) {
  // First operation: timestamp callback entry before the later control decode.
  const uint32_t rxUs=micros();
  if (!active || !isMotorFeedback(m)) return;
  if (m.flags.overrun) stageDiagnostics[stageIndex].hardwareOverruns++;
  if (measurementStarted) {
    const int16_t index=pendingTiming;
    if (index>=0) {
      TimingRow &r=timingRows[index];
      r.rxUs=rxUs; r.rttUs=rxUs-r.txUs; // unsigned subtraction handles wrap
      r.status=r.rttUs<controlPeriodUs ? OK : DEADLINE;
      if (r.status==DEADLINE) responseDeadlineMissed=true;
      pendingTiming=-1;
    } else stageDiagnostics[stageIndex].unmatchedRx++;
  }
  const uint8_t next=(receivedHead+1)%64;
  if (next==receivedTail) { stageDiagnostics[stageIndex].rxQueueOverflow++; return; }
  receivedFrames[receivedHead].frame=m;
  receivedFrames[receivedHead].receivedMs=millis();
  receivedHead=next;
}

bool popReceived(CAN_message_t &m, uint32_t &receivedMs) {
  if (receivedTail==receivedHead) return false;
  m=receivedFrames[receivedTail].frame;
  receivedMs=receivedFrames[receivedTail].receivedMs;
  receivedTail=(receivedTail+1)%64;
  return true;
}

void printStageReport(uint8_t stage) {
  // active=false and measurementStarted=false; rows stay frozen until end.
  const uint16_t count=timingCount;
  uint32_t good=0, failed=0, deadlines=0, cancelled=0, stageRows=0;
  uint32_t minUs=UINT32_MAX, maxUs=0, maxISR=0, minPeriod=UINT32_MAX, maxPeriod=0;
  double mean=0, m2=0;
  Serial.print("STAGE_BEGIN,Hz,"); Serial.print(STAGE_HZ[stage]);
  Serial.print(",State,");
  Serial.println(stageDiagnostics[stage].completed ? "COMPLETE" :
                 (stageDiagnostics[stage].started ? "ABORTED" : "NOT_RUN"));
  Serial.println("TIMING,Hz,Index,Tick,TxRequest_us,RxCallback_us,RTT_us,Period_us,ISR_us,Status");
  for (uint16_t i=0; i<count; ++i) {
    const TimingRow &r=timingRows[i];
    if (r.stage!=stage) continue;
    const uint32_t localIndex=stageRows++;
    const char *status="CANCELLED";
    if (r.status==OK) {
      status="OK"; good++;
      if (r.rttUs<minUs) minUs=r.rttUs;
      if (r.rttUs>maxUs) maxUs=r.rttUs;
      const double delta=r.rttUs-mean;
      mean+=delta/good; m2+=delta*(r.rttUs-mean);
    } else if (r.status==TX_FAILED) { status="TX_FAILED"; failed++; }
    else if (r.status==DEADLINE) { status="DEADLINE"; deadlines++; }
    else cancelled++;
    if (r.periodUs<minPeriod) minPeriod=r.periodUs;
    if (r.periodUs>maxPeriod) maxPeriod=r.periodUs;
    if (r.executionUs>maxISR) maxISR=r.executionUs;
    char line[192];
    snprintf(line,sizeof(line),"TIMING,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%s\n",
      (unsigned long)STAGE_HZ[stage],(unsigned long)localIndex,(unsigned long)r.tick,(unsigned long)r.txUs,(unsigned long)r.rxUs,
      (unsigned long)r.rttUs,(unsigned long)r.periodUs,
      (unsigned long)r.executionUs,status);
    Serial.print(line);
  }
  char summary[512];
  snprintf(summary,sizeof(summary),
    "TIMING_SUMMARY,Hz,%lu,Count,%lu,OK,%lu,TxFailed,%lu,Deadline,%lu,Cancelled,%lu,RTTMin_us,%lu,RTTMean_us,%.3f,RTTMax_us,%lu,RTTStdDev_us,%.3f,PeriodMin_us,%lu,PeriodMax_us,%lu,ISRMax_us,%lu,UnmatchedRx,%lu,RxQueueOverflow,%lu,HardwareOverruns,%lu\n",
    (unsigned long)STAGE_HZ[stage],(unsigned long)stageRows,(unsigned long)good,(unsigned long)failed,(unsigned long)deadlines,
    (unsigned long)cancelled,(unsigned long)(good ? minUs : 0),good ? mean : NAN,
    (unsigned long)maxUs,good ? sqrt(m2/good) : NAN,
    (unsigned long)(stageRows ? minPeriod : 0),(unsigned long)maxPeriod,
    (unsigned long)maxISR,(unsigned long)stageDiagnostics[stage].unmatchedRx,
    (unsigned long)stageDiagnostics[stage].rxQueueOverflow,(unsigned long)stageDiagnostics[stage].hardwareOverruns);
  Serial.print(summary);
  Serial.println("STAGE_END");
}



void printTimingReport() {
  Serial.println("TIMING_BEGIN,VERSION,AK80_TIMER_SEQUENCE_V3");
  for (uint8_t stage=0; stage<STAGE_COUNT; ++stage) printStageReport(stage);
  Serial.println("TIMING_END");
}

