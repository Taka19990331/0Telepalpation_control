#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <IntervalTimer.h>
#include <SdFat.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "C:/0Telepalpation_control/SDlog/LogProtocol.h"

// Two-motor external PD position hold for gravity/load torque measurement.
// The target for each motor is its first valid feedback angle after `S`.
// No encoder zero command or feedback-angle offset is applied.

namespace Config {
constexpr uint8_t MOTOR_ID[2] = {1, 2};
constexpr uint32_t CAN_BAUD = 1000000;
constexpr uint32_t CONTROL_PERIOD_US = 500; // 2 kHz
constexpr uint32_t FEEDBACK_TIMEOUT_US = 100000;
constexpr uint32_t STARTUP_TIMEOUT_US = 1000000;
constexpr uint32_t LOG_RING_ROWS = 2048;
constexpr uint8_t LOG_BATCH_ROWS = 32;
constexpr unsigned LOG_FILE_COUNT = 10000;
constexpr uint32_t SERIAL_BAUD = 115200;

// AK80 standard MIT encoding ranges.
constexpr float POSITION_MIN = -12.5f, POSITION_MAX = 12.5f;
constexpr float VELOCITY_MIN = -37.5f, VELOCITY_MAX = 37.5f;
constexpr float TORQUE_MIN = -32.0f, TORQUE_MAX = 32.0f;
constexpr float MIT_KP_MIN = 0.0f, MIT_KP_MAX = 500.0f;
constexpr float MIT_KD_MIN = 0.0f, MIT_KD_MAX = 5.0f;

// External PD gains. Angle error is in degrees; velocity is rad/s.
constexpr float KP_NM_PER_DEG = 0.5f; //0.00278
constexpr float KD_NM_PER_RAD_S = 0.1f;
constexpr float TORQUE_LIMIT_NM = 3.0f;
}

using Can1Type = FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16>;
using Can2Type = FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16>;
static Can1Type can1;
static Can2Type can2;
static IntervalTimer controlTimer;
static SdFs sd;
static FsFile logFile;

struct Feedback {
  float angleRad = 0.0f;
  float velocityRadS = 0.0f;
  float torqueNm = 0.0f;
  uint32_t rxUs = 0;
  uint8_t error = 0;
  bool valid = false;
};
static volatile Feedback feedback[2];
static volatile bool captureFeedback = false;

struct Sample {
  uint32_t timeUs;
  float angleRad[2];
  float velocityRadS[2];
  float commandNm[2];
  float feedbackNm[2];
};
DMAMEM static Sample logRing[Config::LOG_RING_ROWS];
static volatile uint32_t logHead = 0, logTail = 0;
static volatile bool recording = false, finalizeRequested = false;
static volatile uint32_t logDropped = 0;

static volatile bool active = false;
static volatile uint32_t runStartUs = 0;
static volatile uint32_t faultCode = 0;
static volatile float targetAngleRad[2] = {0.0f, 0.0f};
static volatile float commandTorqueNm[2] = {0.0f, 0.0f};
static volatile bool stopRequested = false;
static char latestFile[16] = {};
static bool cardReady = false;
static bool timerReady = false;

static uint32_t encode(float value, float low, float high, uint8_t bits) {
  value = constrain(value, low, high);
  return uint32_t((value - low) * float((1UL << bits) - 1) / (high - low));
}

static float decode(uint32_t value, float low, float high, uint8_t bits) {
  return float(value) * (high - low) / float((1UL << bits) - 1) + low;
}

static bool sendFrame(uint8_t index, const CAN_message_t &message) {
  return index == 0 ? can1.write(message) != 0 : can2.write(message) != 0;
}

static bool sendSpecial(uint8_t index, uint8_t code) {
  CAN_message_t message = {};
  message.id = Config::MOTOR_ID[index];
  message.len = 8;
  for (uint8_t i = 0; i < 7; ++i) message.buf[i] = 0xff;
  message.buf[7] = code;
  return sendFrame(index, message);
}

