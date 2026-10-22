#include <Arduino.h>
#include <util/atomic.h>
#include <IntervalTimer.h>
#include <SdFat.h>
#include <FlexCAN_T4.h>
#include "fric_config.h"
#include "motor_protocol.h"

#if !defined(ARDUINO_TEENSY41)
#error "Select Teensy 4.1; this firmware uses its built-in SDIO microSD slot"
#endif

FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> MotorCan;
constexpr FLEXCAN_MAILBOX TX_MAILBOX=MB8;
constexpr uint32_t TX_BIT=1u<<8;
IntervalTimer controlTimer;
SdFs sd;
FsFile logFile;
struct Feedback { float positionRad,unwrappedPositionRad,speedRadS,torqueNm; uint32_t rxUs,sequence; uint8_t error,positionRebased; };
Feedback feedback={};
volatile RunState state=IDLE;
volatile uint32_t head=0,tail=0;
volatile bool abortRequested=false,ioFault=false;
volatile uint8_t faultReason=0;
bool canReady=false,cardReady=false,timerReady=false,recording=false,entered=false,storageFailed=false;
bool fileComplete=false;
uint32_t startEpoch=0,startUs=0,lastStepUs=0,lastTickUs=0,lastSyncUs=0,stepNumber=0;
uint32_t highWater=0,missedRows=0;
volatile uint16_t torqueCount=TORQUE_ZERO_COUNT;
volatile uint8_t phase=0,mode=0,stepResult=RESULT_NONE,eventCode=EVENT_NONE;
float meanSpeed=0,meanTorque=0,speedStd=0,torqueStd=0,acceleration=0,windowSeconds=0;
volatile uint16_t validCount=0;
bool metricsReady=false;
uint8_t commandAcceptedThisTick=0;
volatile uint32_t canTxAcceptedCount=0,canTxCompletedCount=0,canTxFailedCount=0;
volatile bool canTxInFlight=false,stopConfirmed=false,disableTxCompleted=false;
volatile uint8_t stopReason=0;
constexpr uint8_t TX_KIND_OTHER=0,TX_KIND_STOP_ZERO=1,TX_KIND_DISABLE=2;
volatile uint8_t canTxInFlightKind=TX_KIND_OTHER;
volatile uint32_t stopZeroCompletedCount=0,disableTxCompletedCount=0;
DMAMEM Sample ring[RING_SIZE];
char serialLine[64]; uint8_t serialLength=0;
char startDateTime[32]="unknown";
char pendingName[]="FRICPEND.CSV";

struct Point { uint32_t us,seq; float speed,torque; };
constexpr uint32_t STEADY_CAP=STEADY_WINDOW_US/PERIOD_US+8;
Point steadyPoints[STEADY_CAP]; uint32_t steadyHead=0,steadyCount=0,steadyLastSeq=0;
double sumT=0,sumV=0,sumTT=0,sumTV=0,sumVV=0,sumTau=0,sumTau2=0;
struct SpeedPoint {uint32_t us;float v;};
constexpr uint32_t TRANSITION_CAP=TRANSITION_WINDOW_US/PERIOD_US+8;
SpeedPoint transitionPoints[TRANSITION_CAP]; uint32_t transHead=0,transCount=0,transLastSeq=0; double transSum=0;
float stopPosition[STOP_WINDOW_US/PERIOD_US+8],stopSpeed[STOP_WINDOW_US/PERIOD_US+8];
uint32_t stopHead=0,stopCount=0,stopLastScan=0,stopLastRxUs=0,stopEvalTick=0; double stopSpeedSum=0; float stopRange=INFINITY,stopMeanSpeed=INFINITY;
uint8_t handshakeStage=0; bool stopForPhaseChange=false, emergencyStop=false;
uint32_t stopStartUs=0,disableStartUs=0,disableTxTarget=0,disableFailBaseline=0;
uint32_t stopZeroStartCount=0,lastStopZeroCompleteUs=0;
bool stopZeroActive=false;

