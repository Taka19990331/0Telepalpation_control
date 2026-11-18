#pragma once
#include <Arduino.h>
#include "control_config.h"

namespace Diag {
enum State : uint8_t { IDLE=0, RUNNING=1, FAULT_STOPPED=2 };
enum TxKind : uint8_t { PERIODIC_ZERO=0, START_ENABLE=1, START_ZERO=2, STOP_ZERO=3, STOP_DISABLE=4, TX_KIND_COUNT=5 };
enum FaultKind : uint8_t { FAULT_NONE=0, FAULT_TX_FAIL, FAULT_RX_TIMEOUT, FAULT_BUS_OFF, FAULT_RX_OVERFLOW, FAULT_ISR_OVERRUN, FAULT_MOTOR_ERROR };
enum HistoryKind : uint8_t { HISTORY_SAMPLE=0, HISTORY_FAULT=1 };

struct RxFrame { CAN_message_t msg; uint32_t us; };
struct HistoryRow {
  uint32_t us, rxFrames, rxAccepted, rxQueued, rxProcessed;
  uint32_t periodicAccepted, periodicFailures, stopZeroFailures, stopDisableFailures;
  uint32_t lastRxUs, esr1, isrMaxUs;
  uint16_t tec, rec, txQueue, rxQueue;
  uint8_t kind;
};
struct TxFailure {
  bool valid=false;
  uint32_t us=0;
  uint8_t bus=0, motorId=0, type=0;
  int16_t apiResult=0;
  uint16_t txQueueBefore=0, txQueueAfter=0, rxQueue=0;
  uint16_t tec=0, rec=0;
  uint32_t esr1=0, rxAgeUs=UINT32_MAX;
  uint32_t rxFrames=0, rxProcessed=0, periodicAccepted=0, periodicFailures=0, isrMaxUs=0;
};
struct FaultSnapshot {
  bool valid=false;
  uint32_t us=0, esr1=0, ecr=0, rxAgeUs=UINT32_MAX;
  uint8_t motor=255, kind=FAULT_NONE, motorErrorCode=0;
  uint32_t rxFrames=0, rxProcessed=0, periodicAccepted=0, periodicFailures=0;
  uint16_t txQueue=0, rxQueue=0;
  uint32_t isrMaxUs=0;
};
struct Motor {
  volatile uint32_t rxFrames=0, rxAccepted=0, rxQueued=0, rxProcessed=0, rxRejected=0;
  volatile uint32_t rejectId=0, rejectPayloadId=0, rejectDlc=0, rejectFormat=0;
  volatile uint32_t rxOverflow=0, rxHwOverrun=0, lastRxUs=0;
  volatile uint32_t txAccepted[TX_KIND_COUNT]={0,0,0,0,0};
  volatile uint32_t txFailures[TX_KIND_COUNT]={0,0,0,0,0};
  volatile uint32_t isrMaxUs=0, canErrorEvents=0, motorError=0;
  volatile uint32_t ecrMax=0, esrSeen=0, busOff=0;
  volatile uint32_t head=0, tail=0;
  RxFrame queue[RX_QUEUE_SIZE];
  HistoryRow history[HISTORY_SIZE];
  volatile uint16_t histHead=0, histCount=0;
  volatile bool historyFrozen=false;
  uint32_t historyStampUs=0;
  bool sawRx=false, gapReported=false;
  uint8_t motorErrorCode=0;
  uint32_t lastCanSignature=0;
};
static Motor motor[2];
static volatile State state=IDLE;
static volatile bool faultPending=false;
static volatile uint32_t startUs=0;
static volatile FaultSnapshot fault;
static volatile TxFailure firstTxFailure;

inline void reset(uint32_t now) {
  state=IDLE; faultPending=false; startUs=now;
  fault.valid=false;fault.us=0;fault.esr1=0;fault.ecr=0;fault.rxAgeUs=UINT32_MAX;fault.motor=255;fault.kind=FAULT_NONE;fault.motorErrorCode=0;
  fault.rxFrames=fault.rxProcessed=fault.periodicAccepted=fault.periodicFailures=0;fault.txQueue=fault.rxQueue=0;fault.isrMaxUs=0;
  firstTxFailure.valid=false;firstTxFailure.us=0;firstTxFailure.bus=firstTxFailure.motorId=firstTxFailure.type=0;firstTxFailure.apiResult=0;
  firstTxFailure.txQueueBefore=firstTxFailure.txQueueAfter=firstTxFailure.rxQueue=0;firstTxFailure.tec=firstTxFailure.rec=0;
  firstTxFailure.esr1=0;firstTxFailure.rxAgeUs=UINT32_MAX;firstTxFailure.rxFrames=firstTxFailure.rxProcessed=0;
  firstTxFailure.periodicAccepted=firstTxFailure.periodicFailures=firstTxFailure.isrMaxUs=0;
  for(uint8_t i=0;i<2;++i) if(motorOn(i)) {
    Motor &d=motor[i];
    d.rxFrames=d.rxAccepted=d.rxQueued=d.rxProcessed=d.rxRejected=0;
    d.rejectId=d.rejectPayloadId=d.rejectDlc=d.rejectFormat=0;
    d.rxOverflow=d.rxHwOverrun=d.lastRxUs=0;
    for(uint8_t k=0;k<TX_KIND_COUNT;++k) d.txAccepted[k]=d.txFailures[k]=0;
    d.isrMaxUs=d.canErrorEvents=d.motorError=0;
    d.ecrMax=d.esrSeen=d.busOff=0; d.head=d.tail=0;
    d.histHead=d.histCount=0; d.historyFrozen=false; d.historyStampUs=now;
    d.sawRx=false; d.gapReported=false; d.motorErrorCode=0; d.lastCanSignature=0;
  }
}
inline void recordRx(uint8_t i, const CAN_message_t &m) {
  if(state!=RUNNING) return;
  Motor &d=motor[i]; const uint32_t now=micros(); ++d.rxFrames;
  if(m.flags.overrun) ++d.rxHwOverrun;
  if(m.id!=MOTOR_ID[i]) { ++d.rejectId; ++d.rxRejected; return; }
  if(m.flags.remote||m.flags.extended) { ++d.rejectFormat; ++d.rxRejected; return; }
  if(m.len!=8) { ++d.rejectDlc; ++d.rxRejected; return; }
  if(m.buf[0]!=MOTOR_ID[i]) { ++d.rejectPayloadId; ++d.rxRejected; return; }
  ++d.rxAccepted; d.sawRx=true; d.lastRxUs=now;
  const uint32_t h=d.head,t=d.tail;
  if(h-t>=RX_QUEUE_SIZE) { ++d.rxOverflow; return; }
  const uint32_t slot=h%RX_QUEUE_SIZE; d.queue[slot].msg=m; d.queue[slot].us=now;
  asm volatile("" ::: "memory"); d.head=h+1; ++d.rxQueued;
}
inline void commitHistoryRow(uint8_t i, const HistoryRow &row, bool faultRow=false) {
  Motor &d=motor[i];
  noInterrupts();
  if(faultRow || !d.historyFrozen) {
    const uint16_t p=d.histHead; d.history[p]=row;
    d.histHead=(p+1)%HISTORY_SIZE; if(d.histCount<HISTORY_SIZE) ++d.histCount;
  }
  interrupts();
}
inline void pushHistory(uint8_t i, uint32_t now, uint32_t esr1, uint32_t ecr, uint32_t txQ) {
  Motor &d=motor[i]; HistoryRow r{};
  noInterrupts();
  r.us=now; r.rxFrames=d.rxFrames; r.rxAccepted=d.rxAccepted; r.rxQueued=d.rxQueued; r.rxProcessed=d.rxProcessed;
  r.periodicAccepted=d.txAccepted[PERIODIC_ZERO]; r.periodicFailures=d.txFailures[PERIODIC_ZERO];
  r.stopZeroFailures=d.txFailures[STOP_ZERO]; r.stopDisableFailures=d.txFailures[STOP_DISABLE];
  r.lastRxUs=d.sawRx?d.lastRxUs:0; r.esr1=esr1; r.isrMaxUs=d.isrMaxUs;
  r.tec=ecr&255U; r.rec=(ecr>>8)&255U; r.txQueue=txQ; r.rxQueue=d.head-d.tail; r.kind=HISTORY_SAMPLE;
  interrupts();
  commitHistoryRow(i,r);
}
inline void pushFaultHistory(uint8_t i, uint32_t now, uint32_t esr1, uint32_t ecr, uint32_t txQ) {
  Motor &d=motor[i]; HistoryRow r{};
  r.us=now; r.rxFrames=d.rxFrames; r.rxAccepted=d.rxAccepted; r.rxQueued=d.rxQueued; r.rxProcessed=d.rxProcessed;
  r.periodicAccepted=d.txAccepted[PERIODIC_ZERO]; r.periodicFailures=d.txFailures[PERIODIC_ZERO];
  r.stopZeroFailures=d.txFailures[STOP_ZERO]; r.stopDisableFailures=d.txFailures[STOP_DISABLE];
  r.lastRxUs=d.sawRx?d.lastRxUs:0; r.esr1=esr1; r.isrMaxUs=d.isrMaxUs;
  r.tec=ecr&255U; r.rec=(ecr>>8)&255U; r.txQueue=txQ; r.rxQueue=d.head-d.tail; r.kind=HISTORY_FAULT;
  commitHistoryRow(i,r,true);
}
}
