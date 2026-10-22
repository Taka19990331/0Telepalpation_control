#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <IntervalTimer.h>
#include <SdFat.h>
#include <math.h>
#include <string.h>

// AK80 MIT-mode velocity command test for Teensy 4.1.
// Configure the motor for MIT mode before running this sketch.
constexpr uint8_t MOTOR_ID = 2;
constexpr uint32_t CAN_BAUD = 1000000;
constexpr uint32_t CONTROL_PERIOD_US = 10000; // 100 Hz, as speed_servo_test1
constexpr bool SAVE_TO_SD = false;              // false: retain the run in RAM and stream CSV.
constexpr float TARGET_SPEED_RAD_S = 0.5f;    // Edit for the desired shaft speed.
constexpr float MIT_KD = 2.0f;              // MIT internal velocity gain [Nm/(rad/s)].
constexpr float MAX_ABS_SPEED_RAD_S = 8.0f;   // Abort threshold, not a speed guarantee.
constexpr uint32_t RAMP_UP_US = 1000000;
constexpr uint32_t HOLD_US = 2000000;
constexpr uint32_t RAMP_DOWN_US = 1000000;
constexpr uint32_t TEST_DURATION_US = RAMP_UP_US + HOLD_US + RAMP_DOWN_US;
constexpr uint32_t FEEDBACK_TIMEOUT_US = 200000;
constexpr uint32_t START_TIMEOUT_US = 500000;
constexpr uint16_t LOG_CAPACITY = TEST_DURATION_US / CONTROL_PERIOD_US + 2;

// AK80 MIT ranges follow sys_identification/chirp_simplified_test1.
constexpr float P_MIN = -12.5f, P_MAX = 12.5f;
constexpr float V_MIN = -37.5f, V_MAX = 37.5f;
constexpr float T_MIN = -32.0f, T_MAX = 32.0f;
constexpr float KP_MIN = 0.0f, KP_MAX = 500.0f;
constexpr float KD_MIN = 0.0f, KD_MAX = 5.0f;

FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> Can2;
IntervalTimer controlTimer;
SdFs sd;

struct Feedback {
  float positionRad, velocityRadS, torqueEstimateNm;
  uint32_t receivedUs;
  uint8_t errorCode;
  bool valid;
};
struct LogRow {
  uint32_t timestampUs;
  float commandRadS;
  float positionRad;
  float velocityRadS;
  float torqueEstimateNm;
  uint8_t errorCode;
  uint8_t feedbackValid;
  uint16_t reserved;
};
struct LogHeader {
  char magic[8];
  uint32_t version, motorId, canBitrate, controlPeriodUs, rampUpUs, holdUs, rampDownUs, rowBytes, rowCount;
  float targetSpeedRadS, mitKp, mitKd, maxSpeedRadS;
};
struct LogFooter { char magic[8]; uint32_t rowCount, status; };
static_assert(sizeof(LogRow) == 24, "Update CSV download when LogRow changes");
static_assert(sizeof(LogHeader) == 60, "Update CSV download when LogHeader changes");

LogRow logRows[LOG_CAPACITY];
uint16_t logCount = 0;
Feedback feedback = {};
volatile bool tickPending = false, tickOverrun = false;
volatile uint32_t tickTimeUs = 0;
bool timerReady = false, trialRunning = false, reportPending = false;
bool waitingForStartFeedback = false;
uint32_t trialStartUs = 0, startWaitUs = 0, txFailures = 0, rxFrames = 0, rxAccepted = 0;
uint32_t lastRxId = 0;
const char *trialStatus = "NOT_STARTED";
uint32_t trialStatusCode = 0;
char savedFilename[16] = {};

float uintToFloat(uint32_t value, float low, float high, uint8_t bits) {
  return static_cast<float>(value) * (high - low) / ((1UL << bits) - 1) + low;
}
uint32_t floatToUint(float value, float low, float high, uint8_t bits) {
  value = fmaxf(low, fminf(high, value));
  return static_cast<uint32_t>((value - low) * ((1UL << bits) - 1) / (high - low));
}

