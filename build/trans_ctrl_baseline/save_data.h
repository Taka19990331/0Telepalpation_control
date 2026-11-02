#pragma once
#include <Arduino.h>
#include <SdFat.h>
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
DMAMEM static Sample samples[RING_SIZE];
static volatile uint32_t head=0, tail=0;
static volatile bool recording=false, finalizeRequest=false;
static volatile uint32_t statusCode=0;
static SdFs sd;
static FsFile logFile;
static char latestFile[16]={};
static uint32_t savedRows=0;
static bool cardReady=false;
constexpr char CSV_HEADER[]="timestamp_us,position_rad,velocity_raw_rad_s,velocity_filtered_rad_s,feedback_torque_Nm,torque_bias_Nm,feedback_age_us,friction_model_Nm,rfob_input_Nm,rfob_filter_state_Nm,external_torque_est_Nm,torque_RFOB_Nm,torque_FF_Nm,torque_cmd_unsat_Nm,torque_cmd_Nm,K_R,K_F,control_enabled,torque_saturated,motor_error,fault_code,tx_queue_count,isr_interval_us,previous_isr_duration_us,can_snapshot_time_us,can_esr1,can_ecr,can_write_success,can_write_failure";
inline bool begin() {
  cardReady=sd.begin(SdioConfig(FIFO_SDIO));
  if(cardReady) for(int n=int(ControlConfig::LOG_FILE_COUNT)-1;n>=0;--n) {
    snprintf(latestFile,sizeof(latestFile),"TC%04d.CSV",n);
    if(sd.exists(latestFile)) return true;
  }
  latestFile[0]=0; return cardReady;
}
inline bool start() {
  if(!cardReady || recording || finalizeRequest) return false;
  char name[16]; bool found=false;
  for(unsigned n=0;n<ControlConfig::LOG_FILE_COUNT;++n) {
    snprintf(name,sizeof(name),"TC%04u.CSV",n);
    if(!sd.exists(name)) { found=true; break; }
  }
  if(!found || !logFile.open(name,O_WRONLY|O_CREAT|O_EXCL)) return false;
  strcpy(latestFile,name);
  logFile.println("#design,AK80-8_transparency_v1.2");
  using namespace ControlConfig;
  auto meta=[&](const char *name,double value) { logFile.print(name); logFile.print(','); logFile.println(value,9); };
  meta("#period_us",CONTROL_PERIOD_US); meta("#log_period_us",LOG_PERIOD_US);
  meta("#motor_id",MOTOR_ID); meta("#can_bus",CAN_BUS); meta("#can_baud",CAN_BAUD);
  meta("#J",J); meta("#velocity_lpf_hz",VELOCITY_HZ); meta("#rfob_q_hz",Q_HZ);
  meta("#torque_limit_Nm",TORQUE_LIMIT); meta("#feedback_timeout_us",FEEDBACK_TIMEOUT_US);
  meta("#positive_tc",POSITIVE.tc); meta("#positive_ts",POSITIVE.ts); meta("#positive_vs",POSITIVE.vs); meta("#positive_b",POSITIVE.b);
  meta("#negative_tc",NEGATIVE.tc); meta("#negative_ts",NEGATIVE.ts); meta("#negative_vs",NEGATIVE.vs); meta("#negative_b",NEGATIVE.b);
  meta("#initial_K_R",INITIAL_K_R); meta("#initial_K_F",INITIAL_K_F); meta("#initial_control_enabled",INITIAL_ENABLED);
  meta("#position_min_rad",POSITION_MIN); meta("#position_max_rad",POSITION_MAX);
  meta("#velocity_min_rad_s",VELOCITY_MIN); meta("#velocity_max_rad_s",VELOCITY_MAX);
  meta("#torque_encode_min_Nm",TORQUE_MIN); meta("#torque_encode_max_Nm",TORQUE_MAX);
  meta("#log_ring_rows",LOG_RING_ROWS); meta("#log_batch_rows",LOG_BATCH_ROWS);
  logFile.println(CSV_HEADER);
  if(logFile.getWriteError() || !logFile.sync()) { logFile.close(); return false; }
  head=tail=0; savedRows=0; statusCode=0; recording=true; return true;
}
inline bool record(const Sample &s) {
  if(!recording || head-tail>=RING_SIZE) return false;
  samples[head%RING_SIZE]=s; asm volatile("" ::: "memory"); ++head; CanDiagnostics::ringMax=max(CanDiagnostics::ringMax,uint32_t(head-tail)); return true;
}
inline void finish(uint32_t status) { statusCode=status; recording=false; finalizeRequest=true; }
inline void writeDiagnostics(Print &out) {
  using namespace CanDiagnostics;
  auto value=[&](const char *key,uint32_t v) { out.print("#diag,"); out.print(key); out.print(','); out.println(v); };
  value("write_success",success); value("write_failure",failures); value("tx_queue_max_observed",queueMax);
  value("rx_gap_max_us",rxGapMax); value("isr_interval_max_us",intervalMax);
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
    out.print("#diag,failure_torque_Nm,"); out.println(f.torque,9);
  }
}
inline bool service() {
  if(!recording && !finalizeRequest) return false;
  uint32_t count=head-tail;
  if(count>=BATCH_ROWS || finalizeRequest) {
    if(count>BATCH_ROWS) count=BATCH_ROWS;
    for(uint32_t i=0;i<count;++i) {
      const Sample &s=samples[tail%RING_SIZE]; const auto &v=s.signals;
      char line[ControlConfig::CSV_BUFFER_BYTES];
      const int n=snprintf(line,sizeof(line),"%lu,%.7g,%.7g,%.7g,%.7g,%.7g,%lu,%.7g,%.7g,%.7g,%.7g,%.7g,%.7g,%.7g,%.7g,%.7g,%.7g,%u,%u,%u,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
        (unsigned long)s.timestamp,s.position,s.rawVelocity,v.velocity,s.torque,s.bias,(unsigned long)s.age,
        v.friction,v.input,v.state,v.external,v.torqueR,v.torqueF,v.unsaturated,v.command,s.Kr,s.Kf,
        unsigned(s.enabled),unsigned(v.saturated),unsigned(s.error),(unsigned long)s.fault,(unsigned long)s.txQueue,(unsigned long)s.isrInterval,(unsigned long)s.previousIsrUs,(unsigned long)s.diagUs,(unsigned long)s.esr,(unsigned long)s.ecr,(unsigned long)s.txSuccess,(unsigned long)s.txFailure);
      if(n<=0 || size_t(n)>=sizeof(line) || logFile.write(line,n)!=size_t(n)) {
        noInterrupts(); statusCode=4; recording=false; tail=head; finalizeRequest=true; interrupts(); break;
      }
      ++tail; ++savedRows;
    }
  }
  // On an SD error, give loop a chance to latch stopRun(4) before the footer.
  if(finalizeRequest && tail==head && CanDiagnostics::stopReason!=0) {
    CanProtocol::serviceDiagnostics();
    writeDiagnostics(logFile);
    logFile.print("#rows,"); logFile.println(savedRows);
    logFile.print("#status,"); logFile.println(statusCode);
    const bool ok=!logFile.getWriteError() && logFile.sync(); logFile.close(); finalizeRequest=false;
    Serial.print("SAVED,"); Serial.print(latestFile); Serial.print(",STATUS,"); Serial.println(ok?statusCode:5);
    return true;
  }
  return false;
}
inline void dumpCsv() {
  if(recording || finalizeRequest || !latestFile[0]) return;
  FsFile f; if(!f.open(latestFile,O_RDONLY)) return;
  // Count data rows so files remain downloadable after a reboot.
  uint32_t rows=0; bool first=true, data=false;
  while(f.available()) { const char c=f.read(); if(first) { data=c>='0'&&c<='9'; first=false; } if(c=='\n') { if(data) ++rows; first=true; } }
  f.seekSet(0);
  Serial.print("TRANS_BEGIN,"); Serial.print(latestFile); Serial.print(','); Serial.println(rows);
  // Stop-and-wait chunks prevent USB CDC receive overruns on large CSV files.
  char buf[ControlConfig::CSV_BUFFER_BYTES]; uint32_t chunk=0, lines=0;
  while(f.available()) {
    const int n=f.read(buf,sizeof(buf)); if(n<=0) break;
    for(int i=0;i<n;++i) if(buf[i]=='\n') ++lines;
    Serial.write((const uint8_t*)buf,n);
    if(lines>=32 || !f.available()) {
      Serial.print("TRANS_CHUNK,"); Serial.println(chunk); Serial.flush();
      const uint32_t deadline=millis()+60000; bool accepted=false;
      while(int32_t(millis()-deadline)<0 && !accepted) {
        while(Serial.available()) {
          const char c=Serial.read();
          if(c=='A') { accepted=true; break; }
        }
      }
      if(!accepted) { Serial.print("TRANS_ABORT,"); Serial.print(chunk); Serial.println(",ACK_TIMEOUT"); f.close(); return; }
      ++chunk; lines=0;
    }
  }
  Serial.print("TRANS_END,"); Serial.print(latestFile); Serial.print(','); Serial.println(rows); f.close();
}
}





