#include <Arduino.h>
#include <util/atomic.h>
#include <IntervalTimer.h>
#include <SdFat.h>
#include <FlexCAN_T4.h>
#include "friction_core.h"
#include "motor_protocol.h"
#include "config_metadata.h"
#if !defined(ARDUINO_TEENSY41)
#error "Select Teensy 4.1"
#endif
constexpr CAN_DEV_TABLE MOTOR_CAN_BUS=IS_80A?CAN1:CAN2;
FlexCAN_T4<MOTOR_CAN_BUS,RX_SIZE_256,TX_SIZE_16> MotorCan;
constexpr FLEXCAN_MAILBOX TX_MAILBOX=MB8;
constexpr uint32_t TX_BIT=1u<<8;
SdFs sd; FsFile logFile; IntervalTimer controlTimer;
DMAMEM Sample ring[RING_SIZE];
Engine engine; Observation feedback={};
volatile uint32_t head=0,tail=0,rxFault=OK;
volatile bool active=false,finished=false,startRequested=false,userStop=false,ioStop=false;
bool cardReady=false,canReady=false,timerReady=false,recording=false,ioFailed=false;
volatile bool restartLocked=false;
uint32_t firstUs=0,previousUs=0,exitUs=0,exitStage=0,exitAcknowledged=0,startStage=0;
uint32_t highWater=0,maxWriteUs=0,configBytes=0;
char filename[16],configJson[4096];
float simPosition=0,simSpeed=0;
uint32_t lockInterrupts() { uint32_t p=__get_primask(); __disable_irq(); return p; }
void unlockInterrupts(uint32_t p) { if(!p) __enable_irq(); }
bool txIdle() {
  return FLEXCAN_get_code(FLEXCANb_MBn_CS(MOTOR_CAN_BUS, 8)) == FLEXCAN_MB_CODE_TX_INACTIVE;
}
bool sendFrame(const CAN_message_t &frame) {
  if (NO_MOTOR_TEST) return false; // No CAN write at all in test mode.
  // Installed FlexCAN_T4::write(MB, msg) queues on a busy MB. Precheck the only
  // TX mailbox under the sole timer writer, with seq=false, to avoid that path.
  if (!txIdle() || MotorCan.getTXQueueCount()) return false;
  return MotorCan.write(TX_MAILBOX, frame) != 0;
}
bool sendTorque(float torque, float limit = TORQUE_LIMIT_NM) {
  const MotorFrame frame=torqueFrame(torque, limit);
  CAN_message_t m = {};
  m.id=frame.id; m.flags.extended=frame.extended; m.len=frame.len;
  memcpy(m.buf,frame.data,8);
  return sendFrame(m);
}
bool sendSpecial(uint8_t code) {
  if (IS_60) return false; // AK60 has no verified AK80 FC/FD commands.
  CAN_message_t m = {}; m.id = MOTOR_ID; m.len = 8;
  for (unsigned i = 0; i < 7; ++i) m.buf[i] = 0xff;
  m.buf[7] = code;
  return sendFrame(m);
}