static bool sendCommand(uint8_t index, float torqueNm) {
  if (!isfinite(torqueNm)) torqueNm = 0.0f;
  torqueNm = constrain(torqueNm, -Config::TORQUE_LIMIT_NM, Config::TORQUE_LIMIT_NM);
  const uint32_t p = encode(0.0f, Config::POSITION_MIN, Config::POSITION_MAX, 16);
  const uint32_t v = encode(0.0f, Config::VELOCITY_MIN, Config::VELOCITY_MAX, 12);
  const uint32_t kp = encode(0.0f, Config::MIT_KP_MIN, Config::MIT_KP_MAX, 12);
  const uint32_t kd = encode(0.0f, Config::MIT_KD_MIN, Config::MIT_KD_MAX, 12);
  const uint32_t t = encode(torqueNm, Config::TORQUE_MIN, Config::TORQUE_MAX, 12);
  CAN_message_t message = {};
  message.id = Config::MOTOR_ID[index];
  message.len = 8;
  message.buf[0] = p >> 8;
  message.buf[1] = p;
  message.buf[2] = v >> 4;
  message.buf[3] = ((v & 0x0f) << 4) | (kp >> 8);
  message.buf[4] = kp;
  message.buf[5] = kd >> 4;
  message.buf[6] = ((kd & 0x0f) << 4) | (t >> 8);
  message.buf[7] = t;
  return sendFrame(index, message);
}

static void receiveForMotor(uint8_t index, const CAN_message_t &message) {
  if (!captureFeedback || message.flags.remote || message.flags.extended ||
      message.len != 8 || message.buf[0] != Config::MOTOR_ID[index]) return;
  volatile Feedback &f = feedback[index];
  f.angleRad = decode((uint32_t(message.buf[1]) << 8) | message.buf[2],
                      Config::POSITION_MIN, Config::POSITION_MAX, 16);
  f.velocityRadS = decode((uint32_t(message.buf[3]) << 4) | (message.buf[4] >> 4),
                          Config::VELOCITY_MIN, Config::VELOCITY_MAX, 12);
  f.torqueNm = decode(((uint32_t(message.buf[4]) & 0x0f) << 8) | message.buf[5],
                      Config::TORQUE_MIN, Config::TORQUE_MAX, 12);
  f.error = message.buf[7];
  f.rxUs = micros();
  f.valid = true;
}

static void receiveCan1(const CAN_message_t &message) { receiveForMotor(0, message); }
static void receiveCan2(const CAN_message_t &message) { receiveForMotor(1, message); }

static void configureCan(Can1Type &can) {
  can.begin();
  can.setBaudRate(Config::CAN_BAUD);
  can.setMaxMB(16);
  can.setMB(MB0, RX, STD);
  can.setMBFilter(REJECT_ALL);
  can.setMBFilter(MB0, ACCEPT_ALL);
}
static void configureCan(Can2Type &can) {
  can.begin();
  can.setBaudRate(Config::CAN_BAUD);
  can.setMaxMB(16);
  can.setMB(MB0, RX, STD);
  can.setMBFilter(REJECT_ALL);
  can.setMBFilter(MB0, ACCEPT_ALL);
}

static bool openNewLog() {
  if (!cardReady || recording || finalizeRequested) return false;
  char name[16];
  for (unsigned n = 0; n < Config::LOG_FILE_COUNT; ++n) {
    snprintf(name, sizeof(name), "GPD%04u.CSV", n);
    if (!sd.exists(name) && logFile.open(name, O_WRONLY | O_CREAT | O_EXCL)) {
      strcpy(latestFile, name);
      logFile.println("#firmware,gravity_estimate_PD");
      logFile.println("#schema,GRAVPD1");
      logFile.print("#control_period_us,"); logFile.println(Config::CONTROL_PERIOD_US);
      logFile.print("#motor1_id,"); logFile.println(Config::MOTOR_ID[0]);
      logFile.println("#motor1_can_bus,1");
      logFile.print("#motor2_id,"); logFile.println(Config::MOTOR_ID[1]);
      logFile.println("#motor2_can_bus,2");
      logFile.print("#kp_Nm_per_deg,"); logFile.println(Config::KP_NM_PER_DEG, 9);
      logFile.print("#kd_Nm_per_rad_s,"); logFile.println(Config::KD_NM_PER_RAD_S, 9);
      logFile.print("#torque_limit_Nm,"); logFile.println(Config::TORQUE_LIMIT_NM, 6);
      logFile.println("#MIT_position_command,0");
      logFile.println("#MIT_velocity_command,0");
      logFile.println("#MIT_kp,0");
      logFile.println("#MIT_kd,0");
      logFile.println("#angle_zeroing,none; target is each motor's first feedback angle after S");
      if (logFile.getWriteError() || !logFile.sync()) { logFile.close(); return false; }
      logHead = logTail = 0;
      logDropped = 0;
      recording = true;
      return true;
    }
  }
  return false;
}

