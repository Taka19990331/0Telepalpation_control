#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <IntervalTimer.h>
#include <SdFat.h>
#include <math.h>
#include <string.h>
#include "constvel_config.h"

#if !defined(ARDUINO_TEENSY41)
#error "Select Teensy 4.1 with built-in SD socket"
#endif

constexpr float P_MIN=-12.5f, P_MAX=12.5f, V_MIN=-37.5f, V_MAX=37.5f;
constexpr float T_MIN=-32.0f, T_MAX=32.0f, KP_MIN=0.0f, KP_MAX=500.0f, KD_MIN=0.0f, KD_MAX=5.0f;
constexpr uint8_t ISR_PRIORITY=192;
FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> Can2;
IntervalTimer controlTimer;
SdFs sd;

struct Feedback { float position, velocity, torque; uint32_t rxUs; uint8_t error; bool valid; };
struct Sample { uint32_t timestampUs; float command, position, velocity, torque, iTorque; uint8_t stage, error, valid, reserved; };
struct Header {
  char magic[8]; uint32_t version, periodUs, motorId, canBaud, rowBytes, rows;
  float kp, kd, ki, maxITorque, maxSpeed, targetSpeed;
  uint32_t holdDurationUs;
};
struct Footer { char magic[8]; uint32_t rows, status; };
static_assert(sizeof(Sample)==28, "Update CSV exporter if Sample changes");
static_assert(sizeof(Header)==60, "Update CSV exporter if Header changes");

// Stage is 0 for the constant-speed hold and 1 for the stopped/finalized state.
DMAMEM Sample ring[RING_SIZE];
volatile uint32_t head=0, tail=0, sampleIndex=0;
volatile bool active=false, startRequested=false, waitingForFeedback=false, overrun=false;
volatile uint32_t waitStartedUs=0;
volatile uint8_t stage=0;
volatile uint32_t runStartUs=0;
volatile uint32_t statusCode=0;
volatile float iTorque=0;
Feedback feedback={};
FsFile logFile;
char currentFilename[16]={}, lastFilename[16]={};
uint32_t savedRows=0;
bool cardReady=false, timerReady=false, canReady=false, recording=false;
volatile bool finalizePending=false;
uint32_t txFailures=0;