void controlTickISR() {
  if (tickPending) tickOverrun = true;
  else { tickTimeUs = micros(); tickPending = true; }
}

bool sendSpecial(uint8_t command) {
  CAN_message_t m = {};
  m.id = MOTOR_ID; m.len = 8; m.flags.extended = 0;
  for (uint8_t i = 0; i < 7; ++i) m.buf[i] = 0xff;
  m.buf[7] = command;
  if (!Can2.write(m)) { ++txFailures; return false; }
  return true;
}

bool sendMitCommand(float position, float velocity, float kp, float kd, float torque) {
  const uint32_t p = floatToUint(position, P_MIN, P_MAX, 16);
  const uint32_t v = floatToUint(velocity, V_MIN, V_MAX, 12);
  const uint32_t kpRaw = floatToUint(kp, KP_MIN, KP_MAX, 12);
  const uint32_t kdRaw = floatToUint(kd, KD_MIN, KD_MAX, 12);
  const uint32_t t = floatToUint(torque, T_MIN, T_MAX, 12);
  CAN_message_t m = {};
  m.id = MOTOR_ID; m.len = 8; m.flags.extended = 0;
  m.buf[0] = p >> 8; m.buf[1] = p;
  m.buf[2] = v >> 4; m.buf[3] = ((v & 0x0f) << 4) | (kpRaw >> 8);
  m.buf[4] = kpRaw; m.buf[5] = kdRaw >> 4;
  m.buf[6] = ((kdRaw & 0x0f) << 4) | (t >> 8); m.buf[7] = t;
  if (!Can2.write(m)) { ++txFailures; return false; }
  return true;
}

void readCanFeedback() {
  CAN_message_t m;
  while (Can2.read(m)) {
    ++rxFrames; lastRxId = m.id;
    if (m.flags.extended || m.flags.remote || m.len != 8 || m.buf[0] != MOTOR_ID) continue;
    const uint32_t p = (uint32_t(m.buf[1]) << 8) | m.buf[2];
    const uint32_t v = (uint32_t(m.buf[3]) << 4) | (m.buf[4] >> 4);
    const uint32_t t = ((uint32_t(m.buf[4]) & 0x0f) << 8) | m.buf[5];
    feedback.positionRad = uintToFloat(p, P_MIN, P_MAX, 16);
    feedback.velocityRadS = uintToFloat(v, V_MIN, V_MAX, 12);
    feedback.torqueEstimateNm = uintToFloat(t, T_MIN, T_MAX, 12);
    feedback.errorCode = m.buf[7];
    feedback.receivedUs = micros();
    feedback.valid = true;
    ++rxAccepted;
  }
}

bool feedbackFresh() {
  return feedback.valid && uint32_t(micros() - feedback.receivedUs) <= FEEDBACK_TIMEOUT_US;
}
int32_t commandAt(uint32_t elapsedUs) {
  // Kept integer for a predictable, linear ramp command profile.
  if (elapsedUs < RAMP_UP_US)
    return static_cast<int32_t>((TARGET_SPEED_RAD_S * 1000000.0f * elapsedUs) / RAMP_UP_US);
  if (elapsedUs < RAMP_UP_US + HOLD_US)
    return static_cast<int32_t>(TARGET_SPEED_RAD_S * 1000000.0f);
  if (elapsedUs < TEST_DURATION_US) {
    const uint32_t remaining = TEST_DURATION_US - elapsedUs;
    return static_cast<int32_t>((TARGET_SPEED_RAD_S * 1000000.0f * remaining) / RAMP_DOWN_US);
  }
  return 0;
}