static void appendSample(const Sample &sample) {
  if (!recording) return;
  if (logHead - logTail >= Config::LOG_RING_ROWS) {
    ++logDropped;
    stopRequested = true;
    return;
  }
  logRing[logHead % Config::LOG_RING_ROWS] = sample;
  asm volatile("" ::: "memory");
  ++logHead;
}

static bool writeSample(const Sample &s) {
  char row[240];
  const int n = snprintf(row, sizeof(row),
      "%lu,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g\n",
      (unsigned long)s.timeUs,
      s.angleRad[0], s.velocityRadS[0], s.commandNm[0], s.feedbackNm[0],
      s.angleRad[1], s.velocityRadS[1], s.commandNm[1], s.feedbackNm[1]);
  return n > 0 && size_t(n) < sizeof(row) &&
         logFile.write(reinterpret_cast<const uint8_t *>(row), size_t(n)) == size_t(n);
}

static void finalizeLog() {
  if (!finalizeRequested || logTail != logHead) return;
  logFile.print("#rows,"); logFile.println(logHead);
  logFile.print("#log_dropped_rows,"); logFile.println(logDropped);
  logFile.print("#stop_reason,"); logFile.println(faultCode);
  const bool ok = !logFile.getWriteError() && logFile.sync();
  logFile.close();
  finalizeRequested = false;
  Serial.print("SAVED,"); Serial.print(latestFile); Serial.print(",STATUS,"); Serial.println(ok ? faultCode : 99);
}

static void serviceLog() {
  if (!recording && !finalizeRequested) return;
  const uint32_t pending = logHead - logTail;
  if (recording && !finalizeRequested && pending < Config::LOG_BATCH_ROWS) return;
  uint8_t count = 0;
  while (logTail != logHead && count < Config::LOG_BATCH_ROWS) {
    Sample sample = logRing[logTail % Config::LOG_RING_ROWS];
    if (!writeSample(sample)) {
      const bool wasActive = active;
      noInterrupts();
      recording = false;
      finalizeRequested = true;
      faultCode = 4;
      active = false;
      captureFeedback = false;
      logDropped += logHead - logTail;
      logTail = logHead;
      interrupts();
      if (wasActive) {
        sendCommand(0, 0.0f); sendCommand(1, 0.0f);
        sendSpecial(0, 0xfd); sendSpecial(1, 0xfd);
      }
      break;
    }
    ++logTail;
    ++count;
  }
  if (count && !logFile.sync()) {
    const bool wasActive = active;
    noInterrupts(); recording = false; finalizeRequested = true; faultCode = 4; active = false;
    captureFeedback = false; logDropped += logHead - logTail; logTail = logHead; interrupts();
    if (wasActive) {
      sendCommand(0, 0.0f); sendCommand(1, 0.0f);
      sendSpecial(0, 0xfd); sendSpecial(1, 0xfd);
    }
  }
  finalizeLog();
}

static void stopFromIsr(uint32_t reason) {
  active = false;
  faultCode = reason;
  commandTorqueNm[0] = commandTorqueNm[1] = 0.0f;
  sendCommand(0, 0.0f);
  sendCommand(1, 0.0f);
  sendSpecial(0, 0xfd);
  sendSpecial(1, 0xfd);
  captureFeedback = false;
  recording = false;
  finalizeRequested = true;
}

static void controlTick() {
  if (!active) return;
  const uint32_t now = micros();
  Feedback f[2];
  for (uint8_t i = 0; i < 2; ++i) {
    f[i].angleRad = feedback[i].angleRad; f[i].velocityRadS = feedback[i].velocityRadS;
    f[i].torqueNm = feedback[i].torqueNm; f[i].rxUs = feedback[i].rxUs;
    f[i].error = feedback[i].error; f[i].valid = feedback[i].valid;
  }
  float tau[2] = {0.0f, 0.0f};
  for (uint8_t i = 0; i < 2; ++i) {
    if (!f[i].valid || uint32_t(now - f[i].rxUs) > Config::FEEDBACK_TIMEOUT_US) {
      stopFromIsr(6); return;
    }
    if (f[i].error != 0) { stopFromIsr(7); return; }
    if (!isfinite(f[i].angleRad) || !isfinite(f[i].velocityRadS) || !isfinite(f[i].torqueNm)) {
      stopFromIsr(8); return;
    }
    const float errorDeg = (float(targetAngleRad[i]) - f[i].angleRad) * 57.2957795131f;
    tau[i] = Config::KP_NM_PER_DEG * errorDeg - Config::KD_NM_PER_RAD_S * f[i].velocityRadS;
    if (!isfinite(tau[i])) { stopFromIsr(8); return; }
    tau[i] = constrain(tau[i], -Config::TORQUE_LIMIT_NM, Config::TORQUE_LIMIT_NM);
  }
  commandTorqueNm[0] = tau[0];
  commandTorqueNm[1] = tau[1];
  if (!sendCommand(0, tau[0]) || !sendCommand(1, tau[1])) { stopFromIsr(2); return; }

  Sample sample = {};
  sample.timeUs = now - runStartUs;
  for (uint8_t i = 0; i < 2; ++i) {
    sample.angleRad[i] = f[i].angleRad;
    sample.velocityRadS[i] = f[i].velocityRadS;
    sample.commandNm[i] = tau[i];
    sample.feedbackNm[i] = f[i].torqueNm;
  }
  appendSample(sample);
  if (stopRequested) stopFromIsr(3);
}

