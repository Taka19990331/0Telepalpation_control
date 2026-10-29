#include <Arduino.h>
#include <SdFat.h>
#include "can_protocol.h"

#if !defined(ARDUINO_TEENSY41)
#error "Select Teensy 4.1 with built-in SD socket"
#endif

// Experiment command: constant feed-forward torque. Edit this value as needed.
volatile float tau_cmd = 0.8f;
constexpr uint32_t RUN_LIMIT_US = 3600000000UL;
constexpr uint32_t RING_SIZE = 4096, BATCH_ROWS = 128;
struct Sample { uint32_t timeUs; float commandNm; uint8_t reserved[4]; };
struct Header { char magic[8]; uint32_t version, periodUs, motorId, canBaud, rowBytes, rows; float torqueNm; };
struct Footer { char magic[8]; uint32_t rows, status; };
static_assert(sizeof(Sample) == 12, "Unexpected sample layout");
DMAMEM Sample samples[RING_SIZE];
volatile uint32_t head=0, tail=0;
volatile bool active=false, startRequest=false, finalizeRequest=false;
volatile uint32_t runStart=0, statusCode=0;
IntervalTimer controlTimer;
SdFs sd;
FsFile logFile;
char currentFile[16] = {}, latestFile[16] = {};
uint32_t savedRows=0;
bool recording=false, cardReady=false;

void controlISR() {
  const uint32_t now=micros();
  if (startRequest) { startRequest=false; runStart=now; active=true; }
  if (!active) return;
  if (uint32_t(now-runStart)>=RUN_LIMIT_US) { active=false; finalizeRequest=true; return; }
  const float torque=tau_cmd;
  if (!CanProtocol::command(0,0,0,0,torque)) { statusCode=2; active=false; finalizeRequest=true; CanProtocol::command(0,0,0,0,0); return; }
  if (head-tail>=RING_SIZE) { statusCode=3; active=false; finalizeRequest=true; CanProtocol::command(0,0,0,0,0); return; }
  Sample &s=samples[head%RING_SIZE]; s.timeUs=now-runStart; s.commandNm=torque;
  ++head;
}
bool openLog() {
  for (uint16_t n=0;n<10000;n++) {
    snprintf(currentFile,sizeof(currentFile),"TC%04u.BIN",n);
    if (!sd.exists(currentFile)) break;
  }
  if (!logFile.open(currentFile,O_RDWR|O_CREAT|O_EXCL)) return false;
  Header h={{'T','R','A','N','S','C','T','1'},1,CanProtocol::PERIOD_US,CanProtocol::MOTOR_ID,CanProtocol::CAN_BAUD,sizeof(Sample),0,tau_cmd};
  if (logFile.write(&h,sizeof(h))!=sizeof(h) || !logFile.sync()) { logFile.close(); return false; }
  strcpy(latestFile,currentFile); return true;
}
void startRun() {
  if (!cardReady || recording || active) return;
  noInterrupts(); head=tail=0; statusCode=0; finalizeRequest=false; interrupts();
  savedRows=0;
  if (!openLog()) return;
  recording=true;
  CanProtocol::enable(); CanProtocol::command(0,0,0,0,0);
  noInterrupts(); startRequest=true; interrupts();
}
void serviceLog() {
  if (!recording) return;
  uint32_t count=head-tail;
  if (count && (count>=BATCH_ROWS || finalizeRequest)) {
    if (count>BATCH_ROWS) count=BATCH_ROWS;
    uint32_t contiguous=RING_SIZE-(tail%RING_SIZE); if(count>contiguous) count=contiguous;
    size_t bytes=size_t(count)*sizeof(Sample);
    if(logFile.write(&samples[tail%RING_SIZE],bytes)!=bytes) { noInterrupts(); statusCode=4; active=false; tail=head; finalizeRequest=true; interrupts(); }
    else { noInterrupts(); tail+=count; interrupts(); savedRows+=count; }
  }
  if(finalizeRequest && tail==head) {
    Header h={}; bool ok=logFile.seekSet(0)&&logFile.read(&h,sizeof(h))==sizeof(h);
    if(ok) { h.rows=savedRows; Footer f={{'E','N','D','T','C','0','0','1'},savedRows,statusCode};
      ok=logFile.seekSet(sizeof(Header)+uint64_t(savedRows)*sizeof(Sample))&&logFile.write(&f,sizeof(f))==sizeof(f)&&logFile.seekSet(0)&&logFile.write(&h,sizeof(h))==sizeof(h)&&logFile.sync(); }
    logFile.close(); recording=false; finalizeRequest=false;
    CanProtocol::command(0,0,0,0,0); CanProtocol::disable();
    Serial.print("SAVED,"); Serial.print(latestFile); Serial.print(",ROWS,"); Serial.print(savedRows); Serial.print(",STATUS,"); Serial.println(ok?statusCode:5);
  }
}
void dumpCsv() {
  if(recording || !latestFile[0]) return;
  FsFile f; if(!f.open(latestFile,O_RDONLY)) return;
  Header h; Footer end;
  bool ok=f.read(&h,sizeof(h))==sizeof(h)&&memcmp(h.magic,"TRANSCT1",8)==0&&h.rowBytes==sizeof(Sample);
  ok=ok&&f.seekSet(sizeof(Header)+uint64_t(h.rows)*sizeof(Sample))&&f.read(&end,sizeof(end))==sizeof(end)&&memcmp(end.magic,"ENDTC001",8)==0&&end.rows==h.rows;
  if(!ok||!f.seekSet(sizeof(Header))) { f.close(); Serial.println("SD_READ_ERROR"); return; }
  Serial.print("TRANS_BEGIN,"); Serial.print(latestFile); Serial.print(','); Serial.println(h.rows);
  Serial.print("#period_us,"); Serial.print(h.periodUs); Serial.print("\n#motor_id,"); Serial.print(h.motorId); Serial.print("\n#torque_nm,"); Serial.println(h.torqueNm,6);
  Serial.println("time_us,command_torque_nm");
  for(uint32_t i=0;i<h.rows;i++){ Sample s; if(f.read(&s,sizeof(s))!=sizeof(s)) break; Serial.print(s.timeUs); Serial.print(','); Serial.println(s.commandNm,6); }
  Serial.print("TRANS_END,"); Serial.print(latestFile); Serial.print(','); Serial.println(h.rows); f.close();
}
void findLatest() { for(int n=9999;n>=0;n--){ snprintf(currentFile,sizeof(currentFile),"TC%04d.BIN",n); if(sd.exists(currentFile)){strcpy(latestFile,currentFile);return;} } }
void setup() {
  Serial.begin(115200); CanProtocol::begin();
  cardReady=sd.begin(SdioConfig(FIFO_SDIO)); if(cardReady) findLatest();
  controlTimer.priority(192); controlTimer.begin(controlISR,CanProtocol::PERIOD_US);
  Serial.print("READY,MOTOR,"); Serial.print(CanProtocol::MOTOR_SELECT); Serial.print(",ID,"); Serial.print(CanProtocol::MOTOR_ID); Serial.print(",SD,"); Serial.println(cardReady?"OK":"ERROR");
}
void loop() {
  while(Serial.available()){char c=Serial.read(); if(c=='s'||c=='S') startRun(); else if(c=='x'||c=='X'){ if(active){noInterrupts(); active=false; finalizeRequest=true; statusCode=1; interrupts(); CanProtocol::command(0,0,0,0,0); CanProtocol::disable();} } else if(c=='d'||c=='D') dumpCsv(); }
  serviceLog();
}