void saveRun() {
  if (!sd.card()) { Serial.println("ERROR,SD_NOT_READY"); return; }
  bool found = false;
  for (uint16_t n = 0; n < 10000; ++n) {
    snprintf(savedFilename, sizeof(savedFilename), "SM%04u.BIN", n);
    if (!sd.exists(savedFilename)) { found = true; break; }
  }
  if (!found) { Serial.println("ERROR,NO_FILENAME_AVAILABLE"); return; }
  FsFile file;
  if (!file.open(savedFilename, O_WRONLY | O_CREAT | O_EXCL)) { Serial.println("ERROR,FILE_CREATE"); return; }
  LogHeader h = {{'S','P','D','M','I','T','0','1'}, 1, MOTOR_ID, CAN_BAUD,
    CONTROL_PERIOD_US, RAMP_UP_US, HOLD_US, RAMP_DOWN_US, sizeof(LogRow), logCount,
    TARGET_SPEED_RAD_S, 0.0f, MIT_KD, MAX_ABS_SPEED_RAD_S};
  LogFooter f = {{'E','N','D','S','P','0','0','1'}, logCount, trialStatusCode};
  bool ok = file.write(&h, sizeof(h)) == sizeof(h);
  if (ok && logCount) ok = file.write(logRows, size_t(logCount) * sizeof(LogRow)) == size_t(logCount) * sizeof(LogRow);
  if (ok) ok = file.write(&f, sizeof(f)) == sizeof(f) && file.sync();
  file.close();
  if (ok) { Serial.print("SAVED,"); Serial.print(savedFilename); Serial.print(",Rows,"); Serial.println(logCount); }
  else Serial.println("ERROR,SD_WRITE");
}

void printCsvReport() {
  Serial.println("SPEED_BEGIN,RAM");
  Serial.print("CONFIG,target_rad_s,"); Serial.print(TARGET_SPEED_RAD_S, 6);
  Serial.print(",kp,0.000000,kd,"); Serial.print(MIT_KD, 6);
  Serial.print(",period_us,"); Serial.print(CONTROL_PERIOD_US);
  Serial.print(",rows,"); Serial.print(logCount);
  Serial.print(",result,"); Serial.println(trialStatus);
  Serial.println("timestamp_us,command_rad_s,position_rad,velocity_rad_s,torque_estimate_nm,error_code,feedback_valid");
  for (uint16_t i = 0; i < logCount; ++i) {
    const LogRow &r = logRows[i];
    Serial.print(r.timestampUs); Serial.print(','); Serial.print(r.commandRadS, 6); Serial.print(',');
    if (r.feedbackValid) Serial.print(r.positionRad, 6); else Serial.print("nan"); Serial.print(',');
    if (r.feedbackValid) Serial.print(r.velocityRadS, 6); else Serial.print("nan"); Serial.print(',');
    if (r.feedbackValid) Serial.print(r.torqueEstimateNm, 6); else Serial.print("nan"); Serial.print(',');
    if (r.feedbackValid) Serial.print(r.errorCode); else Serial.print("nan"); Serial.print(',');
    Serial.println(r.feedbackValid);
  }
  Serial.println("SPEED_END");
}

void finishTrial(const char *reason, bool sendZero) {
  if (!trialRunning) return;
  trialRunning = false;
  waitingForStartFeedback = false;
  trialStatus = reason;
  trialStatusCode = strcmp(reason, "COMPLETED") == 0 ? 0 :
      strcmp(reason, "USER_STOP") == 0 ? 1 : strcmp(reason, "FEEDBACK_TIMEOUT") == 0 ? 2 :
      strcmp(reason, "MOTOR_ERROR") == 0 ? 3 : strcmp(reason, "SPEED_LIMIT") == 0 ? 4 : 5;
  if (sendZero && !sendMitCommand(0, 0, 0, 0, 0)) trialStatus = "STOP_TX_FAILED";
  if (!sendSpecial(0xfd)) trialStatus = "EXIT_TX_FAILED";
  reportPending = true;
}

void startTrial() {
  if (trialRunning || reportPending) return;
  if (!timerReady) { Serial.println("ERROR,TIMER_INIT"); return; }
  if (SAVE_TO_SD && !sd.card()) { Serial.println("ERROR,SD_INIT"); return; }
  noInterrupts(); tickPending = tickOverrun = false; interrupts();
  logCount = txFailures = 0;
  rxFrames = rxAccepted = 0;
  startWaitUs = micros();
  trialStartUs = 0;
  feedback.valid = false;
  trialStatus = "COMPLETED";
  trialRunning = true;
  if (!sendSpecial(0xfc) || !sendMitCommand(0, 0, 0, 0, 0)) {
    finishTrial("START_TX_FAILED", false); return;
  }
  waitingForStartFeedback = true;
  Serial.println("WAITING_FOR_MIT_FEEDBACK");
}