static bool waitForInitialFeedback(uint32_t started) {
  uint32_t lastZeroTx = 0;
  while (uint32_t(micros() - started) < Config::STARTUP_TIMEOUT_US) {
    const uint32_t now = micros();
    if (uint32_t(now - lastZeroTx) >= Config::CONTROL_PERIOD_US) {
      if (!sendCommand(0, 0.0f) || !sendCommand(1, 0.0f)) return false;
      lastZeroTx = now;
    }
    noInterrupts();
    Feedback f1, f2;
    f1.angleRad = feedback[0].angleRad; f1.velocityRadS = feedback[0].velocityRadS;
    f1.torqueNm = feedback[0].torqueNm; f1.rxUs = feedback[0].rxUs;
    f1.error = feedback[0].error; f1.valid = feedback[0].valid;
    f2.angleRad = feedback[1].angleRad; f2.velocityRadS = feedback[1].velocityRadS;
    f2.torqueNm = feedback[1].torqueNm; f2.rxUs = feedback[1].rxUs;
    f2.error = feedback[1].error; f2.valid = feedback[1].valid;
    interrupts();
    if (f1.valid && f2.valid) {
      if (f1.error || f2.error || !isfinite(f1.angleRad) || !isfinite(f2.angleRad)) return false;
      noInterrupts();
      targetAngleRad[0] = f1.angleRad;
      targetAngleRad[1] = f2.angleRad;
      runStartUs = micros();
      interrupts();
      logFile.print("#target_angle_m1_rad,"); logFile.println(f1.angleRad, 9);
      logFile.print("#target_angle_m2_rad,"); logFile.println(f2.angleRad, 9);
      logFile.println("time_us,m1_angle_rad,m1_velocity_rad_s,m1_command_torque_Nm,m1_feedback_torque_Nm,m2_angle_rad,m2_velocity_rad_s,m2_command_torque_Nm,m2_feedback_torque_Nm");
      if (logFile.getWriteError() || !logFile.sync()) return false;
      return true;
    }
    if (Serial.available()) {
      const int c = Serial.peek();
      if (c == 'x' || c == 'X') { Serial.read(); return false; }
    }
    yield();
  }
  return false;
}

static void startRun() {
  if (active || recording || finalizeRequested) { Serial.println("ERROR,BUSY"); return; }
  if (!timerReady) { Serial.println("ERROR,TIMER_NOT_READY"); return; }
  if (!openNewLog()) { Serial.println("ERROR,SD_OPEN"); return; }
  noInterrupts();
  for (uint8_t i = 0; i < 2; ++i) {
    feedback[i].angleRad = 0.0f; feedback[i].velocityRadS = 0.0f;
    feedback[i].torqueNm = 0.0f; feedback[i].rxUs = 0;
    feedback[i].error = 0; feedback[i].valid = false;
  }
  captureFeedback = true;
  stopRequested = false;
  faultCode = 0;
  interrupts();
  const bool enabled1 = sendSpecial(0, 0xfc);
  const bool enabled2 = sendSpecial(1, 0xfc);
  const bool enabled = enabled1 && enabled2;
  const bool ready = enabled && waitForInitialFeedback(micros());
  if (!ready) {
    sendCommand(0, 0.0f); sendCommand(1, 0.0f);
    sendSpecial(0, 0xfd); sendSpecial(1, 0xfd);
    noInterrupts(); captureFeedback = false; recording = false; finalizeRequested = true;
    if (!faultCode) faultCode = 6;
    interrupts();
    serviceLog();
    Serial.println("ERROR,STARTUP_FEEDBACK");
    return;
  }
  noInterrupts();
  logHead = logTail = 0;
  runStartUs = micros();
  active = true;
  interrupts();
  Serial.print("RUNNING,TARGET_M1_RAD,"); Serial.print(float(targetAngleRad[0]), 8);
  Serial.print(",TARGET_M2_RAD,"); Serial.println(float(targetAngleRad[1]), 8);
}

