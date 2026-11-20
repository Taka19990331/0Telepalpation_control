#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <IntervalTimer.h>
#include <math.h>
#include <string.h>

// Debug version of gravity_estimate_PD: same two-motor PD hold and CAN
// polling, with no SD access. Prints 200-sample moving-average angles at ~10 Hz.
namespace Config {
constexpr uint8_t MOTOR_ID[2] = {1, 2};
constexpr uint32_t CAN_BAUD = 1000000;
constexpr uint32_t CONTROL_PERIOD_US = 500; // 2 kHz PD + torque command
constexpr uint32_t RX_POLL_PERIOD_US = 100;  // 10 kHz polling
constexpr uint8_t RX_BUDGET_PER_TICK = 4;
constexpr uint8_t TIMER_ISR_PRIORITY = 96;
constexpr uint8_t CAN_IRQ_PRIORITY = 128;
constexpr uint32_t FEEDBACK_TIMEOUT_US = 100000;
constexpr uint32_t STARTUP_TIMEOUT_US = 1000000;
constexpr uint32_t ANGLE_AVERAGE_SAMPLES = 200; // 100 ms at 2 kHz
constexpr uint32_t PRINT_PERIOD_MS = 100;       // about 10 Hz
constexpr uint32_t SERIAL_BAUD = 115200;

constexpr float POSITION_MIN = -12.5f, POSITION_MAX = 12.5f;
constexpr float VELOCITY_MIN = -37.5f, VELOCITY_MAX = 37.5f;
constexpr float TORQUE_MIN = -32.0f, TORQUE_MAX = 32.0f;
constexpr float MIT_KP_MIN = 0.0f, MIT_KP_MAX = 500.0f;
constexpr float MIT_KD_MIN = 0.0f, MIT_KD_MAX = 5.0f;
constexpr float KP_NM_PER_DEG = 0.0f;
constexpr float KD_NM_PER_RAD_S = 0.1f;
constexpr float TORQUE_LIMIT_NM = 3.0f;
}
static_assert(Config::CONTROL_PERIOD_US % Config::RX_POLL_PERIOD_US == 0,
              "CONTROL_PERIOD_US must be a multiple of RX_POLL_PERIOD_US");

using Can1Type = FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16>;
using Can2Type = FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16>;
static Can1Type can1;
static Can2Type can2;
static IntervalTimer controlTimer;

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
static volatile bool startupZeroTx = false;
static volatile bool startupTxError = false;
static volatile bool stopCanPending = false;
static uint8_t txDivider = 0;

// Updated only by the CAN polling ISR; read atomically in loop().
static float angleWindow[2][Config::ANGLE_AVERAGE_SAMPLES] = {};
static volatile float angleSum[2] = {0.0f, 0.0f};
static volatile uint16_t angleWindowIndex[2] = {0, 0};
static volatile uint16_t angleWindowCount[2] = {0, 0};

static volatile bool active = false;
static volatile uint32_t faultCode = 0;
static volatile float targetAngleRad[2] = {0.0f, 0.0f};
static volatile float commandTorqueNm[2] = {0.0f, 0.0f};
static volatile bool stopRequested = false;
static bool timerReady = false;

static uint32_t encode(float value, float low, float high, uint8_t bits) {
  value = constrain(value, low, high);
  return uint32_t((value - low) * float((1UL << bits) - 1) / (high - low));
}

static float decode(uint32_t value, float low, float high, uint8_t bits) {
  return float(value) * (high - low) / float((1UL << bits) - 1) + low;
}

static bool sendFrame(uint8_t index, const CAN_message_t &message) {
  const int result = index == 0 ? can1.write(message) : can2.write(message);
  return result == 1 || result == -1;
}

