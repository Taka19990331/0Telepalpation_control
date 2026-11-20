#pragma once
#include "Arduino.h"
enum { CAN1=1,CAN2=2,CAN3=3,RX_SIZE_256=256,TX_SIZE_16=16,MB0=0,RX=0,STD=0,REJECT_ALL=0,ACCEPT_ALL=1 };
struct CAN_message_t {
  uint32_t id=0; uint8_t len=0, buf[8]={};
  struct { bool remote=false,extended=false,overrun=false; } flags;
};
struct CAN_error_t { uint32_t ESR1=0,ECR=0; };
template<int B,int R,int T> struct FlexCAN_T4 {
  unsigned getTXQueueCount() { return 0; }
  int write(const CAN_message_t &) { return 1; }
  bool error(CAN_error_t &,bool) { return false; }
  void begin() {} void setBaudRate(unsigned) {} void setMaxMB(unsigned) {}
  void setMB(int,int,int) {} void setMBFilter(int) {} void setMBFilter(int,int) {}
  void onReceive(int,void (*)(const CAN_message_t &)) {} void enableMBInterrupt(int) {}
};
#define FLEXCANb_ESR1(b) 0U
#define FLEXCANb_ECR(b) 0U
#define FLEXCANb_MBn_CS(b,i) 0U
#define FLEXCAN_get_code(x) (x)
#define FLEXCAN_MB_CODE_TX_INACTIVE 0U
#define FLEXCAN_MB_CODE_TX_ONCE 1U