// Same interrupt priority as timer: feedback snapshots cannot tear.
void receiveISR(const CAN_message_t &m) {
  if(!active) return;
  MotorFrame frame={m.id,m.flags.extended,m.flags.remote,m.flags.overrun,m.len,{}};
  memcpy(frame.data,m.buf,8);
  const MotorFeedback decoded=decodeMotorFeedback(frame);
  if(!decoded.valid) return;
  if(m.flags.overrun) rxFault=CAN_ERROR;
  feedback={decoded.positionRad,decoded.speedRadS,decoded.torqueEstimateNm,
            micros(),feedback.sequence+1,decoded.error};
}
void simulate(uint32_t now) {
  const float torque=engine.target;
  float acceleration=0;
  if(fabsf(simSpeed)>.0001f || fabsf(torque)>.18f)
    acceleration=(torque-.12f*copysignf(1.f,simSpeed==0?torque:simSpeed)-.2f*simSpeed)/.01f;
  float next=simSpeed+acceleration*.0005f;
  if(next*simSpeed<0 && fabsf(torque)<.18f) next=0;
  simSpeed=next; simPosition+=simSpeed*.0005f;
  float wrapped=fmodf(simPosition+12.5f,25.f); if(wrapped<0) wrapped+=25.f;
  feedback={wrapped-12.5f,simSpeed,torque,now,feedback.sequence+1,0};
}
// Bounded, polled mailbox cancellation then zero + FD. ACK is NOT proof of rest.
void serviceExit(uint32_t now) {
  engine.target=0;
  if(NO_MOTOR_TEST) { exitAcknowledged=0; engine.state=FINISHED; return; }
  if(exitStage==0) {
    exitUs=now; exitStage=1;
    if(!txIdle()) FLEXCANb_MBn_CS(MOTOR_CAN_BUS,8)=FLEXCAN_MB_CS_CODE(FLEXCAN_MB_CODE_TX_ABORT);
  }
  if(exitStage==1) {
    uint32_t code=FLEXCAN_get_code(FLEXCANb_MBn_CS(MOTOR_CAN_BUS,8));
    if(code==FLEXCAN_MB_CODE_TX_ABORT && (FLEXCANb_IFLAG1(MOTOR_CAN_BUS)&TX_BIT)) {
      FLEXCANb_IFLAG1(MOTOR_CAN_BUS)=TX_BIT;
      FLEXCANb_MBn_CS(MOTOR_CAN_BUS,8)=FLEXCAN_MB_CS_CODE(FLEXCAN_MB_CODE_TX_INACTIVE);
    }
    if(txIdle() && sendTorque(0)) exitStage=2;
  } else if(exitStage==2 && txIdle()) {
    if(sendSpecial(0xfd)) exitStage=3;
  } else if(exitStage==3 && txIdle()) {
    exitAcknowledged=1; engine.state=FINISHED;
  }
  if(engine.state!=FINISHED && now-exitUs>=EXIT_TIMEOUT_US) {
    if(engine.reason==OK) engine.reason=STOP_FAILED;
    restartLocked=true;
    if(!txIdle()) FLEXCANb_MBn_CS(MOTOR_CAN_BUS,8)=FLEXCAN_MB_CS_CODE(FLEXCAN_MB_CODE_TX_ABORT);
    engine.state=FINISHED;
  }
}
void controlISR() {
  uint32_t now=micros();
  if(startRequested) {
    startRequested=false; active=true; finished=false;
    firstUs=previousUs=now; engine.reset(now); feedback={}; rxFault=OK;
    startStage=exitStage=exitAcknowledged=0; simPosition=simSpeed=0;
  }
  if(!active) return;
  if(NO_MOTOR_TEST) simulate(now);
  uint32_t dt=now-previousUs; previousUs=now;
  engine.event=0;
  // Faults are handled before advancing the protocol, even during stopping.
  if(ioStop) { ioStop=false; engine.stop(SD_ERROR,now); }
  if(userStop) { userStop=false; engine.stop(USER_ABORT,now); }
  if(rxFault!=OK) engine.stop(rxFault,now,false);
  if(head && dt>MAX_CONTROL_GAP_US) engine.stop(CONTROL_DEADLINE,now,false);
  if(now-firstUs>=MAX_RUN_US) engine.stop(RUN_LIMIT,now,false);
  if(!NO_MOTOR_TEST && (FLEXCANb_ESR1(MOTOR_CAN_BUS)&FLEXCAN_ESR_FLT_CONF_MASK)) engine.stop(CAN_ERROR,now,false);
  if(head-tail>=RING_SIZE) engine.stop(RING_FULL,now);
  bool startupSent=false;
  if(!NO_MOTOR_TEST && engine.state==STARTUP && startStage<3) {
    bool sent=startStage==1?sendSpecial(0xfc):sendTorque(0);
    startupSent=sent;
    if(!sent) engine.stop(TX_FAILED,now,false); else ++startStage;
  } else if(engine.state!=EXITING) {
    uint32_t preserved=engine.event;
    engine.tick(now,feedback); engine.event|=preserved;
  }
  uint32_t tx=startupSent;
  if(engine.state!=EXITING && engine.state!=FINISHED) {
    if(!isfinite(engine.target)||fabsf(engine.target)>TORQUE_LIMIT_NM) engine.stop(TORQUE_RANGE,now);
    // Startup stages already submitted a frame this tick.
    if(!NO_MOTOR_TEST && !startupSent) {
      tx=sendTorque(engine.target);
      if(!tx) engine.stop(TX_FAILED,now,false);
    }
  }
  if(engine.state==EXITING) serviceExit(now);
  if(head-tail<RING_SIZE && head<EXPECTED_ROWS) {
    Sample r={}; r.index=head; r.actualUs=now-firstUs; r.phase=engine.phase; r.step=engine.step;
    r.state=engine.state; r.reason=engine.event&2?engine.result:engine.reason; r.event=engine.event;
    r.target=engine.target; r.positionDeg=feedback.position*180.f/PI;
    r.speed=feedback.speed; r.feedbackTorque=feedback.torque;
    r.acceleration=engine.regression.acceleration; r.speedStd=engine.regression.deviation;
    r.rxUs=feedback.sequence?feedback.rxUs-firstUs:0; r.ageUs=feedback.sequence?now-feedback.rxUs:UINT32_MAX;
    r.sequence=feedback.sequence; r.motorError=feedback.error; r.txAccepted=tx; r.periodUs=dt;
    ring[head%RING_SIZE]=r; ring[head%RING_SIZE].executionUs=micros()-now;
    __asm__ volatile("dmb" ::: "memory"); ++head;
  }
  if(micros()-now>=PERIOD_US) engine.stop(CONTROL_DEADLINE,micros(),false);
  if(engine.state==FINISHED) {
    restartLocked=restartLocked || !engine.stopConfirmed || (!NO_MOTOR_TEST&&!exitAcknowledged);
    __asm__ volatile("dmb" ::: "memory");
    active=false; finished=true;
  }
}
bool setupCAN() {
  MotorCan.begin(); MotorCan.setBaudRate(CAN_BITRATE); MotorCan.setMaxMB(16);
  for (unsigned i=0; i<16; ++i) MotorCan.setMB(FLEXCAN_MAILBOX(i), RX, INACTIVE);
  MotorCan.setMB(MB0, RX, IS_60 ? EXT : STD);
  MotorCan.setMBFilter(REJECT_ALL);
  MotorCan.setMB(TX_MAILBOX, TX, STD); // Only one TX mailbox, no delayed command chain.
  // setMB(TX) enables its IRQ; disableMBInterrupt(TX) is a keeper in this
  // library and does NOT clear it. Clear the mask directly for polled aborts.
  FLEXCANb_IMASK1(MOTOR_CAN_BUS) &= ~TX_BIT;
  MotorCan.setMBFilter(MB0, IS_60 ? ((0x29u<<8)|MOTOR_ID) : MOTOR_ID);
  MotorCan.onReceive(MB0, receiveISR); MotorCan.enableMBInterrupt(MB0);
  // TX interrupt stays disabled: controlISR polls completion/abort state.
  NVIC_SET_PRIORITY(IS_60 ? IRQ_CAN3 : (IS_80A ? IRQ_CAN1 : IRQ_CAN2), ISR_PRIORITY);
  // Enable hardware TX-abort support while frozen, with bounded setup waits.
  FLEXCANb_MCR(MOTOR_CAN_BUS) |= FLEXCAN_MCR_FRZ | FLEXCAN_MCR_HALT;
  uint32_t started = micros();
  while (!(FLEXCANb_MCR(MOTOR_CAN_BUS) & FLEXCAN_MCR_FRZ_ACK)) if (micros()-started>10000) return false;
  FLEXCANb_MCR(MOTOR_CAN_BUS) |= FLEXCAN_MCR_AEN;
  FLEXCANb_MCR(MOTOR_CAN_BUS) &= ~FLEXCAN_MCR_HALT;
  started = micros();
  while (FLEXCANb_MCR(MOTOR_CAN_BUS) & FLEXCAN_MCR_FRZ_ACK) if (micros()-started>10000) return false;
  return true;
}