void saveRow(uint32_t timestamp, int32_t commandMicroRadS) {
  if (logCount >= LOG_CAPACITY) return;
  LogRow &r = logRows[logCount++];
  r.timestampUs = timestamp;
  r.commandRadS = commandMicroRadS / 1000000.0f;
  r.feedbackValid = feedbackFresh() ? 1 : 0;
  r.positionRad = r.feedbackValid ? feedback.positionRad : NAN;
  r.velocityRadS = r.feedbackValid ? feedback.velocityRadS : NAN;
  r.torqueEstimateNm = r.feedbackValid ? feedback.torqueEstimateNm : NAN;
  r.errorCode = r.feedbackValid ? feedback.errorCode : 0;
  r.reserved = 0;
}

void serviceControlTick() {
  noInterrupts();
  const bool pending = tickPending, overrun = tickOverrun;
  const uint32_t tick = tickTimeUs;
  tickPending = false; tickOverrun = false;
  interrupts();
  if (!pending) return;
  if (!trialRunning) return;
  if (overrun) { finishTrial("CONTROL_TICK_OVERRUN", true); return; }
  if (waitingForStartFeedback) {
    if (feedbackFresh()) {
      if (feedback.errorCode) { finishTrial("MOTOR_ERROR", true); return; }
      waitingForStartFeedback = false;
      trialStartUs = tick;
      saveRow(0, 0);
      Serial.println("RUN,MIT_SPEED");
    } else if (uint32_t(tick - startWaitUs) >= START_TIMEOUT_US) {
      finishTrial("START_FEEDBACK_TIMEOUT", true);
      return;
    }
    if (!sendMitCommand(0, 0, 0, 0, 0)) finishTrial("CAN_TX_FAILED", true);
    return;
  }
  if (!feedbackFresh()) { finishTrial("FEEDBACK_TIMEOUT", true); return; }
  if (feedback.errorCode) { finishTrial("MOTOR_ERROR", true); return; }
  if (!isfinite(feedback.velocityRadS) || fabsf(feedback.velocityRadS) > MAX_ABS_SPEED_RAD_S) {
    finishTrial("SPEED_LIMIT", true); return;
  }
  const uint32_t elapsed = uint32_t(tick - trialStartUs);
  const int32_t commandMicroRadS = commandAt(elapsed);
  if (!sendMitCommand(0, commandMicroRadS / 1000000.0f, 0, MIT_KD, 0)) {
    finishTrial("CAN_TX_FAILED", true); return;
  }
  saveRow(uint32_t(micros() - trialStartUs), commandMicroRadS);
  if (elapsed >= TEST_DURATION_US) finishTrial("COMPLETED", true);
}