uint32_t lockInterrupts(){uint32_t p=__get_primask();__disable_irq();return p;}
void unlockInterrupts(uint32_t p){if(!p)__enable_irq();}
bool txIdle(){return !canTxInFlight&&FLEXCAN_get_code(FLEXCANb_MBn_CS(CAN2,8))==FLEXCAN_MB_CODE_TX_INACTIVE;}
void pollTxCompletion(){
  if(!canTxInFlight||!(FLEXCANb_IFLAG1(CAN2)&TX_BIT))return;
  uint32_t code=FLEXCAN_get_code(FLEXCANb_MBn_CS(CAN2,8));
  FLEXCANb_IFLAG1(CAN2)=TX_BIT;
  const uint8_t kind=canTxInFlightKind;
  canTxInFlight=false;
  if(code==FLEXCAN_MB_CODE_TX_INACTIVE){
    ++canTxCompletedCount;
    if(kind==TX_KIND_STOP_ZERO){++stopZeroCompletedCount;lastStopZeroCompleteUs=micros();}
    if(kind==TX_KIND_DISABLE)++disableTxCompletedCount;
  }else ++canTxFailedCount;
}
bool sendFrame(const CAN_message_t &m,uint8_t kind=TX_KIND_OTHER){
  if(!txIdle()||MotorCan.getTXQueueCount()){++canTxFailedCount;return false;}
  FLEXCANb_IFLAG1(CAN2)=TX_BIT;
  if(!MotorCan.write(TX_MAILBOX,m)){++canTxFailedCount;return false;}
  canTxInFlight=true;canTxInFlightKind=kind;++canTxAcceptedCount;return true;
}
bool sendCount(uint16_t c,uint8_t kind=TX_KIND_OTHER){MotorFrame f=torqueFrameFromCount(c);CAN_message_t m={};m.id=f.id;m.len=8;memcpy(m.buf,f.data,8);return sendFrame(m,kind);}
bool sendSpecial(uint8_t code,uint8_t kind=TX_KIND_OTHER){CAN_message_t m={};m.id=MOTOR_ID;m.len=8;for(uint8_t i=0;i<7;i++)m.buf[i]=0xff;m.buf[7]=code;return sendFrame(m,kind);}
void receiveISR(const CAN_message_t &m){
  MotorFrame f={m.id,m.flags.extended,m.flags.remote,m.flags.overrun,m.len,{}};memcpy(f.data,m.buf,8);
  MotorFeedback d=decodeMotorFeedback(f);if(!d.valid)return;
  if(m.flags.overrun){if(!faultReason)faultReason=1;return;}
  const Feedback previous=feedback;
  Feedback n={d.positionRad,d.positionRad,d.speedRadS,d.torqueEstimateNm,micros(),previous.sequence+1,d.error,0};
  if(d.error&&!faultReason)faultReason=2;
  if((!isfinite(n.positionRad)||!isfinite(n.speedRadS)||!isfinite(n.torqueNm))&&!faultReason)faultReason=3;
  if(previous.sequence){
    const uint32_t dtUs=uint32_t(n.rxUs-previous.rxUs);
    const float delta=wrappedPositionDelta(n.positionRad,previous.positionRad);
    const float maxTravel=VELOCITY_MAX_RAD_S*dtUs*1e-6f+POSITION_JUMP_TOLERANCE_RAD_X1000*0.001f;
    if(fabsf(delta)>maxTravel&&!faultReason)faultReason=14;
    // After a long gap, multiple 25-rad wraps are indistinguishable. Rebase
    // the logged angle and mark the discontinuity instead of inventing turns.
    if(maxTravel>=POSITION_WRAP_PERIOD_RAD*0.5f){n.unwrappedPositionRad=n.positionRad;n.positionRebased=1;}
    else n.unwrappedPositionRad=previous.unwrappedPositionRad+delta;
  }
  feedback=n;
}
void resetMetrics(){steadyHead=steadyCount=steadyLastSeq=0;sumT=sumV=sumTT=sumTV=sumVV=sumTau=sumTau2=0;transHead=transCount=transLastSeq=0;transSum=0;meanSpeed=meanTorque=speedStd=torqueStd=acceleration=windowSeconds=NAN;validCount=0;metricsReady=false;}
void addFeedback(const Feedback &f){
  if(f.sequence==steadyLastSeq)return;
  steadyLastSeq=f.sequence;
  while(steadyCount){Point &old=steadyPoints[steadyHead];if(uint32_t(f.rxUs-old.us)<=STEADY_WINDOW_US)break;sumT-=double(uint32_t(old.us-startUs))*1e-6;sumV-=old.speed;sumTT-=pow(double(uint32_t(old.us-startUs))*1e-6,2);sumTV-=double(uint32_t(old.us-startUs))*1e-6*old.speed;sumVV-=double(old.speed)*old.speed;sumTau-=old.torque;sumTau2-=double(old.torque)*old.torque;steadyHead=(steadyHead+1)%STEADY_CAP;--steadyCount;}
  uint32_t slot=(steadyHead+steadyCount)%STEADY_CAP;
  if(steadyCount==STEADY_CAP){Point &old=steadyPoints[steadyHead];double ot=uint32_t(old.us-startUs)*1e-6;sumT-=ot;sumV-=old.speed;sumTT-=ot*ot;sumTV-=ot*old.speed;sumVV-=double(old.speed)*old.speed;sumTau-=old.torque;sumTau2-=double(old.torque)*old.torque;steadyHead=(steadyHead+1)%STEADY_CAP;--steadyCount;slot=(steadyHead+steadyCount)%STEADY_CAP;}
  steadyPoints[slot]={f.rxUs,f.sequence,f.speedRadS,f.torqueNm};++steadyCount;validCount=steadyCount;
  double tr=uint32_t(f.rxUs-startUs)*1e-6;sumT+=tr;sumV+=f.speedRadS;sumTT+=tr*tr;sumTV+=tr*f.speedRadS;sumVV+=double(f.speedRadS)*f.speedRadS;sumTau+=f.torqueNm;sumTau2+=double(f.torqueNm)*f.torqueNm;
  while(transCount){SpeedPoint &old=transitionPoints[transHead];if(uint32_t(f.rxUs-old.us)<=TRANSITION_WINDOW_US)break;transSum-=old.v;transHead=(transHead+1)%TRANSITION_CAP;--transCount;}
  if(transCount==TRANSITION_CAP){transSum-=transitionPoints[transHead].v;transHead=(transHead+1)%TRANSITION_CAP;--transCount;}
  uint32_t ts=(transHead+transCount)%TRANSITION_CAP;transitionPoints[ts]={f.rxUs,f.speedRadS};++transCount;transSum+=f.speedRadS;transLastSeq=f.sequence;
  if(steadyCount>=3){
    Point &first=steadyPoints[steadyHead],&last=steadyPoints[(steadyHead+steadyCount-1)%STEADY_CAP];
    const double span=uint32_t(last.us-first.us)*1e-6,den=steadyCount*sumTT-sumT*sumT;
    windowSeconds=span;
    if(span>=0.98&&den>1e-9){meanSpeed=sumV/steadyCount;meanTorque=sumTau/steadyCount;speedStd=sqrt(fmax(0.0,sumVV/steadyCount-meanSpeed*meanSpeed));torqueStd=sqrt(fmax(0.0,sumTau2/steadyCount-meanTorque*meanTorque));acceleration=(steadyCount*sumTV-sumT*sumV)/den;metricsReady=true;}
  }
}
bool steady(){return metricsReady&&fabsf(acceleration)<=STEADY_ACCEL_LIMIT&&speedStd<=STEADY_SPEED_STD_LIMIT;}
void resetStop(){stopHead=stopCount=stopLastScan=stopLastRxUs=stopEvalTick=0;stopSpeedSum=0;stopRange=stopMeanSpeed=INFINITY;}
bool observeStop(uint32_t now,const Feedback &f){
  if(f.sequence==0||f.sequence==stopLastScan)return false;
  if(stopCount&&uint32_t(f.rxUs-stopLastRxUs)>FEEDBACK_TIMEOUT_US)resetStop();
  stopLastScan=f.sequence;
  stopLastRxUs=f.rxUs;
  if(uint32_t(now-f.rxUs)>FEEDBACK_TIMEOUT_US)return false;
  const uint32_t cap=sizeof(stopPosition)/sizeof(stopPosition[0]),slot=(stopHead+stopCount)%cap;
  if(stopCount==cap){stopSpeedSum-=stopSpeed[stopHead];stopHead=(stopHead+1)%cap;--stopCount;}
  stopPosition[slot]=f.unwrappedPositionRad;stopSpeed[slot]=f.speedRadS;++stopCount;stopSpeedSum+=f.speedRadS;
  if(stopCount<STOP_WINDOW_US/PERIOD_US+1)return false;
  if(++stopEvalTick%40)return stopRange<STOP_POSITION_RANGE_RAD&&fabsf(stopMeanSpeed)<STOP_MEAN_SPEED_RAD_S;
  float mn=INFINITY,mx=-INFINITY;for(uint32_t i=0;i<stopCount;i++){float p=stopPosition[(stopHead+i)%cap];mn=fminf(mn,p);mx=fmaxf(mx,p);}
  stopRange=mx-mn;stopMeanSpeed=stopSpeedSum/stopCount;
  return stopRange<STOP_POSITION_RANGE_RAD&&fabsf(stopMeanSpeed)<STOP_MEAN_SPEED_RAD_S;
}
void setTorque(uint16_t count,uint32_t now){torqueCount=count;lastStepUs=now;stepResult=RESULT_NONE;eventCode=STEP_START;resetMetrics();}
uint16_t nextCount(bool increasing){if(phase==1)return increasing?fminf(TORQUE_MAX_COUNT_POS,torqueCount+1):torqueCount;if(phase==2)return torqueCount>TORQUE_ZERO_COUNT?torqueCount-1:TORQUE_ZERO_COUNT;if(phase==3)return increasing?(torqueCount>TORQUE_MIN_COUNT_NEG?torqueCount-1:TORQUE_MIN_COUNT_NEG):torqueCount;if(phase==4)return torqueCount<TORQUE_ZERO_COUNT?torqueCount+1:TORQUE_ZERO_COUNT;return TORQUE_ZERO_COUNT;}
void appendSample(uint32_t now,const Feedback &f,bool txAccepted=false){
  if(head-tail>=RING_SIZE){ioFault=true;missedRows++;return;}
  Sample s={};s.timeUs=now-startUs;s.feedbackSequence=f.sequence;s.feedbackAgeUs=now-f.rxUs;s.stepIndex=stepNumber;s.torqueCount=torqueCount;s.phase=phase;s.mode=mode;s.eventCode=eventCode;s.stepResult=stepResult;s.motorError=f.error;s.feedbackFresh=(f.sequence!=0);s.targetTorqueNm=torqueForCount(torqueCount);s.feedbackTorqueNm=f.torqueNm;s.speedRadS=f.speedRadS;s.positionDeg=f.unwrappedPositionRad*(180.0f/PI);s.rawPositionRad=f.positionRad;s.unwrappedPositionRad=f.unwrappedPositionRad;s.meanSpeedRadS=meanSpeed;s.meanFeedbackTorqueNm=meanTorque;s.speedStdRadS=speedStd;s.torqueStdNm=torqueStd;s.accelerationRadS2=acceleration;s.steadyWindowS=windowSeconds;s.validFeedbackCount=validCount;s.stopWindowSampleCount=stopCount;s.commandAccepted=txAccepted;s.positionRebased=f.positionRebased;s.disableTxCompleted=disableTxCompleted;
  ring[head%RING_SIZE]=s;__asm__ volatile("dmb":::"memory");head++;
}
void beginSafeStop(uint8_t reason,bool emergency,uint32_t now){
  if(state==STOPPING||state==STOPPED)return;
  if(!faultReason)faultReason=reason;
  emergencyStop=emergency;state=STOPPING;stopStartUs=now;lastStepUs=now;
  handshakeStage=0;stopReason=0;stopConfirmed=false;disableTxCompleted=false;
  disableStartUs=0;disableTxTarget=0;disableFailBaseline=0;
  stopZeroStartCount=stopZeroCompletedCount;lastStopZeroCompleteUs=0;
  torqueCount=TORQUE_ZERO_COUNT;lastStepUs=now;
  if(stepResult!=SETTLED)stepResult=NOT_SETTLED;
  resetStop();stopLastScan=feedback.sequence;stopZeroActive=false;
  eventCode=reason==13?SPEED_LIMIT_STOP:(emergency?SAFETY_EVENT:ABORT_EVENT);
}
void timerISR(){
  const uint32_t now=micros();pollTxCompletion();if(state==IDLE||state==STOPPED)return;
  commandAcceptedThisTick=0;
  if(abortRequested){abortRequested=false;beginSafeStop(4,false,now);}
  if(ioFault){beginSafeStop(5,true,now);}
  if(faultReason&&state!=STOPPING){beginSafeStop(faultReason,true,now);}
  if(state==STOPPING){
    if(handshakeStage<=1){
      // Try at every 500 us tick. Stage 0 has a bounded deadline even if the
      // first zero command can never be queued; fresh feedback fills the same
      // 2,001-sample confirmation window used by the existing procedure.
      if(sendCount(TORQUE_ZERO_COUNT,TX_KIND_STOP_ZERO)){
        commandAcceptedThisTick=1;
      }
      if(!stopZeroActive&&stopZeroCompletedCount>stopZeroStartCount){
        resetStop();stopLastScan=feedback.sequence;stopZeroActive=true;handshakeStage=1;
      }
      Feedback f=feedback;
      if(stopZeroActive&&f.sequence&&uint32_t(now-f.rxUs)<=FEEDBACK_TIMEOUT_US){
        if(observeStop(now,f)){
          stopConfirmed=true;eventCode=STOP_CONFIRMED;handshakeStage=2;disableStartUs=now;
        }
      }else if(uint32_t(now-stopStartUs)>=FEEDBACK_TIMEOUT_US){
        stopReason=7;eventCode=STOP_TIMEOUT;handshakeStage=2;disableStartUs=now;
      }
      if(handshakeStage==0&&stopZeroCompletedCount==stopZeroStartCount&&uint32_t(now-stopStartUs)>=FEEDBACK_TIMEOUT_US){
        stopReason=12;eventCode=STOP_TIMEOUT;handshakeStage=2;disableStartUs=now;
      }else if(handshakeStage==1&&lastStopZeroCompleteUs&&uint32_t(now-lastStopZeroCompleteUs)>=FEEDBACK_TIMEOUT_US){
        stopReason=12;eventCode=STOP_TIMEOUT;handshakeStage=2;disableStartUs=now;
      }
      if(handshakeStage<=1&&uint32_t(now-stopStartUs)>=STOP_TIMEOUT_US){
        stopReason=11;eventCode=STOP_TIMEOUT;handshakeStage=2;disableStartUs=now;
      }
      appendSample(now,feedback,commandAcceptedThisTick);
      return;
    }
    if(handshakeStage==2){
      // Disable is a request followed by an independent mailbox-completion
      // check. Never treat queue acceptance alone as successful Disable.
      if(!disableStartUs)disableStartUs=now;
      disableTxTarget=disableTxCompletedCount+1;
      if(sendSpecial(0xfd,TX_KIND_DISABLE)){
        commandAcceptedThisTick=1;disableFailBaseline=canTxFailedCount;handshakeStage=3;
      }else if(uint32_t(now-disableStartUs)>=DISABLE_TIMEOUT_US){
        stopReason=16;eventCode=DISABLE_TX_UNCONFIRMED;state=STOPPED;
      }
      appendSample(now,feedback,commandAcceptedThisTick);
      return;
    }
    if(handshakeStage==3){
      if(disableTxCompletedCount>=disableTxTarget){disableTxCompleted=true;handshakeStage=4;state=STOPPED;}
      else if(canTxFailedCount>disableFailBaseline){handshakeStage=2;}
      else if(uint32_t(now-disableStartUs)>=DISABLE_TIMEOUT_US){
        stopReason=16;eventCode=DISABLE_TX_UNCONFIRMED;state=STOPPED;
      }
    }
    appendSample(now,feedback,commandAcceptedThisTick);
    return;
  }
  if(state==WAIT_FEEDBACK){
    if(!startUs)startUs=now;
    if(now-startUs>500000){beginSafeStop(6,true,now);return;}
    if(handshakeStage==0&&txIdle()){if(sendCount(TORQUE_ZERO_COUNT))handshakeStage=1;return;}
    if(handshakeStage==1&&txIdle()){if(sendSpecial(0xfc)){handshakeStage=2;entered=true;}return;}
    if(handshakeStage==2&&txIdle()){if(sendCount(TORQUE_ZERO_COUNT)){handshakeStage=3;return;}}
    Feedback f=feedback;if(handshakeStage>=3&&f.sequence&&now-f.rxUs<=FEEDBACK_TIMEOUT_US){if(fabsf(f.speedRadS)>=EMERGENCY_SPEED_RAD_S){beginSafeStop(8,true,now);return;}startUs=now;phase=1;mode=0;stepNumber=0;setTorque(TORQUE_ZERO_COUNT,now);state=SWEEPING;}
    return;
  }
  Feedback f=feedback;
  if(FLEXCANb_ESR1(CAN2)&FLEXCAN_ESR_FLT_CONF_MASK){beginSafeStop(15,true,now);appendSample(now,f);return;}
  if(!f.sequence||now-f.rxUs>=FEEDBACK_TIMEOUT_US){beginSafeStop(7,true,now);appendSample(now,f);return;}
  if(f.error){beginSafeStop(2,true,now);appendSample(now,f);return;}
  if(fabsf(f.speedRadS)>=EMERGENCY_SPEED_RAD_S){beginSafeStop(8,true,now);appendSample(now,f);return;}
  if(now-startUs>=MAX_RUN_US){beginSafeStop(9,true,now);appendSample(now,f);return;}
  if(uint32_t(now-lastTickUs)>PERIOD_US*3&&lastTickUs){beginSafeStop(10,true,now);appendSample(now,f);return;}lastTickUs=now;
  addFeedback(f);
  if(state==STOPPING)return;
  // The 15 rad/s limit is a coast-stop ceiling, not a cue to hold the current
  // torque while waiting for a steady window. The motor can gain several rad/s
  // within the 3 s settle timeout, so end the sweep immediately at this limit.
  if(fabsf(f.speedRadS)>=NORMAL_SPEED_LIMIT_RAD_S){beginSafeStop(13,false,now);appendSample(now,f);return;}
  // Reaching zero in either descending phase ends that phase immediately,
  // before steady-window and 3 s step-timeout evaluation can return early.
  if((phase==2&&torqueCount<=TORQUE_ZERO_COUNT)||(phase==4&&torqueCount>=TORQUE_ZERO_COUNT)){
    torqueCount=TORQUE_ZERO_COUNT;mode=2;stopForPhaseChange=true;lastStepUs=now;
    resetStop();stopLastScan=f.sequence;stopZeroStartCount=stopZeroCompletedCount;lastStopZeroCompleteUs=0;stopZeroActive=false;
  }
  if(stopForPhaseChange){
    torqueCount=TORQUE_ZERO_COUNT;if(sendCount(TORQUE_ZERO_COUNT,TX_KIND_STOP_ZERO))commandAcceptedThisTick=1;
    if(!stopZeroActive&&stopZeroCompletedCount>stopZeroStartCount){resetStop();stopLastScan=f.sequence;stopZeroActive=true;}
    if(stopZeroActive&&observeStop(now,f)){stopForPhaseChange=false;eventCode=STOP_CONFIRMED;resetStop();
      if(phase==2){phase=3;mode=0;setTorque(TORQUE_ZERO_COUNT,now);eventCode=PHASE_CHANGE;}
      else {
        state=STOPPING;emergencyStop=false;stopStartUs=now;lastStepUs=now;handshakeStage=0;
        disableStartUs=0;stopConfirmed=disableTxCompleted=false;stopReason=0;
        stopZeroStartCount=stopZeroCompletedCount;lastStopZeroCompleteUs=0;stopZeroActive=false;
        resetStop();stopLastScan=f.sequence;eventCode=PHASE_CHANGE;
      }
    }else if(now-lastStepUs>=STOP_TIMEOUT_US){beginSafeStop(11,true,now);}
    if(stopForPhaseChange&&!stopZeroActive&&uint32_t(now-lastStepUs)>=FEEDBACK_TIMEOUT_US){beginSafeStop(12,true,now);}
    else if(stopForPhaseChange&&stopZeroActive&&lastStopZeroCompleteUs&&uint32_t(now-lastStopZeroCompleteUs)>=FEEDBACK_TIMEOUT_US){beginSafeStop(12,true,now);}
    appendSample(now,f,commandAcceptedThisTick);
    return;
  }
  if(mode==0&&transCount&&uint32_t(now-transitionPoints[transHead].us)>=TRANSITION_WINDOW_US&&fabs(transSum/transCount)>=LOW_SPEED_BOUNDARY_RAD_S){mode=1;setTorque(torqueCount,now);}
  if((phase==2||phase==4)&&mode!=2&&fabsf(f.speedRadS)<LOW_SPEED_BOUNDARY_RAD_S){mode=2;setTorque(torqueCount,now);}
  if(phase==1||phase==3){
    if(torqueCount==(phase==1?TORQUE_MAX_COUNT_POS:TORQUE_MIN_COUNT_NEG)){
      phase=phase==1?2:4;mode=fabsf(f.speedRadS)<LOW_SPEED_BOUNDARY_RAD_S?2:1;stepResult=RESULT_NONE;eventCode=PHASE_CHANGE;lastStepUs=now;resetMetrics();appendSample(now,f);return;
    }
  }
  bool low=(mode==0||mode==2);
  if(low){
    if(now-lastStepUs>=LOW_RAMP_US){uint16_t n=nextCount(phase==1||phase==3);if(n!=torqueCount){++stepNumber;setTorque(n,now);}}
  }else if(now-lastStepUs>=MAX_STEP_WAIT_US){
    stepResult=NOT_SETTLED;eventCode=STEP_TIMEOUT;appendSample(now,f);
    uint16_t n=nextCount(phase==1||phase==3);if(n!=torqueCount){++stepNumber;setTorque(n,now);}return;
  }else if(steady()){
    stepResult=SETTLED;eventCode=STEP_SETTLED;appendSample(now,f);
    uint16_t n=nextCount(phase==1||phase==3);if(n!=torqueCount){++stepNumber;setTorque(n,now);}return;
  }
  if(txIdle()){
    if(!sendCount(torqueCount)){beginSafeStop(12,true,now);appendSample(now,f);return;}
    commandAcceptedThisTick=1;
  }
  appendSample(now,f,commandAcceptedThisTick);
  eventCode=EVENT_NONE;stepResult=RESULT_NONE;
}