void startRun() {
  if(recording || restartLocked || !cardReady || !timerReady || (!NO_MOTOR_TEST&&!canReady)) return;
  bool found=false;
  for(unsigned n=0;n<10000;++n) {
    snprintf(filename,sizeof(filename),"FR%04u.BIN",n);
    if(!sd.exists(filename)) {found=true; break;}
  }
  if(!found || !logFile.open(filename,O_RDWR|O_CREAT|O_EXCL)) { Serial.println("ERROR,CREATE"); return; }
  configBytes=makeConfig(configJson,sizeof(configJson)); Header h={{'F','R','I','C','0','0','1',0},sizeof(Sample),PERIOD_US,configBytes};
  uint64_t capacity=sizeof(h)+configBytes+uint64_t(EXPECTED_ROWS)*sizeof(Sample)+sizeof(Footer);
  if(!logFile.preAllocate(capacity) || logFile.write(&h,sizeof(h))!=sizeof(h) ||
     logFile.write(configJson,configBytes)!=configBytes || !logFile.sync()) {
    logFile.close(); Serial.println("ERROR,PREPARE"); return;
  }
  uint32_t p=lockInterrupts(); head=tail=0; ioFailed=false; highWater=maxWriteUs=0;
  userStop=ioStop=false; finished=false; recording=true; startRequested=true; unlockInterrupts(p);
  Serial.print("START,"); Serial.println(filename);
}
void serviceLog() {
  if(!recording) return;
  uint32_t available=head-tail;
  __asm__ volatile("dmb" ::: "memory");
  highWater=max(highWater,available);
  if(!ioFailed && available && (available>=BATCH || finished) && !sd.card()->isBusy()) {
    uint32_t rows=min(available,BATCH), offset=tail%RING_SIZE;
    rows=min(rows,RING_SIZE-offset);
    uint32_t before=micros(),bytes=rows*sizeof(Sample);
    // Foreground SdFat write may take time; interrupts remain enabled.
    size_t written=logFile.write(&ring[offset],bytes);
    maxWriteUs=max(maxWriteUs,micros()-before);
    if(written!=bytes) { ioFailed=true; ioStop=true; }
    else { __asm__ volatile("dmb" ::: "memory"); tail+=rows; }
  }
  if(!finished || (!ioFailed && tail!=head)) return;
  __asm__ volatile("dmb" ::: "memory");
  uint32_t rows=tail;
  if(ioFailed && engine.reason==OK) engine.reason=SD_ERROR;
  Footer f={{'E','N','D','F','R','0','0','1'},rows,engine.reason,engine.stopConfirmed,exitAcknowledged};
  bool ok=logFile.seekSet(sizeof(Header)+configBytes+uint64_t(rows)*sizeof(Sample)) &&
    logFile.write(&f,sizeof(f))==sizeof(f) && logFile.truncate() && logFile.sync();
  logFile.close(); recording=false;
  if(!ok || ioFailed) { cardReady=false; restartLocked=true; }
  Serial.print(ok?"SAVED,":"ERROR,FINALIZE,"); Serial.print(filename);
  Serial.print(",Reason,"); Serial.print(engine.reason);
  Serial.print(",RestConfirmed,"); Serial.print(engine.stopConfirmed);
  Serial.print(",ExitACK,"); Serial.print(exitAcknowledged);
  Serial.print(",RingHighWater,"); Serial.print(highWater);
  Serial.print(",WriteMax_us,"); Serial.println(maxWriteUs);
  if(restartLocked) Serial.println("RESTART_LOCKED: STOP NOT CONFIRMED OR STORAGE FAILED");
}
bool downloadLatest() {
  if(recording || active || !cardReady) { Serial.println("ERROR,BUSY_OR_NO_SD"); return false; }
  char source[16]={};
  // Prefer the run created during this boot; after a reset select the newest
  // FRxxxx.BIN without changing or deleting it.
  if(filename[0] && sd.exists(filename)) strncpy(source,filename,sizeof(source)-1);
  else for(int n=9999;n>=0;--n) {
    snprintf(source,sizeof(source),"FR%04d.BIN",n);
    if(sd.exists(source)) break;
    source[0]=0;
  }
  FsFile input;
  if(!source[0] || !input.open(source,O_RDONLY)) { Serial.println("ERROR,NO_SAVED_FILE"); return false; }
  const uint64_t size=input.fileSize();
  Header h; Footer footer;
  bool valid=size>=sizeof(Header)+sizeof(Footer) && input.read(&h,sizeof(h))==sizeof(h) &&
    memcmp(h.magic,"FRIC001\0",8)==0 && h.rowBytes==sizeof(Sample) && h.periodUs==PERIOD_US &&
    h.configBytes<=4096 && size>=sizeof(Header)+h.configBytes+sizeof(Footer) &&
    input.seekSet(size-sizeof(Footer)) && input.read(&footer,sizeof(footer))==sizeof(footer) &&
    memcmp(footer.magic,"ENDFR001",8)==0 && footer.rows<=EXPECTED_ROWS &&
    size==sizeof(Header)+h.configBytes+uint64_t(footer.rows)*sizeof(Sample)+sizeof(Footer);
  if(!valid || !input.seekSet(0)) { input.close(); Serial.println("ERROR,INVALID_LOG"); return false; }
  // Protocol: ASCII size header, exactly size binary bytes, ASCII end marker.
  Serial.print("FRICTION_BIN,"); Serial.print(source); Serial.print(','); Serial.println(uint32_t(size));
  uint8_t buffer[512]; uint64_t remaining=size;
  while(remaining) {
    size_t want=remaining<sizeof(buffer)?size_t(remaining):sizeof(buffer);
    size_t got=input.read(buffer,want);
    if(got!=want) { input.close(); Serial.println("\nFRICTION_ERROR,SD_READ"); return false; }
    size_t sent=0;
    while(sent<got) sent+=Serial.write(buffer+sent,got-sent);
    remaining-=got;
  }
  input.close(); Serial.print("\nFRICTION_END,"); Serial.print(source); Serial.print(','); Serial.println(uint32_t(size));
  Serial.flush(); return true;
}
void setup() {
  Serial.begin(115200);
  if(!NO_MOTOR_TEST) canReady=setupCAN();
  cardReady=sd.begin(SdioConfig(FIFO_SDIO));
  controlTimer.priority(ISR_PRIORITY);
  timerReady=controlTimer.begin(controlISR,PERIOD_US);
  Serial.println(NO_MOTOR_TEST?"FRICTION: DRY RUN (NO CAN WRITES)":"FRICTION: HARDWARE");
  Serial.println(cardReady&&timerReady?"s=start, x=stop, d=download latest FRxxxx.BIN over USB":"INIT FAILED");
}
void loop() {
  for(unsigned n=0;n<32 && Serial.available();++n) {
    char c=Serial.read();
    if(c=='x'||c=='X') userStop=true;
    if((c=='s'||c=='S')&&!userStop) startRun();
    if(c=='d'||c=='D') downloadLatest();
  }
  serviceLog(); // Direct CAN callbacks continue during SD writes; no events().
}