void serviceSerial() {
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == 'g' || c == 'G' || c == 's' || c == 'S') startTrial();
    else if ((c == 'x' || c == 'X') && trialRunning) finishTrial("USER_STOP", true);
    else if (c == 'd' || c == 'D') {
      if (trialRunning) { Serial.println("ERROR,BUSY"); continue; }
      if (!SAVE_TO_SD) {
        if (!logCount) { Serial.println("ERROR,NO_RAM_LOG"); continue; }
        printCsvReport();
        continue;
      }
      if (!savedFilename[0]) { Serial.println("ERROR,NO_SAVED_FILE"); continue; }
      FsFile file;
      if (!file.open(savedFilename, O_RDONLY)) { Serial.println("ERROR,FILE_OPEN"); continue; }
      LogHeader h; LogFooter f;
      bool valid = file.fileSize() >= sizeof(h) + sizeof(f) && file.read(&h, sizeof(h)) == sizeof(h) && memcmp(h.magic, "SPDMIT01", 8) == 0 && h.rowBytes == sizeof(LogRow);
      if (valid) valid = file.seekSet(file.fileSize() - sizeof(f)) && file.read(&f, sizeof(f)) == sizeof(f) && memcmp(f.magic, "ENDSP001", 8) == 0 && f.rowCount == h.rowCount;
      if (!valid || !file.seekSet(sizeof(h))) { file.close(); Serial.println("ERROR,FILE_INVALID"); continue; }
      Serial.print("SPEED_BEGIN,"); Serial.println(savedFilename);
      Serial.print("CONFIG,target_rad_s,"); Serial.print(h.targetSpeedRadS, 6);
      Serial.print(",kp,"); Serial.print(h.mitKp, 6); Serial.print(",kd,"); Serial.print(h.mitKd, 6);
      Serial.print(",period_us,"); Serial.print(h.controlPeriodUs); Serial.print(",rows,"); Serial.println(h.rowCount);
      Serial.println("timestamp_us,command_rad_s,position_rad,velocity_rad_s,torque_estimate_nm,error_code,feedback_valid");
      for (uint32_t i=0; i<h.rowCount; ++i) {
        LogRow r;
        if (file.read(&r, sizeof(r)) != sizeof(r)) { Serial.println("ERROR,SD_READ"); break; }
        Serial.print(r.timestampUs); Serial.print(','); Serial.print(r.commandRadS, 6); Serial.print(',');
        if (r.feedbackValid) Serial.print(r.positionRad, 6); else Serial.print("nan"); Serial.print(',');
        if (r.feedbackValid) Serial.print(r.velocityRadS, 6); else Serial.print("nan"); Serial.print(',');
        if (r.feedbackValid) Serial.print(r.torqueEstimateNm, 6); else Serial.print("nan"); Serial.print(',');
        if (r.feedbackValid) Serial.print(r.errorCode); else Serial.print("nan"); Serial.print(','); Serial.println(r.feedbackValid);
      }
      Serial.println("SPEED_END"); file.close();
    }
  }
}

void serviceReport() {
  if (!reportPending) return;
  reportPending = false;
  Serial.print("RESULT,"); Serial.println(trialStatus);
  Serial.print("TX_FAILURES,"); Serial.print(txFailures);
  Serial.print(",RX_ACCEPTED,"); Serial.print(rxAccepted);
  Serial.print(",LAST_RX_ID,0x"); Serial.println(lastRxId, HEX);
  if (SAVE_TO_SD) {
    saveRun();
    Serial.println("READY: g=start, x=stop, d=download CSV");
  } else {
    printCsvReport();
    Serial.println("READY: g=start, x=stop, d=repeat RAM CSV");
  }
}

void setup() {
  Serial.begin(115200);
  Can2.begin(); Can2.setBaudRate(CAN_BAUD);
  timerReady = controlTimer.begin(controlTickISR, CONTROL_PERIOD_US);
  const bool cardOk = !SAVE_TO_SD || sd.begin(SdioConfig(FIFO_SDIO));
  if (SAVE_TO_SD && cardOk) {
    for (int16_t n = 9999; n >= 0; --n) {
      snprintf(savedFilename, sizeof(savedFilename), "SM%04d.BIN", n);
      if (sd.exists(savedFilename)) break;
      savedFilename[0] = '\0';
    }
  }
  Serial.println("AK80 MIT Mode velocity test ready (CAN2, ID 2, 1 Mbps).");
  Serial.print("SD,");
  Serial.println(SAVE_TO_SD ? (cardOk ? "OK" : "ERROR") : "DISABLED_RAM_LOGGING");
  Serial.println("Set MIT mode before use; fresh feedback required. g=start, x=stop, d=download CSV.");
  Serial.println("Profile: 1 s ramp up, 2 s hold, 1 s ramp down; command is v_des [rad/s].");
  Serial.println("CSV torque feedback is the driver's MIT estimate, not an external torque measurement.");
}

void loop() {
  readCanFeedback();
  serviceSerial();
  serviceControlTick();
  serviceReport();
}