float uintToFloat(uint32_t x,float lo,float hi,uint8_t bits) { return float(x)*(hi-lo)/float((1UL<<bits)-1)+lo; }
uint32_t floatToUint(float x,float lo,float hi,uint8_t bits) {
  x=fmaxf(lo,fminf(hi,x)); return uint32_t((x-lo)*float((1UL<<bits)-1)/(hi-lo));
}
void receiveFrame(const CAN_message_t &m) {
  if (m.flags.extended || m.flags.remote || m.len!=8 || m.buf[0]!=MOTOR_ID) return;
  const uint32_t p=(uint32_t(m.buf[1])<<8)|m.buf[2];
  const uint32_t v=(uint32_t(m.buf[3])<<4)|(m.buf[4]>>4);
  const uint32_t t=((uint32_t(m.buf[4])&15)<<8)|m.buf[5];
  feedback.position=uintToFloat(p,P_MIN,P_MAX,16);
  feedback.velocity=uintToFloat(v,V_MIN,V_MAX,12);
  feedback.torque=uintToFloat(t,T_MIN,T_MAX,12);
  feedback.error=m.buf[7]; feedback.rxUs=micros(); feedback.valid=true;
}
bool sendSpecial(uint8_t code) {
  CAN_message_t m={}; m.id=MOTOR_ID; m.len=8;
  for (uint8_t i=0;i<7;i++) m.buf[i]=0xff; m.buf[7]=code;
  if (!Can2.write(m)) { ++txFailures; return false; } return true;
}
bool sendMit(float velocity,float torque) {
  const uint32_t p=floatToUint(0,P_MIN,P_MAX,16), v=floatToUint(velocity,V_MIN,V_MAX,12);
  const uint32_t kp=floatToUint(MIT_KP,KP_MIN,KP_MAX,12), kd=floatToUint(MIT_KD,KD_MIN,KD_MAX,12);
  const uint32_t t=floatToUint(torque,T_MIN,T_MAX,12);
  CAN_message_t m={}; m.id=MOTOR_ID; m.len=8;
  m.buf[0]=p>>8; m.buf[1]=p; m.buf[2]=v>>4; m.buf[3]=((v&15)<<4)|(kp>>8);
  m.buf[4]=kp; m.buf[5]=kd>>4; m.buf[6]=((kd&15)<<4)|(t>>8); m.buf[7]=t;
  if (!Can2.write(m)) { ++txFailures; return false; } return true;
}
void timerISR() {
  const uint32_t now=micros();
  if (!active) return;
  if (startRequested) {
    startRequested=false; waitingForFeedback=true; waitStartedUs=now;
  }
  if (head-tail>=RING_SIZE) { overrun=true; statusCode=2; active=false; finalizePending=true; sendMit(0,0); sendSpecial(0xfd); return; }
  if (waitingForFeedback) {
    if (feedback.valid && uint32_t(now-feedback.rxUs)<=FEEDBACK_TIMEOUT_US) {
      if (feedback.error) { statusCode=4; active=false; waitingForFeedback=false; finalizePending=true; sendMit(0,0); sendSpecial(0xfd); return; }
      waitingForFeedback=false; runStartUs=now; stage=0; iTorque=0;
    } else {
      if (uint32_t(now-waitStartedUs)>=START_TIMEOUT_US) {
        statusCode=3; active=false; waitingForFeedback=false; finalizePending=true; sendMit(0,0); sendSpecial(0xfd); return;
      }
      sendMit(0,0); return;
    }
  }
  if (!feedback.valid || uint32_t(now-feedback.rxUs)>FEEDBACK_TIMEOUT_US) {
    statusCode=3; active=false; finalizePending=true; sendMit(0,0); sendSpecial(0xfd); return;
  }
  if (feedback.error) { statusCode=4; active=false; finalizePending=true; sendMit(0,0); sendSpecial(0xfd); return; }
  if (!isfinite(feedback.velocity) || fabsf(feedback.velocity)>MAX_ABS_SPEED_RAD_S) {
    statusCode=5; active=false; finalizePending=true; sendMit(0,0); sendSpecial(0xfd); return;
  }
  if (uint32_t(now-runStartUs)>=HOLD_DURATION_US) {
    stage=1; active=false; finalizePending=true; sendMit(0,0); sendSpecial(0xfd); return;
  }
  const float cmd = TARGET_SPEED_RAD_S;
  const float dt = CONTROL_PERIOD_US * 1e-6f;
  const float error = cmd - feedback.velocity;

  // Integral term
  iTorque += SPEED_KI_NM_PER_RAD * error * dt;
  iTorque = fmaxf(-MAX_I_TORQUE_NM,
            fminf(MAX_I_TORQUE_NM, iTorque));

  // Proportional + Integral
  const float pTorque = SPEED_KP_NM_PER_RAD_S * error;
  const float torqueCmd = pTorque + iTorque;

  // MIT torque command
  if (!sendMit(0.0f, torqueCmd)) {
      statusCode = 6;
      active = false;
      finalizePending = true;
      sendMit(0, 0);
      sendSpecial(0xfd);
      return;
  }

bool openNewLog() {
  bool found=false;
  for (uint16_t n=0;n<10000;n++) {
    snprintf(currentFilename,sizeof(currentFilename),"CV%04u.BIN",n);
    if (!sd.exists(currentFilename)) { found=true; break; }
  }
  if (!found || !logFile.open(currentFilename,O_RDWR|O_CREAT|O_EXCL)) return false;
  Header h={{'C','O','N','S','T','V','L','1'},1,CONTROL_PERIOD_US,MOTOR_ID,CAN_BAUD,sizeof(Sample),0,
    MIT_KP,MIT_KD,SPEED_KI_NM_PER_RAD,MAX_I_TORQUE_NM,MAX_ABS_SPEED_RAD_S,TARGET_SPEED_RAD_S,HOLD_DURATION_US};
  if (!logFile.preAllocate(uint64_t(sizeof(Header))+uint64_t(PREALLOCATE_ROWS)*sizeof(Sample)+sizeof(Footer)) ||
      logFile.write(&h,sizeof(h))!=sizeof(h) || !logFile.sync()) { logFile.close(); return false; }
  strcpy(lastFilename,currentFilename); return true;
}
void startRun() {
  if (recording || active || !cardReady || !timerReady || !canReady) return;
  if (!isfinite(TARGET_SPEED_RAD_S) || fabsf(TARGET_SPEED_RAD_S)>MAX_ABS_SPEED_RAD_S ||
      fabsf(TARGET_SPEED_RAD_S)>V_MAX || HOLD_DURATION_US==0 ||
      HOLD_DURATION_US>PREALLOCATE_ROWS*CONTROL_PERIOD_US) return;
  noInterrupts(); head=tail=sampleIndex=0; active=false; waitingForFeedback=false; startRequested=true;
  overrun=false; statusCode=0; finalizePending=false; interrupts();
  savedRows=0; txFailures=0; feedback.valid=false;
  if (!openNewLog()) { startRequested=false; return; }
  recording=true;
  sendSpecial(0xfc); sendMit(0,0);
  active=true;
}
void serviceLog() {
  if (recording) {
    const uint32_t available=head-tail;
    if (available>=WRITE_BATCH_ROWS || (finalizePending && available)) {
      uint32_t count=available<WRITE_BATCH_ROWS?available:WRITE_BATCH_ROWS;
      const uint32_t offset=tail%RING_SIZE;
      if (count>RING_SIZE-offset) count=RING_SIZE-offset;
      const size_t bytes=size_t(count)*sizeof(Sample);
      if (logFile.write(&ring[offset],bytes)!=bytes) {
        noInterrupts(); statusCode=7; active=false; head=tail; finalizePending=true; interrupts();
      } else { __asm__ volatile("dmb" ::: "memory"); tail+=count; savedRows+=count; }
    }
    if (finalizePending && tail==head) {
      Header h={};
      bool ok=logFile.seekSet(0) && logFile.read(&h,sizeof(h))==sizeof(h);
      if (ok) {
        h.rows=savedRows;
        const Footer f={{'E','N','D','C','V','0','0','1'},savedRows,statusCode};
        ok=logFile.seekSet(sizeof(Header)+uint64_t(savedRows)*sizeof(Sample)) &&
          logFile.write(&f,sizeof(f))==sizeof(f) && logFile.truncate() && logFile.seekSet(0) &&
          logFile.write(&h,sizeof(h))==sizeof(h) && logFile.sync();
      }
      logFile.close(); recording=false; finalizePending=false;
      if (!ok) statusCode=8;
    }
  }
}
void outputCsv() {
  if (recording || !lastFilename[0]) return;
  FsFile f;
  if (!f.open(lastFilename,O_RDONLY)) return;
  Header h; Footer end;
  bool ok=f.fileSize()>=sizeof(Header)+sizeof(Footer) && f.read(&h,sizeof(h))==sizeof(h) &&
    memcmp(h.magic,"CONSTVL1",8)==0 && h.rowBytes==sizeof(Sample);
  if (ok) ok=f.seekSet(f.fileSize()-sizeof(Footer)) && f.read(&end,sizeof(end))==sizeof(end) &&
    memcmp(end.magic,"ENDCV001",8)==0 && end.rows==h.rows && f.fileSize()==sizeof(Header)+uint64_t(h.rows)*sizeof(Sample)+sizeof(Footer);
  if (!ok || !f.seekSet(sizeof(Header))) { f.close(); return; }
  Serial.print("CONSTVEL_BEGIN,"); Serial.print(lastFilename); Serial.print(','); Serial.println(h.rows);
  Serial.print("#format,ConstVel1\n#sampling_hz,"); Serial.print(1000000.0f/h.periodUs,3);
  Serial.print("\n#period_us,"); Serial.print(h.periodUs); Serial.print("\n#motor_id,"); Serial.print(h.motorId);
  Serial.print("\n#can_bitrate,"); Serial.print(h.canBaud); Serial.print("\n#kp,"); Serial.print(h.kp,6);
  Serial.print("\n#kd,"); Serial.print(h.kd,6); Serial.print("\n#speed_ki_nm_per_rad,"); Serial.print(h.ki,6);
  Serial.print("\n#max_i_torque_nm,"); Serial.print(h.maxITorque,6); Serial.print("\n#max_abs_speed_rad_s,"); Serial.print(h.maxSpeed,6);
  Serial.print("\n#target_speed_rad_s,"); Serial.print(h.targetSpeed,6); Serial.print("\n#hold_duration_us,"); Serial.print(h.holdDurationUs);
  Serial.print("\n#result_code,"); Serial.println(end.status);
  Serial.println("timestamp_us,command_rad_s,position_rad,velocity_rad_s,torque_estimate_nm,i_torque_command_nm,stage,error_code,feedback_valid");
  for (uint32_t i=0;i<h.rows;i++) {
    Sample r; if (f.read(&r,sizeof(r))!=sizeof(r)) break;
    Serial.print(r.timestampUs); Serial.print(','); Serial.print(r.command,6); Serial.print(',');
    Serial.print(r.position,6); Serial.print(','); Serial.print(r.velocity,6); Serial.print(',');
    Serial.print(r.torque,6); Serial.print(','); Serial.print(r.iTorque,6); Serial.print(',');
    Serial.print(r.stage); Serial.print(','); Serial.print(r.error); Serial.print(','); Serial.println(r.valid);
  }
  Serial.print("CONSTVEL_END,"); Serial.print(lastFilename); Serial.print(','); Serial.println(h.rows);
  f.close();
}
void findLatestLog() {
  if (!cardReady) return;
  for (int n=9999;n>=0;--n) {
    snprintf(currentFilename,sizeof(currentFilename),"CV%04d.BIN",n);
    if (sd.exists(currentFilename)) { strcpy(lastFilename,currentFilename); return; }
  }
}
void setup() {
  Serial.begin(115200);
  Can2.begin(); Can2.setBaudRate(CAN_BAUD); Can2.setMaxMB(16);
  Can2.setMB(MB0,RX,STD); Can2.setMBFilter(REJECT_ALL); Can2.setMBFilter(MB0,MOTOR_ID);
  Can2.onReceive(MB0,receiveFrame); Can2.enableMBInterrupt(MB0);
  NVIC_SET_PRIORITY(IRQ_CAN2,ISR_PRIORITY); canReady=true;
  cardReady=sd.begin(SdioConfig(FIFO_SDIO));
  findLatestLog();
  controlTimer.priority(ISR_PRIORITY);
  timerReady=controlTimer.begin(timerISR,CONTROL_PERIOD_US);
}
void loop() {
  while (Serial.available()) {
    const char c=Serial.read();
    if ((c=='s'||c=='S') && !recording && !active) startRun();
    else if (c=='d'||c=='D') outputCsv();
    else if ((c=='x'||c=='X') && active) { noInterrupts(); statusCode=1; active=false; finalizePending=true; interrupts(); sendMit(0,0); sendSpecial(0xfd); }
  }
  serviceLog();
}