static bool sendSpecial(uint8_t index, uint8_t code) {
  CAN_message_t message = {};
  message.id = Config::MOTOR_ID[index];
  message.len = 8;
  message.flags.extended = 0;
  message.flags.remote = 0;
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
  message.flags.extended = 0;
  message.flags.remote = 0;
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

static void addAngleToMovingAverage(uint8_t index, float angleRad) {
  const uint16_t pos = angleWindowIndex[index];
  if (angleWindowCount[index] == Config::ANGLE_AVERAGE_SAMPLES)
    angleSum[index] -= angleWindow[index][pos];
  else
    ++angleWindowCount[index];
  angleWindow[index][pos] = angleRad;
  angleSum[index] += angleRad;
  angleWindowIndex[index] = (pos + 1) % Config::ANGLE_AVERAGE_SAMPLES;
}

static void receiveForMotor(uint8_t index, const CAN_message_t &message, uint32_t now) {
  if (message.flags.extended || message.flags.remote ||
      message.len < 6 || message.len > 8 ||
      (message.id != Config::MOTOR_ID[index] &&
       message.buf[0] != Config::MOTOR_ID[index])) return;
  volatile Feedback &f = feedback[index];
  f.angleRad = decode((uint32_t(message.buf[1]) << 8) | message.buf[2],
                      Config::POSITION_MIN, Config::POSITION_MAX, 16);
  f.velocityRadS = decode((uint32_t(message.buf[3]) << 4) | (message.buf[4] >> 4),
                          Config::VELOCITY_MIN, Config::VELOCITY_MAX, 12);
  f.torqueNm = decode(((uint32_t(message.buf[4]) & 0x0f) << 8) | message.buf[5],
                      Config::TORQUE_MIN, Config::TORQUE_MAX, 12);
  f.error = message.len >= 8 ? message.buf[7] : 0;
  f.rxUs = now;
  f.valid = true;
  addAngleToMovingAverage(index, f.angleRad);
}

static void pollCanMotor(uint8_t index) {
  for (uint8_t n = 0; n < Config::RX_BUDGET_PER_TICK; ++n) {
    CAN_message_t message = {};
    const int got = index == 0 ? can1.read(message) : can2.read(message);
    if (!got) break;
    if (captureFeedback) receiveForMotor(index, message, micros());
  }
}

static void stopFromIsr(uint32_t reason) {
  active = false;
  faultCode = reason;
  commandTorqueNm[0] = commandTorqueNm[1] = 0.0f;
  captureFeedback = false;
  stopRequested = true;
  stopCanPending = true;
}

static void controlTick() {
  if (!active) return;
  const uint32_t now = micros();
  Feedback f[2];
  for (uint8_t i = 0; i < 2; ++i) {
    f[i].angleRad = feedback[i].angleRad;
    f[i].velocityRadS = feedback[i].velocityRadS;
    f[i].torqueNm = feedback[i].torqueNm;
    f[i].rxUs = feedback[i].rxUs;
    f[i].error = feedback[i].error;
    f[i].valid = feedback[i].valid;
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
  const bool tx1 = sendCommand(0, tau[0]);
  const bool tx2 = sendCommand(1, tau[1]);
  if (!tx1 || !tx2) stopFromIsr(2);
}

static void canPollingTimerTick() {
  pollCanMotor(0);
  pollCanMotor(1);
  if (++txDivider < Config::CONTROL_PERIOD_US / Config::RX_POLL_PERIOD_US) return;
  txDivider = 0;
  if (startupZeroTx) {
    const bool tx1 = sendCommand(0, 0.0f);
    const bool tx2 = sendCommand(1, 0.0f);
    if (!tx1 || !tx2) {
      startupZeroTx = false;
      startupTxError = true;
      stopCanPending = true;
    }
  } else if (active) {
    controlTick();
  }
}

static bool waitForInitialFeedback(uint32_t started) {
  while (uint32_t(micros() - started) < Config::STARTUP_TIMEOUT_US) {
    if (startupTxError) return false;
    noInterrupts();
    Feedback f1, f2;
    f1.angleRad = feedback[0].angleRad; f1.error = feedback[0].error; f1.valid = feedback[0].valid;
    f2.angleRad = feedback[1].angleRad; f2.error = feedback[1].error; f2.valid = feedback[1].valid;
    interrupts();
    if (f1.valid && f2.valid) {
      if (f1.error || f2.error || !isfinite(f1.angleRad) || !isfinite(f2.angleRad)) return false;
      noInterrupts();
      targetAngleRad[0] = f1.angleRad;
      targetAngleRad[1] = f2.angleRad;
      interrupts();
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
  if (active) { Serial.println("ERROR,BUSY"); return; }
  if (!timerReady) { Serial.println("ERROR,TIMER_NOT_READY"); return; }
  noInterrupts();
  for (uint8_t i = 0; i < 2; ++i) {
    feedback[i].angleRad = 0.0f; feedback[i].velocityRadS = 0.0f;
    feedback[i].torqueNm = 0.0f; feedback[i].rxUs = 0;
    feedback[i].error = 0; feedback[i].valid = false;
    angleSum[i] = 0.0f; angleWindowIndex[i] = 0; angleWindowCount[i] = 0;
  }
  captureFeedback = false;
  startupZeroTx = false;
  startupTxError = false;
  stopCanPending = false;
  stopRequested = false;
  faultCode = 0;
  interrupts();
  const bool enabled1 = sendSpecial(0, 0xfc);
  const bool enabled2 = sendSpecial(1, 0xfc);
  const bool enabled = enabled1 && enabled2;
  if (enabled) {
    noInterrupts(); captureFeedback = true; startupZeroTx = true; interrupts();
  }
  const bool gotFeedback = enabled && waitForInitialFeedback(micros());
  noInterrupts();
  const bool ready = gotFeedback && startupZeroTx && !startupTxError;
  startupZeroTx = false;
  if (ready) active = true;
  else {
    captureFeedback = false;
    stopCanPending = false;
    if (!faultCode) faultCode = startupTxError ? 2 : 6;
  }
  interrupts();
  if (!ready) {
    sendCommand(0, 0.0f); sendCommand(1, 0.0f);
    sendSpecial(0, 0xfd); sendSpecial(1, 0xfd);
    Serial.println("ERROR,STARTUP_FEEDBACK");
    return;
  }
  Serial.print("RUNNING,TARGET_M1_DEG,"); Serial.print(float(targetAngleRad[0]) * 57.2957795131f, 3);
  Serial.print(",TARGET_M2_DEG,"); Serial.println(float(targetAngleRad[1]) * 57.2957795131f, 3);
}

static void stopRun(uint32_t reason) {
  noInterrupts();
  const bool wasActive = active || startupZeroTx;
  const bool pendingStop = stopCanPending;
  active = false; startupZeroTx = false; stopCanPending = false;
  if (wasActive || (!pendingStop && faultCode == 0)) faultCode = reason;
  commandTorqueNm[0] = commandTorqueNm[1] = 0.0f;
  captureFeedback = false;
  interrupts();
  if (wasActive || pendingStop) {
    sendCommand(0, 0.0f); sendCommand(1, 0.0f);
    sendSpecial(0, 0xfd); sendSpecial(1, 0xfd);
  }
}

static void servicePendingCanStop() {
  if (!stopCanPending) return;
  noInterrupts(); const bool pending = stopCanPending; stopCanPending = false; interrupts();
  if (!pending) return;
  sendCommand(0, 0.0f); sendCommand(1, 0.0f);
  sendSpecial(0, 0xfd); sendSpecial(1, 0xfd);
}

static void printAverageAngles() {
  static uint32_t lastPrintMs = 0;
  const uint32_t now = millis();
  if (uint32_t(now - lastPrintMs) < Config::PRINT_PERIOD_MS) return;
  lastPrintMs = now;
  noInterrupts();
  const uint16_t n1 = angleWindowCount[0], n2 = angleWindowCount[1];
  const float a1 = n1 ? angleSum[0] / n1 : 0.0f;
  const float a2 = n2 ? angleSum[1] / n2 : 0.0f;
  const float torque1 = commandTorqueNm[0];
  const float torque2 = commandTorqueNm[1];
  interrupts();
  if (n1 < Config::ANGLE_AVERAGE_SAMPLES || n2 < Config::ANGLE_AVERAGE_SAMPLES) return;
  Serial.print("ANGLE_AVG,");
  Serial.print(a1 * 57.2957795131f, 3); Serial.print(","); Serial.print(a1, 5);
  Serial.print(","); Serial.print(torque1, 5); Serial.print(",");
  Serial.print(a2 * 57.2957795131f, 3); Serial.print(","); Serial.print(a2, 5);
  Serial.print(","); Serial.println(torque2, 5);
}

void setup() {
  Serial.begin(Config::SERIAL_BAUD);
  can1.begin(); can1.setBaudRate(Config::CAN_BAUD);
  can2.begin(); can2.setBaudRate(Config::CAN_BAUD);
  NVIC_SET_PRIORITY(IRQ_CAN1, Config::CAN_IRQ_PRIORITY);
  NVIC_SET_PRIORITY(IRQ_CAN2, Config::CAN_IRQ_PRIORITY);
  controlTimer.priority(Config::TIMER_ISR_PRIORITY);
  timerReady = controlTimer.begin(canPollingTimerTick, Config::RX_POLL_PERIOD_US);
  Serial.print("READY,TIMER,"); Serial.println(timerReady ? "OK" : "ERROR");
  Serial.println("ANGLE_AVG,m1_deg,m1_rad,m1_cmd_torque_Nm,m2_deg,m2_rad,m2_cmd_torque_Nm");
  Serial.println("Commands: S=start, X=stop, STATUS");
}

void loop() {
  servicePendingCanStop();
  static char line[32];
  static size_t used = 0;
  while (Serial.available()) {
    const char c = char(Serial.read());
    if ((c == 'x' || c == 'X') && used == 0) { stopRun(1); continue; }
    if (c == '\r' || c == '\n') {
      line[used] = 0;
      if (!strcmp(line, "S") || !strcmp(line, "s")) startRun();
      else if (!strcmp(line, "STATUS") || !strcmp(line, "status")) {
        noInterrupts();
        const bool running = active; const uint32_t reason = faultCode;
        const float t1 = targetAngleRad[0], t2 = targetAngleRad[1];
        interrupts();
        Serial.print("STATUS,RUNNING,"); Serial.print(running ? 1 : 0);
        Serial.print(",FAULT,"); Serial.print(reason);
        Serial.print(",M1_TARGET_DEG,"); Serial.print(t1 * 57.2957795131f, 3);
        Serial.print(",M2_TARGET_DEG,"); Serial.println(t2 * 57.2957795131f, 3);
      } else if (used) Serial.println("ERROR,UNKNOWN_COMMAND");
      used = 0;
    } else if (used < sizeof(line) - 1) line[used++] = c;
  }
  servicePendingCanStop();
  printAverageAngles();
}