static void stopRun(uint32_t reason) {
  noInterrupts();
  const bool wasActive = active;
  const bool hadLog = recording || finalizeRequested;
  active = false;
  faultCode = reason;
  commandTorqueNm[0] = commandTorqueNm[1] = 0.0f;
  interrupts();
  if (!wasActive && !hadLog) return;
  if (wasActive) {
    sendCommand(0, 0.0f); sendCommand(1, 0.0f);
    sendSpecial(0, 0xfd); sendSpecial(1, 0xfd);
  }
  noInterrupts(); captureFeedback = false; recording = false; finalizeRequested = true; interrupts();
  serviceLog();
}

static void fetchLog(bool previous) {
  if (active || recording || finalizeRequested || !latestFile[0]) {
    Serial.println("LOG,1,ERROR,NO_FILE_OR_BUSY"); return;
  }
  if (previous) { Serial.println("LOG,1,ERROR,PREVIOUS_UNSUPPORTED"); return; }
  FsFile file;
  if (!file.open(latestFile, O_RDONLY)) { Serial.println("LOG,1,ERROR,OPEN"); return; }
  SDlog::sendFile(file, "gravity_estimate_PD", "GRAVPD1", latestFile);
  file.close();
}

void setup() {
  Serial.begin(Config::SERIAL_BAUD);
  configureCan(can1); configureCan(can2);
  can1.onReceive(MB0, receiveCan1); can1.enableMBInterrupt(MB0);
  can2.onReceive(MB0, receiveCan2); can2.enableMBInterrupt(MB0);
  NVIC_SET_PRIORITY(IRQ_CAN1, 192); NVIC_SET_PRIORITY(IRQ_CAN2, 192);
  cardReady = sd.begin(SdioConfig(FIFO_SDIO));
  controlTimer.priority(192);
  timerReady = controlTimer.begin(controlTick, Config::CONTROL_PERIOD_US);
  Serial.print("READY,SD,"); Serial.print(cardReady ? "OK" : "ERROR");
  Serial.print(",TIMER,"); Serial.println(timerReady ? "OK" : "ERROR");
  Serial.println("Commands: S=start PD hold, X=stop, STATUS, D=fetch latest CSV, LOG,1,FETCH,LATEST");
}

void loop() {
  static char line[32];
  static size_t used = 0;
  while (Serial.available()) {
    const char c = char(Serial.read());
    if (SDlog::consumeCommandByte(c, "gravity_estimate_PD", "GRAVPD1", fetchLog)) continue;
    if ((c == 'x' || c == 'X') && used == 0) { stopRun(1); continue; }
    if (c == '\r' || c == '\n') {
      line[used] = 0;
      if (!strcmp(line, "S") || !strcmp(line, "s")) startRun();
      else if (!strcmp(line, "STATUS") || !strcmp(line, "status")) {
        noInterrupts(); const bool running = active; const uint32_t reason = faultCode;
        const float t1 = targetAngleRad[0], t2 = targetAngleRad[1];
        const float c1 = commandTorqueNm[0], c2 = commandTorqueNm[1]; interrupts();
        Serial.print("STATUS,RUNNING,"); Serial.print(running ? 1 : 0);
        Serial.print(",FAULT,"); Serial.print(reason);
        Serial.print(",M1_TARGET_RAD,"); Serial.print(t1, 8);
        Serial.print(",M1_TAU_CMD_NM,"); Serial.print(c1, 8);
        Serial.print(",M2_TARGET_RAD,"); Serial.print(t2, 8);
        Serial.print(",M2_TAU_CMD_NM,"); Serial.println(c2, 8);
      } else if (!strcmp(line, "D") || !strcmp(line, "d")) fetchLog(false);
      else if (used) Serial.println("ERROR,UNKNOWN_COMMAND");
      used = 0;
    } else if (used < sizeof(line) - 1) line[used++] = c;
  }
  if (stopRequested && active) stopRun(3);
  serviceLog();
}
