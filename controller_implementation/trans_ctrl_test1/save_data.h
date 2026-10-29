#pragma once

// SD binary logger and serial CSV exporter for trans_ctrl_test1.
#include <Arduino.h>
#include <SdFat.h>
#include <string.h>
#include "can_protocol.h"

namespace SaveData {
constexpr uint32_t RING_SIZE = 4096;
constexpr uint32_t BATCH_ROWS = 128;
struct Sample { uint32_t timeUs; float commandNm; uint8_t reserved[4]; };
struct Header {
  char magic[8];
  uint32_t version, periodUs, motorId, canBaud, rowBytes, rows;
  float torqueNm;
};
struct Footer { char magic[8]; uint32_t rows, status; };
static_assert(sizeof(Sample) == 12, "Unexpected sample layout");

DMAMEM static Sample samples[RING_SIZE];
static volatile uint32_t head=0, tail=0;
static volatile bool recording=false, finalizeRequest=false;
static volatile uint32_t statusCode=0;
static SdFs sd;
static FsFile logFile;
static char currentFile[16]={}, latestFile[16]={};
static uint32_t savedRows=0;
static bool cardReady=false;

inline bool begin() {
  cardReady=sd.begin(SdioConfig(FIFO_SDIO));
  if(cardReady) {
    for(int n=9999;n>=0;--n) {
      snprintf(currentFile,sizeof(currentFile),"TC%04d.BIN",n);
      if(sd.exists(currentFile)){strcpy(latestFile,currentFile);break;}
    }
  }
  return cardReady;
}

inline bool openLog(float torqueNm) {
  bool found=false;
  for(uint16_t n=0;n<10000;n++) {
    snprintf(currentFile,sizeof(currentFile),"TC%04u.BIN",n);
    if(!sd.exists(currentFile)){found=true;break;}
  }
  if(!found || !logFile.open(currentFile,O_RDWR|O_CREAT|O_EXCL)) return false;
  Header h={{'T','R','A','N','S','C','T','1'},1,CanProtocol::PERIOD_US,
    CanProtocol::MOTOR_ID,CanProtocol::CAN_BAUD,sizeof(Sample),0,torqueNm};
  if(logFile.write(&h,sizeof(h))!=sizeof(h) || !logFile.sync()) {logFile.close();return false;}
  strcpy(latestFile,currentFile);
  return true;
}

inline bool start(float torqueNm) {
  if(!cardReady || recording || finalizeRequest) return false;
  noInterrupts(); head=tail=0; statusCode=0; finalizeRequest=false; interrupts();
  savedRows=0;
  if(!openLog(torqueNm)) return false;
  recording=true;
  return true;
}

// Called from the control timer ISR after a command is sent successfully.
inline bool record(uint32_t timeUs,float torqueNm) {
  if(head-tail>=RING_SIZE) return false;
  Sample &s=samples[head%RING_SIZE];
  s.timeUs=timeUs; s.commandNm=torqueNm;
  ++head;
  return true;
}

inline void finish(uint32_t result) {
  statusCode=result; recording=false; finalizeRequest=true;
}

// Call frequently from loop(); returns true once finalization has completed.
inline bool service() {
  if(!recording && !finalizeRequest) return false;
  uint32_t count=head-tail;
  if(count && (count>=BATCH_ROWS || finalizeRequest)) {
    if(count>BATCH_ROWS) count=BATCH_ROWS;
    const uint32_t contiguous=RING_SIZE-(tail%RING_SIZE);
    if(count>contiguous) count=contiguous;
    const size_t bytes=size_t(count)*sizeof(Sample);
    if(logFile.write(&samples[tail%RING_SIZE],bytes)!=bytes) {
      noInterrupts(); statusCode=4; recording=false; tail=head; finalizeRequest=true; interrupts();
    } else {
      noInterrupts(); tail+=count; interrupts(); savedRows+=count;
    }
  }
  if(finalizeRequest && tail==head) {
    Header h={};
    bool ok=logFile.seekSet(0)&&logFile.read(&h,sizeof(h))==sizeof(h);
    if(ok) {
      h.rows=savedRows;
      Footer f={{'E','N','D','T','C','0','0','1'},savedRows,statusCode};
      ok=logFile.seekSet(sizeof(Header)+uint64_t(savedRows)*sizeof(Sample))&&
        logFile.write(&f,sizeof(f))==sizeof(f)&&logFile.seekSet(0)&&
        logFile.write(&h,sizeof(h))==sizeof(h)&&logFile.sync();
    }
    logFile.close(); finalizeRequest=false;
    Serial.print("SAVED,"); Serial.print(latestFile); Serial.print(",ROWS,");
    Serial.print(savedRows); Serial.print(",STATUS,"); Serial.println(ok?statusCode:5);
    return true;
  }
  return false;
}

inline void dumpCsv() {
  if(recording || !latestFile[0]) return;
  FsFile f; if(!f.open(latestFile,O_RDONLY)) return;
  Header h; Footer end;
  bool ok=f.read(&h,sizeof(h))==sizeof(h)&&memcmp(h.magic,"TRANSCT1",8)==0&&h.rowBytes==sizeof(Sample);
  ok=ok&&f.seekSet(sizeof(Header)+uint64_t(h.rows)*sizeof(Sample))&&
    f.read(&end,sizeof(end))==sizeof(end)&&memcmp(end.magic,"ENDTC001",8)==0&&end.rows==h.rows;
  if(!ok||!f.seekSet(sizeof(Header))) {f.close();Serial.println("SD_READ_ERROR");return;}
  Serial.print("TRANS_BEGIN,");Serial.print(latestFile);Serial.print(',');Serial.println(h.rows);
  Serial.print("#period_us,");Serial.print(h.periodUs);Serial.print("\n#motor_id,");
  Serial.print(h.motorId);Serial.print("\n#torque_nm,");Serial.println(h.torqueNm,6);
  Serial.println("time_us,command_torque_nm");
  for(uint32_t i=0;i<h.rows;i++) {
    Sample s; if(f.read(&s,sizeof(s))!=sizeof(s)) break;
    Serial.print(s.timeUs);Serial.print(',');Serial.println(s.commandNm,6);
  }
  Serial.print("TRANS_END,");Serial.print(latestFile);Serial.print(',');Serial.println(h.rows);
  f.close();
}
} // namespace SaveData