void writeCsvHeader(){
  logFile.println("# experiment=AK80-8_1LSBfric");logFile.print("# start_epoch=");logFile.println(startEpoch);logFile.print("# start_datetime_local=");logFile.println(startDateTime);
  logFile.println("# motor=AK80-8,CAN2,ID2,bitrate=1000000");logFile.print("# MIT_torque_range_Nm=-32..32,counts=0..4095,quantum_Nm=");logFile.print(TORQUE_QUANTUM_NM,10);logFile.println(",zero_count=2047,zero_decoded_Nm=-0.007814408");
  logFile.println("# control_period_us=500,low_ramp_us=100000,transition_window_us=50000,steady_window_us=1000000,max_step_wait_us=3000000");
  logFile.println("# accel_limit_rad_s2=0.3,speed_std_limit_rad_s=0.08,low_boundary_rad_s=1,normal_speed_limit_rad_s=15(coast_stop),emergency_speed_rad_s=20,torque_limit_Nm=1,feedback_timeout_us=20000,max_run_s=900");
  logFile.println("# position_encoding_rad=-12.5..12.5,wrap_period_rad=25; absolute_position_limit=disabled; position_raw_rad and position_unwrapped_rad are both logged; position_deg remains unwrapped");
  logFile.println("# valid_feedback_count=unique_feedback_in_current_step_window; frozen_after_stop_start; stop_window_sample_count_is_separate; steady_metrics_nan_until_window_is_full");
  logFile.println("# step_result=1=SETTLED,2=NOT_SETTLED; CAN_TX_counts=per_run,accepted_is_mailbox_acceptance,completed_is_controller_transmission_completion,failed_is_rejected_or_aborted");
  logFile.println("# modes=0=low_ramp_increase,1=steady_step,2=low_ramp_decrease; events=1=STEP_START,2=STEP_SETTLED,3=STEP_TIMEOUT,4=PHASE_CHANGE,5=STOP_CONFIRMED,6=STOP_TIMEOUT,7=ABORT,8=SAFETY,9=SPEED_LIMIT_STOP,10=DISABLE_TX_UNCONFIRMED; phases=1:+increase,2:+decrease,3:-increase,4:-decrease; stop=nearest_encoded_zero_torque_at_2kHz,coast,no_active_braking");
  logFile.println("# 15_rad_s_stops_the_entire_sweep_before_descending_phases_2_and_4; disable_tx_completed_means_CAN_mailbox_transmission_completed_not_driver_ack");
  logFile.println("# stop_reasons:1=CAN_overrun,2=motor_error,3=invalid_numeric_feedback,4=user_abort,5=SD_error,6=start_timeout,7=feedback_timeout,8=overspeed,9=run_timeout,10=control_gap,11=stop_timeout,12=TX_failed,13=15rad_s_speed_limit,14=position_jump_implausible,15=CAN_error_state,16=disable_tx_unconfirmed");
  logFile.println("# CAN_TX_acceptance_is_not_motor_response; feedback_torque_is_motor_estimate_not_external_sensor");
  logFile.println("record_type,time_us,phase,mode,step_index,torque_command_count,target_torque_Nm,feedback_torque_estimate_Nm,speed_rad_s,position_deg,feedback_sequence,feedback_age_us,motor_error,event_code,step_result,mean_speed_rad_s,mean_feedback_torque_Nm,speed_std_rad_s,torque_std_Nm,estimated_acceleration_rad_s2,steady_window_duration_s,valid_feedback_count,command_tx_accepted,position_rebased,position_raw_rad,position_unwrapped_rad,stop_window_sample_count,disable_tx_completed");
}
void serviceLog(){
  if(!recording)return;
  uint32_t available=head-tail;if(available>highWater)highWater=available;
  if(state==STOPPED&&storageFailed){logFile.close();recording=false;fileComplete=true;Serial.println("STOPPED,SD_ERROR_SOURCE_RETAINED");return;}
  if(available>=BATCH_ROWS||((state==STOPPED)&&available)){
    uint32_t n=available<BATCH_ROWS?available:BATCH_ROWS;char line[320];
    for(uint32_t i=0;i<n;i++){
      Sample s=ring[(tail+i)%RING_SIZE];int len=snprintf(line,sizeof(line),"T,%lu,%u,%u,%u,%u,%.9g,%.9g,%.9g,%.9g,%lu,%lu,%u,%u,%u,%.9g,%.9g,%.9g,%.9g,%.9g,%.6g,%u,%u,%u,%.9g,%.9g,%u,%u\n",
        (unsigned long)s.timeUs,s.phase,s.mode,s.stepIndex,s.torqueCount,s.targetTorqueNm,s.feedbackTorqueNm,s.speedRadS,s.positionDeg,(unsigned long)s.feedbackSequence,(unsigned long)s.feedbackAgeUs,s.motorError,s.eventCode,s.stepResult,s.meanSpeedRadS,s.meanFeedbackTorqueNm,s.speedStdRadS,s.torqueStdNm,s.accelerationRadS2,s.steadyWindowS,s.validFeedbackCount,s.commandAccepted,s.positionRebased,s.rawPositionRad,s.unwrappedPositionRad,s.stopWindowSampleCount,s.disableTxCompleted);
      if(len<=0||len>=int(sizeof(line))||logFile.write(line,len)!=size_t(len)){ioFault=true;storageFailed=true;missedRows++;return;}
    }
    __asm__ volatile("dmb":::"memory");tail+=n;
  }
  if(recording&&uint32_t(micros()-lastSyncUs)>=SD_SYNC_US){if(!logFile.sync()){ioFault=true;storageFailed=true;return;}lastSyncUs=micros();}
  if(state==STOPPED&&tail==head){
    const char *result=!disableTxCompleted?"DISABLE_UNCONFIRMED":(!stopConfirmed?"STOP_UNCONFIRMED":(faultReason==13?"SPEED_LIMIT_STOP":(faultReason?"ABORTED":"COMPLETE")));
    logFile.print("# result=");logFile.print(result);logFile.print(",reason=");logFile.print(faultReason);
    logFile.print(",stop_reason=");logFile.print(stopReason);logFile.print(",stop_confirmed=");logFile.print(stopConfirmed?1:0);
    logFile.print(",disable_tx_completed=");logFile.print(disableTxCompleted?1:0);
    logFile.print(",rows=");logFile.print(head);logFile.print(",dropped=");logFile.println(missedRows);
    logFile.sync();logFile.close();recording=false;fileComplete=true;Serial.println("STOPPED");
  }
}
bool startRun(uint32_t epoch){
  if(recording||canTxInFlight||!cardReady||!timerReady||!canReady||sd.exists(pendingName))return false;
  uint64_t need=uint64_t(900)*2000*320;
  if(uint64_t(sd.vol()->freeClusterCount())*sd.vol()->bytesPerCluster()<need)return false;
  if(!logFile.open(pendingName,O_WRONLY|O_CREAT|O_EXCL))return false;
  startEpoch=epoch;writeCsvHeader();if(!logFile.sync()){storageFailed=true;logFile.close();return false;}
  uint32_t p=lockInterrupts();head=tail=stepNumber=0;phase=mode=0;faultReason=0;stopReason=0;ioFault=false;storageFailed=false;abortRequested=false;missedRows=highWater=0;handshakeStage=0;entered=false;stopForPhaseChange=false;emergencyStop=false;stopConfirmed=disableTxCompleted=false;canTxAcceptedCount=canTxCompletedCount=canTxFailedCount=0;stopZeroCompletedCount=disableTxCompletedCount=0;recording=true;fileComplete=false;startUs=0;lastStepUs=lastTickUs=0;state=WAIT_FEEDBACK;unlockInterrupts(p);return true;
}
uint32_t crc32File(FsFile &f){uint32_t crc=0xffffffff;uint8_t b[512];f.seekSet(0);while(true){int n=f.read(b,sizeof(b));if(n<=0)break;for(int i=0;i<n;i++){crc^=b[i];for(uint8_t k=0;k<8;k++)crc=(crc>>1)^(0xedb88320u&-(crc&1));}}f.seekSet(0);return ~crc;}
void fetchFile(){
  if(recording||!sd.exists(pendingName)){Serial.println("ERROR,NO_FILE_OR_BUSY");return;}
  FsFile f;if(!f.open(pendingName,O_RDONLY)){Serial.println("ERROR,OPEN");return;}
  uint32_t size=f.fileSize(),crc=crc32File(f);Serial.print("DATA,");Serial.print(size);Serial.print(',');Serial.print(crc,HEX);Serial.print(',');Serial.println(startEpoch);Serial.flush();
  uint8_t b[256];while(f.available()){int n=f.read(b,sizeof(b));if(n<=0)break;size_t off=0;while(off<size_t(n)){size_t w=Serial.write(b+off,n-off);if(!w){f.close();return;}off+=w;}}
  f.close();Serial.println("\nTRANSFER_END");Serial.flush();
}
const char *runStateName(uint8_t s){
  switch(s){case IDLE:return "IDLE";case WAIT_FEEDBACK:return "WAIT_FEEDBACK";case SWEEPING:return "SWEEPING";case STOPPING:return "STOPPING";case STOPPED:return "STOPPED";default:return "UNKNOWN";}
}
void printStatus(){
  Feedback f;uint32_t now,pending,accepted,completed,failed,stopSamples;
  uint16_t tCount,vCount;uint8_t s,ph,md,reason,primaryReason,stopOk,disableOk;bool busy;
  uint32_t irq=lockInterrupts();
  f=feedback;now=micros();s=uint8_t(state);ph=phase;md=mode;tCount=torqueCount;vCount=validCount;
  stopSamples=stopCount;accepted=canTxAcceptedCount;completed=canTxCompletedCount;failed=canTxFailedCount;
  busy=canTxInFlight;reason=stopReason?stopReason:faultReason;primaryReason=faultReason;stopOk=stopConfirmed;disableOk=disableTxCompleted;
  pending=head-tail;unlockInterrupts(irq);
  const uint32_t age=f.sequence?uint32_t(now-f.rxUs):0xffffffffu;
  Serial.print("STATUS,");Serial.print(recording?"RUNNING":"IDLE");Serial.print(',');Serial.print(s);Serial.print(',');Serial.print(runStateName(s));
  Serial.print(',');Serial.print(ph);Serial.print(',');Serial.print(md);Serial.print(',');Serial.print(tCount);Serial.print(',');Serial.print(torqueForCount(tCount),6);
  Serial.print(',');Serial.print(f.sequence);Serial.print(',');Serial.print(age);Serial.print(',');Serial.print(accepted);Serial.print(',');Serial.print(completed);Serial.print(',');Serial.print(failed);
  Serial.print(',');Serial.print(vCount);Serial.print(',');Serial.print(stopSamples);Serial.print(',');Serial.print(primaryReason);Serial.print(',');Serial.print(reason);
  Serial.print(',');Serial.print(stopOk);Serial.print(',');Serial.print(disableOk);Serial.print(',');Serial.print(busy);Serial.print(',');Serial.println(pending);
}
void handleLine(char *line){
  if(!strncmp(line,"START,",6)){char *date=strchr(line+6,',');if(date){*date++=0;strncpy(startDateTime,date,sizeof(startDateTime)-1);startDateTime[sizeof(startDateTime)-1]=0;}else strcpy(startDateTime,"epoch_only");uint32_t epoch=strtoul(line+6,nullptr,10);bool ok=startRun(epoch);Serial.println(ok?"STARTED":"ERROR,START_REJECTED_SD_OR_BUSY");}
  else if(!strcmp(line,"STATUS")){printStatus();}
  else if(!strcmp(line,"ABORT")){abortRequested=true;Serial.println("ABORT_ACCEPTED");}
  else if(!strcmp(line,"FETCH")){fetchFile();}
  else if(!strcmp(line,"ACK")){if(!recording&&fileComplete&&sd.remove(pendingName)){fileComplete=false;Serial.println("ACK_OK");}else Serial.println("ERROR,ACK_REJECTED");}
  else Serial.println("ERROR,UNKNOWN_COMMAND");
}
bool setupCAN(){
  MotorCan.begin();MotorCan.setBaudRate(CAN_BITRATE);MotorCan.setMaxMB(16);
  for(uint8_t i=0;i<16;i++)MotorCan.setMB(FLEXCAN_MAILBOX(i),RX,INACTIVE);
  MotorCan.setMB(MB0,RX,STD);MotorCan.setMBFilter(REJECT_ALL);MotorCan.setMB(TX_MAILBOX,TX,STD);
  FLEXCANb_IFLAG1(CAN2)=TX_BIT;FLEXCANb_IMASK1(CAN2)&=~TX_BIT;MotorCan.setMBFilter(MB0,MOTOR_ID);MotorCan.onReceive(MB0,receiveISR);MotorCan.enableMBInterrupt(MB0);
  NVIC_SET_PRIORITY(IRQ_CAN2,ISR_PRIORITY);
  FLEXCANb_MCR(CAN2)|=FLEXCAN_MCR_FRZ|FLEXCAN_MCR_HALT;
  uint32_t started=micros();while(!(FLEXCANb_MCR(CAN2)&FLEXCAN_MCR_FRZ_ACK))if(micros()-started>10000)return false;
  FLEXCANb_MCR(CAN2)|=FLEXCAN_MCR_AEN;FLEXCANb_MCR(CAN2)&=~FLEXCAN_MCR_HALT;
  started=micros();while(FLEXCANb_MCR(CAN2)&FLEXCAN_MCR_FRZ_ACK)if(micros()-started>10000)return false;
  return true;
}
void setup(){Serial.begin(115200);uint32_t t=millis();while(!Serial&&millis()-t<3000){}
  canReady=setupCAN();cardReady=sd.begin(SdioConfig(FIFO_SDIO));if(cardReady&&sd.exists(pendingName))fileComplete=true;controlTimer.priority(ISR_PRIORITY);timerReady=controlTimer.begin(timerISR,PERIOD_US);
  Serial.println(cardReady&&canReady&&timerReady?"READY":"ERROR,INIT");
}
void loop(){
  while(Serial.available()){char c=Serial.read();if(c=='\n'||c=='\r'){if(serialLength){serialLine[serialLength]=0;handleLine(serialLine);serialLength=0;}}else if(serialLength<sizeof(serialLine)-1)serialLine[serialLength++]=c;else serialLength=0;}
  serviceLog();
}
