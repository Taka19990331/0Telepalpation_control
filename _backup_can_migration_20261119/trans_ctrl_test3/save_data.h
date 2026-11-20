#pragma once
#include <Arduino.h>
#include "control_config.h"
#include "C:/0Telepalpation_control/SDlog/LogProtocol.h"
#define SDLOG_FIRMWARE_ID "trans_ctrl_test3"
#include <stddef.h>
#if TRANS_ENABLE_SD_LOGGING
#include <SdFat.h>
#endif
#include "can_protocol.h"
#include "control_algorithm.h"
namespace SaveData {
constexpr uint32_t RING_SIZE=ControlConfig::LOG_RING_ROWS, BATCH_ROWS=ControlConfig::LOG_BATCH_ROWS;
struct Sample {
  uint32_t timestamp, age, txQueue, isrInterval, previousIsrUs;
  uint32_t diagUs, esr, ecr, txSuccess, txFailure;
  float position, rawVelocity, torque, bias;
  ControlAlgorithm::Signals signals;
  float Kr, Kf;
  uint8_t enabled, error; uint32_t fault;
};
// Versioned little-endian Teensy ABI, checked by both the compiler and converter.
struct Record { uint32_t sequence; Sample sample; uint32_t crc; };
static_assert(sizeof(Sample)==112 && sizeof(Record)==120,"Update binary format version");
static_assert(offsetof(Sample,signals)==56 && offsetof(Sample,Kr)==96 && offsetof(Sample,fault)==108,"Binary ABI changed");
#if TRANS_ENABLE_SD_LOGGING
DMAMEM static Sample samples[RING_SIZE];
static Record batch[BATCH_ROWS]; // Foreground CRC and disk staging, never used by ISR.
#endif
static volatile uint32_t head=0, tail=0;
static volatile bool recording=false, finalizeRequest=false;
static volatile uint32_t statusCode=0;
#if TRANS_ENABLE_SD_LOGGING
static SdFs sd;
static FsFile logFile;
static char latestFile[16]={};
static uint32_t savedRows=0;
static bool cardReady=false;
#endif
constexpr char CSV_HEADER[]="timestamp_us,position_rad,velocity_raw_rad_s,velocity_filtered_rad_s,feedback_torque_Nm,torque_bias_Nm,feedback_age_us,friction_model_Nm,rfob_input_Nm,rfob_filter_state_Nm,external_torque_est_Nm,torque_RFOB_Nm,torque_FF_Nm,torque_cmd_unsat_Nm,torque_cmd_Nm,K_R,K_F,control_enabled,torque_saturated,motor_error,fault_code,tx_queue_count,isr_interval_us,previous_isr_duration_us,can_snapshot_time_us,can_esr1,can_ecr,can_write_success,can_write_failure";
inline void writeDiagnostics(Print &out) {
  using namespace CanDiagnostics;
  auto value=[&](const char *key,uint32_t v) { out.print("#diag,"); out.print(key); out.print(','); out.println(v); };
  value("write_success",success); value("write_failure",failures); value("tx_queue_max_observed",queueMax);
  value("rx_gap_max_us",rxGapMax); value("isr_interval_max_us",intervalMax);
  value("sd_logging_enabled",ControlConfig::SD_LOGGING);
  value("isr_interval_min_us",intervalMin==UINT32_MAX?0:intervalMin);
  value("rx_gap_min_us",rxGapMin==UINT32_MAX?0:rxGapMin);
  value("rx_callback_max_us",rxIsrMax); value("rx_frames",rxFrames); value("rx_accepted",rxAccepted);
  value("rx_rejected",rxRejected); value("rx_processed",rxProcessed);
  value("rx_startup_discarded",rxStartupDiscarded);
  value("rx_queue_rows",ControlConfig::RX_QUEUE_ROWS); value("rx_budget",ControlConfig::RX_BUDGET);
  value("rx_queue_max",rxQueueMax); value("rx_queue_overflow",rxOverflow);
  value("rx_queue_pending",CanProtocol::rxHead-CanProtocol::rxTail);
  value("rx_hardware_overruns",rxHardwareOverruns); value("rx_stale",rxStale);
  value("rx_abnormal_interval",rxAbnormal); value("rx_budget_hits",rxBudgetHits);
  value("log_ring_bytes",ControlConfig::SD_LOGGING?sizeof(Sample)*RING_SIZE:0);
  value("log_dropped",logDropped); value("sd_write_failures",sdWriteFailures);
  value("log_pending_rows",head-tail);
  value("isr_duration_max_us",durationMax); value("interval_over_500us",intervalOverruns);
  value("execution_over_500us",executionOverruns); value("sd_service_max_us",sdMaxUs); value("ring_max_rows",ringMax);
  value("error_history_records",errorRecords); value("esr_observed_or",errorBits);
  value("tx_error_max_observed",txErrorMax); value("rx_error_max_observed",rxErrorMax);
  value("bus_off_observed",busOffObserved);
  value("stop_time_us",stopUs); value("stop_reason",stopReason);
  value("stop_zero_write_accepted",stopZero); value("stop_disable_write_accepted",stopDisable);
  auto bus=[&](const char *label,const Snapshot &b) {
    out.print("#diag_bus,"); out.print(label); out.print(','); out.print(b.us);
    out.print(','); out.print(b.queue); out.print(','); out.print(b.esr);
    out.print(','); out.print(b.ecr); out.print(','); out.print(b.freeMB);
    out.print(','); out.println(b.pendingMB);
  };
  out.println("#diag_bus_columns,label,time_us,tx_queue,esr1,ecr,free_tx_mb,pending_tx_mb");
  bus("stop",latest);
  value("first_failure_valid",firstFailure.valid);
  if(firstFailure.valid) {
    const auto &f=firstFailure;
    bus("first_failure",f.bus);
    value("failure_kind",f.kind); value("failure_feedback_age_us",f.age);
    value("failure_interval_us",f.interval); value("failure_previous_isr_us",f.previousDuration);
    value("failure_entry_to_write_us",f.toWrite); value("failure_isr_duration_us",f.duration);
    value("failure_count_at_event",f.failures);
    value("failure_rx_pending",f.rxPending); value("failure_rx_overflow",f.rxOverflow); value("failure_rx_queue_max",f.rxMax);
    value("failure_log_pending",f.logPending); value("failure_log_dropped",f.logDropped);
    out.print("#diag,failure_torque_Nm,"); out.println(f.torque,9);
  }
}

#if TRANS_ENABLE_SD_LOGGING
inline bool begin() {
  cardReady=sd.begin(SdioConfig(FIFO_SDIO));
  if(cardReady) for(int n=int(ControlConfig::LOG_FILE_COUNT)-1;n>=0;--n) {
    snprintf(latestFile,sizeof(latestFile),"TC%04d.BIN",n);
    if(sd.exists(latestFile)) {
      FsFile probe;
      if(probe.open(latestFile,O_RDONLY)) {
        char line[64]; bool compatible=false;
        while(probe.fgets(line,sizeof(line))>0) {
          if(sizeof(Record)==152 && (!strcmp(line,"#binary,TCBIN3,152\n") || !strcmp(line,"#binary,TCBIN3,152\r\n"))) { compatible=true; break; }
          if(sizeof(Record)==120 && (!strcmp(line,"#binary,TCBIN2,120\n") || !strcmp(line,"#binary,TCBIN2,120\r\n"))) { compatible=true; break; }
        }
        probe.close(); if(compatible) return true;
      }
    }
  }
  latestFile[0]=0; return cardReady;
}
inline bool start() {
  if(!cardReady || recording || finalizeRequest) return false;
  char name[16]; bool found=false;
  for(unsigned n=0;n<ControlConfig::LOG_FILE_COUNT;++n) {
    snprintf(name,sizeof(name),"TC%04u.BIN",n);
    if(!sd.exists(name)) { found=true; break; }
  }
  if(!found || !logFile.open(name,O_WRONLY|O_CREAT|O_EXCL)) return false;
  strcpy(latestFile,name);
  logFile.println("#design,AK80-8_transparency_v2_binary");
  using namespace ControlConfig;
  auto meta=[&](const char *name,double value) { logFile.print(name); logFile.print(','); logFile.println(value,9); };
  logFile.println("#friction_model,tanh_coulomb_viscous");
  meta("#omega_0_rad_s",OMEGA_0);
  meta("#period_us",CONTROL_PERIOD_US); meta("#log_period_us",LOG_PERIOD_US);
  meta("#motor_id",MOTOR_ID); meta("#can_bus",CAN_BUS); meta("#can_baud",CAN_BAUD);
  meta("#J",J); meta("#velocity_lpf_hz",VELOCITY_HZ); meta("#rfob_q_hz",Q_HZ);
  meta("#torque_limit_Nm",TORQUE_LIMIT); meta("#feedback_timeout_us",FEEDBACK_TIMEOUT_US);
  meta("#positive_tc",POSITIVE.tc); meta("#positive_b",POSITIVE.b);
  meta("#negative_tc",NEGATIVE.tc); meta("#negative_b",NEGATIVE.b);
  meta("#initial_K_R",INITIAL_K_R); meta("#initial_K_F",INITIAL_K_F); meta("#initial_control_enabled",INITIAL_ENABLED);
  meta("#position_min_rad",POSITION_MIN); meta("#position_max_rad",POSITION_MAX);
  meta("#velocity_min_rad_s",VELOCITY_MIN); meta("#velocity_max_rad_s",VELOCITY_MAX);
  meta("#torque_encode_min_Nm",TORQUE_MIN); meta("#torque_encode_max_Nm",TORQUE_MAX);
  meta("#log_ring_rows",LOG_RING_ROWS); meta("#log_batch_rows",LOG_BATCH_ROWS);
  meta("#sd_logging_enabled",SD_LOGGING); meta("#binary_record_bytes",sizeof(Record));
  meta("#rx_queue_rows",RX_QUEUE_ROWS); meta("#rx_budget",RX_BUDGET);
  meta("#rx_stale_us",RX_STALE_US); meta("#rx_interval_min_us",RX_INTERVAL_MIN_US); meta("#rx_interval_max_us",RX_INTERVAL_MAX_US);
  logFile.println("#binary,TCBIN2,120");
  if(logFile.getWriteError() || !logFile.sync()) { logFile.close(); return false; }
  head=tail=0; savedRows=0; statusCode=0; recording=true; return true;
}
inline bool record(const Sample &s) {
  if(!recording || head-tail>=RING_SIZE) { ++CanDiagnostics::logDropped; return false; }
  samples[head%RING_SIZE]=s; asm volatile("" ::: "memory"); ++head; CanDiagnostics::ringMax=max(CanDiagnostics::ringMax,uint32_t(head-tail)); return true;
}
inline void finish(uint32_t status) { statusCode=status; recording=false; finalizeRequest=true; }
inline uint32_t crc32(const void *ptr,size_t bytes) {
  uint32_t crc=0xffffffffU;
  const auto *p=static_cast<const uint8_t*>(ptr);
  while(bytes--) { crc^=*p++; for(unsigned j=0;j<8;++j) crc=(crc>>1)^(0xedb88320U & (0U-(crc&1U))); }
  return ~crc;
}
inline void seal(Record &r) { r.crc=crc32(&r,offsetof(Record,crc)); }
inline bool valid(const Record &r) { return r.crc==crc32(&r,offsetof(Record,crc)); }
inline bool service() {
  if(!recording && !finalizeRequest) return false;
  uint32_t count=head-tail;
  if(count>=BATCH_ROWS || finalizeRequest) {
    count=min(count,BATCH_ROWS);
    // Keep tail pinned while copying/writing: producer cannot overwrite these rows.
    const uint32_t first=tail;
    for(uint32_t i=0;i<count;++i) {
      batch[i].sequence=savedRows+i; batch[i].sample=samples[(first+i)%RING_SIZE]; seal(batch[i]);
    }
    if(count && logFile.write(batch,count*sizeof(Record))!=count*sizeof(Record)) {
      noInterrupts(); ++CanDiagnostics::sdWriteFailures;
      CanDiagnostics::logDropped+=head-tail; statusCode=4; recording=false; tail=head; finalizeRequest=true; interrupts();
    } else { asm volatile("" ::: "memory"); tail=first+count; savedRows+=count; }
  }
  if(finalizeRequest && tail==head && CanDiagnostics::stopReason!=0) {
    Record end={}; end.sequence=UINT32_MAX; end.sample.timestamp=savedRows; end.sample.fault=statusCode; seal(end);
    const bool ended=logFile.write(&end,sizeof(end))==sizeof(end);
    CanProtocol::serviceDiagnostics(); writeDiagnostics(logFile);
    logFile.print("#rows,"); logFile.println(savedRows);
    logFile.print("#status,"); logFile.println(statusCode);
    const bool ok=ended && !logFile.getWriteError() && logFile.sync(); logFile.close(); finalizeRequest=false;
    Serial.print("SAVED,"); Serial.print(latestFile); Serial.print(",STATUS,"); Serial.println(ok?statusCode:5);
    return true;
  }
  return false;
}
inline bool csvRow(Print &out,const Sample &s) {
  const auto &v=s.signals;
  char line[ControlConfig::CSV_BUFFER_BYTES];
  const int n=snprintf(line,sizeof(line),"%lu,%.7g,%.7g,%.7g,%.7g,%.7g,%lu,%.7g,%.7g,%.7g,%.7g,%.7g,%.7g,%.7g,%.7g,%.7g,%.7g,%u,%u,%u,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
    (unsigned long)s.timestamp,s.position,s.rawVelocity,v.velocity,s.torque,s.bias,(unsigned long)s.age,
    v.friction,v.input,v.state,v.external,v.torqueR,v.torqueF,v.unsaturated,v.command,s.Kr,s.Kf,
    unsigned(s.enabled),unsigned(v.saturated),unsigned(s.error),(unsigned long)s.fault,(unsigned long)s.txQueue,(unsigned long)s.isrInterval,(unsigned long)s.previousIsrUs,(unsigned long)s.diagUs,(unsigned long)s.esr,(unsigned long)s.ecr,(unsigned long)s.txSuccess,(unsigned long)s.txFailure);
  return n>0 && size_t(n)<sizeof(line) && out.write((const uint8_t*)line,n)==size_t(n);
}
inline bool ack(uint32_t chunk) {
  Serial.print("TRANS_CHUNK,"); Serial.println(chunk); Serial.flush();
  const uint32_t start=millis();
  while(uint32_t(millis()-start)<60000) if(Serial.available() && Serial.read()=='A') return true;
  Serial.print("TRANS_ABORT,"); Serial.print(chunk); Serial.println(",ACK_TIMEOUT"); return false;
}
inline bool readHeader(FsFile &f,Print *out=nullptr) {
  char line[ControlConfig::CSV_BUFFER_BYTES];
  while(f.fgets(line,sizeof(line))>0) {
    if(!strcmp(line,"#binary,TCBIN2,120\n") || !strcmp(line,"#binary,TCBIN2,120\r\n")) return true;
    if(out) out->print(line);
  }
  return false;
}
inline void identifyCommon() {
  Serial.print("LOG,1,IDENTIFY,"); Serial.print(SDLOG_FIRMWARE_ID); Serial.print(',');
  Serial.println(sizeof(Record)==152 ? "TCBIN3" : "TCBIN2");
}
inline void sendCommon(bool previous=false) {
  if(recording || finalizeRequest || !latestFile[0]) { Serial.println("LOG,1,ERROR,NO_FILE_OR_BUSY"); return; }  char selectedName[16]; strcpy(selectedName,latestFile);
  if(previous) {
    int current=atoi(latestFile+2); bool found=false;
    for(int n=current-1;n>=0;--n) {
      snprintf(selectedName,sizeof(selectedName),"TC%04d.BIN",n);
      if(!sd.exists(selectedName)) continue;
      FsFile probe; if(!probe.open(selectedName,O_RDONLY)) continue;
      const bool compatible=readHeader(probe); probe.close(); if(compatible) { found=true; break; }
    }
    if(!found) { Serial.println("LOG,1,ERROR,NO_PREVIOUS_FILE"); return; }
  }
  FsFile f; if(!f.open(selectedName,O_RDONLY)) { Serial.println("LOG,1,ERROR,OPEN"); return; }
  if(!readHeader(f)) { f.close(); Serial.println("LOG,1,ERROR,HEADER"); return; }
  Record r; uint32_t rows=0; bool complete=false;
  while(f.read(&r,sizeof(r))==sizeof(r) && valid(r)) {
    if(r.sequence==UINT32_MAX) { complete=r.sample.timestamp==rows; break; }
    if(r.sequence!=rows) break; ++rows;
  }
  if(complete) {
    char line[ControlConfig::CSV_BUFFER_BYTES], expectedRows[40], expectedStatus[40];
    snprintf(expectedRows,sizeof(expectedRows),"#rows,%lu",(unsigned long)rows);
    snprintf(expectedStatus,sizeof(expectedStatus),"#status,%lu",(unsigned long)r.sample.fault);
    bool hasRows=false,hasStatus=false;
    while(f.fgets(line,sizeof(line))>0) {
      line[strcspn(line,"\r\n")]=0;
      hasRows|=!strcmp(line,expectedRows); hasStatus|=!strcmp(line,expectedStatus);
    }
    complete=hasRows && hasStatus;
  }
  if(!complete) { f.close(); Serial.println("LOG,1,ERROR,INCOMPLETE_OR_CORRUPT"); return; }
  f.seekSet(0);
  SDlog::sendFile(f,SDLOG_FIRMWARE_ID,sizeof(Record)==152?"TCBIN3":"TCBIN2",selectedName);
  f.close();
}
inline void dumpCsv() {
  if(recording || finalizeRequest || !latestFile[0]) return;
  FsFile f; if(!f.open(latestFile,O_RDONLY)) return;
  if(!readHeader(f)) { Serial.println("ERROR,binary header"); return; }
  const uint32_t dataOffset=f.curPosition();
  Record r; uint32_t rows=0; bool complete=false;
  // Validate the whole file before transfer, also after reboot.
  while(f.read(&r,sizeof(r))==sizeof(r) && valid(r)) {
    if(r.sequence==UINT32_MAX) { complete=r.sample.timestamp==rows; break; }
    if(r.sequence!=rows) break;
    ++rows;
  }
  if(complete) {
    char line[ControlConfig::CSV_BUFFER_BYTES], expectedRows[40], expectedStatus[40];
    snprintf(expectedRows,sizeof(expectedRows),"#rows,%lu",(unsigned long)rows);
    snprintf(expectedStatus,sizeof(expectedStatus),"#status,%lu",(unsigned long)r.sample.fault);
    bool hasRows=false,hasStatus=false;
    while(f.fgets(line,sizeof(line))>0) {
      line[strcspn(line,"\r\n")]=0;
      hasRows|=!strcmp(line,expectedRows); hasStatus|=!strcmp(line,expectedStatus);
    }
    complete=hasRows && hasStatus;
  }
  if(!complete) { Serial.println("ERROR,incomplete or corrupt binary log; use PC converter for recovery"); return; }
  char csvName[16]; strcpy(csvName,latestFile); strcpy(strrchr(csvName,'.'),".CSV");
  Serial.print("TRANS_BEGIN,"); Serial.print(csvName); Serial.print(','); Serial.println(rows);
  f.seekSet(0); readHeader(f,&Serial); Serial.println(CSV_HEADER);
  f.seekSet(dataOffset); uint32_t chunk=0;
  for(uint32_t i=0;i<rows;++i) {
    if(f.read(&r,sizeof(r))!=sizeof(r) || !valid(r) || r.sequence!=i || !csvRow(Serial,r.sample)) {
      Serial.println("TRANS_ABORT,0,READ_ERROR"); return;
    }
    if((i+1)%32==0 && !ack(chunk++)) return;
  }
  f.read(&r,sizeof(r)); // Skip end record; remaining bytes are textual diagnostics.
  char buf[ControlConfig::CSV_BUFFER_BYTES];
  while(f.available()) { const int n=f.read(buf,sizeof(buf)); if(n<=0) break; Serial.write((const uint8_t*)buf,n); }
  if(!ack(chunk)) return;
  Serial.print("TRANS_END,"); Serial.print(csvName); Serial.print(','); Serial.println(rows);
}
#else
// SD is completely absent: no card access, ring allocation, formatting or writes.
inline bool begin() { return true; }
inline bool start() { if(recording) return false; statusCode=0; recording=true; return true; }
inline bool record(const Sample &) { return true; }
inline void finish(uint32_t status) { statusCode=status; recording=false; }
inline bool service() { return false; }
inline void dumpCsv() { Serial.println("ERROR,SD logging disabled; use STATUS diagnostics"); }
#endif
}
