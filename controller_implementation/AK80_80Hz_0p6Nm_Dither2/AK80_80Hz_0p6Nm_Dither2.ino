/*
 * AK80_80Hz_0p6Nm_Dither.ino (configurable frequency/amplitude)
 * Teensy 4.1 + FlexCAN_T4 + IntervalTimer; Jia Mode1 MIT frames.
 * Built from the working v4 Jia polling / dual-CAN structure.
 *
 * No continuous CAN diagnostics or telemetry over USB.
 * Only READY, mode changes, and concise FAULT/STOP messages are printed.
 *
 * Commands (newline terminated):
 *   S : start 2-kHz ZERO-torque communication (MIT enabled)
 *   D : start sine dither at user-configured frequency and amplitude
 *   X : stop periodic commands, request zero + MIT EXIT (best effort)
 *
 * Zero torque does NOT brake/hold the mechanism. Use an independently
 * operable physical emergency stop and support the arm during testing.
 */

#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <IntervalTimer.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

// ==================== USER SETTINGS ====================
constexpr bool MOTOR1_ON = true;  // CAN1 / MotorID 1
constexpr bool MOTOR2_ON = true;  // CAN2 / MotorID 2
constexpr float DITHER_FREQUENCY_HZ = 50.0f; // arbitrary Hz, e.g. 40.0f, 77.5f, 80.0f
constexpr float DITHER_AMPLITUDE_NM = 0.4f; // peak torque [Nm], NOT peak-to-peak
constexpr float MOTOR1_DITHER_SIGN = 1.0f;
constexpr float MOTOR2_DITHER_SIGN = 1.0f; // both motors IN PHASE by default
constexpr bool TORQUE_FEEDBACK_LIMIT_ENABLED = false;
constexpr float MAX_REPORTED_TORQUE_NM = 2.5f;

// ==================== FIXED COMMUNICATION SETTINGS ====================
constexpr uint8_t MOTOR_ID[2] = {1, 2};
constexpr uint32_t CAN_BAUD = 1000000;
constexpr uint32_t TX_PERIOD_US = 500;     // 2 kHz, both CAN buses
constexpr uint32_t RX_POLL_US = 100;       // polling at 10 kHz per bus
constexpr uint8_t RX_BUDGET_PER_POLL = 4;
constexpr uint8_t TIMER_PRIORITY = 96;
constexpr uint8_t CAN_IRQ_PRIORITY = 128;
constexpr uint32_t SERIAL_BAUD = 115200;

// Safety boundaries (adjust only after physically reviewing the mechanism).
constexpr uint32_t RX_MAX_AGE_US = 3000;       // while dither is active
constexpr uint32_t INITIAL_RX_TIMEOUT_US = 500000; // zero-mode startup timeout
constexpr uint32_t RX_MIN_COUNT_FOR_DITHER = 200;  // >=100ms feedback per motor
constexpr uint32_t TX_QUEUE_FAULT_COUNT = 2;

constexpr float P_MIN = -12.5f, P_MAX = 12.5f;
constexpr float V_MIN = -37.5f, V_MAX = 37.5f;
constexpr float T_MIN = -32.0f, T_MAX = 32.0f;
constexpr float KP_MAX = 500.0f, KD_MAX = 5.0f;
constexpr uint32_t COMMAND_HZ = 1000000UL / TX_PERIOD_US;
static_assert((1000000UL % TX_PERIOD_US) == 0, "TX_PERIOD_US must divide 1 s");
static_assert(MOTOR1_ON || MOTOR2_ON, "At least one motor must be enabled");
static_assert((TX_PERIOD_US % RX_POLL_US) == 0, "TX period must divide into RX poll periods");
// DDS (direct digital synthesis): phase += round(f_d / f_s * 2^32).
// Any positive real frequency below Nyquist is valid (not just integer divisors
// of 2000). Frequencies with fewer than ~10 TX samples per cycle have coarse
// waveform resolution even though the DDS frequency remains correct.
static_assert(DITHER_FREQUENCY_HZ >= (float(COMMAND_HZ) / 4294967296.0f) &&
              DITHER_FREQUENCY_HZ < (float(COMMAND_HZ) * 0.5f),
              "Dither frequency must be between 0 and Nyquist (exclusive)");
// MIT 12-bit torque codes are separated by 64/4095 Nm. The smallest representable
// |torque| (including 'zero') is approximately 0.007814 Nm, not exactly zero.
static_assert(DITHER_AMPLITUDE_NM >= ((T_MAX - T_MIN) / (2.0f * 4095.0f)) &&
              DITHER_AMPLITUDE_NM <= T_MAX,
              "Dither amplitude must be representable and within the MIT torque range");
constexpr uint32_t SINE_LUT_BITS = 10;       // 1024 precomputed points, fixed RAM
constexpr uint32_t SINE_LUT_SIZE = 1UL << SINE_LUT_BITS;
constexpr uint32_t PHASE_TO_LUT_SHIFT = 32 - SINE_LUT_BITS;
static_assert(MOTOR1_DITHER_SIGN == 1.0f || MOTOR1_DITHER_SIGN == -1.0f,
              "M1 dither sign must be +/-1");
static_assert(MOTOR2_DITHER_SIGN == 1.0f || MOTOR2_DITHER_SIGN == -1.0f,
              "M2 dither sign must be +/-1");

using Can1Type = FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16>;
using Can2Type = FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16>;
static Can1Type can1;
static Can2Type can2;
static IntervalTimer canTimer;

static inline bool motorOn(uint8_t i) {
  return (i == 0) ? MOTOR1_ON : (i == 1 ? MOTOR2_ON : false);
}

enum RunState : uint8_t { IDLE, ZERO_MODE, DITHER_MODE, FAULT_STATE };
enum FaultReason : uint8_t {
  NO_FAULT, TX_FAILED, TX_BACKLOG, RX_TIMEOUT, RX_OVERRUN,
  RX_BAD_MOTOR_CODE, MOTOR_TORQUE_LIMIT, CAN_BUS_OFF, ISR_OVERRUN
};

// Arduino's sketch preprocessor may place generated prototypes before enum
// declarations. Keep these typed prototypes explicit so FaultReason is known.
static void setFault(uint8_t i, FaultReason reason);
static const char *faultName(FaultReason reason);

struct MotorFeedback {
  uint32_t rxValid = 0;
  uint32_t lastRxUs = 0;
  bool haveRx = false;
  float torqueFeedback = 0.0f;
  uint8_t motorError = 0;
};

static MotorFeedback motor[2];
static CAN_message_t zeroFrame[2];
// Precomputed torque CODE per phase point, separately for each motor/sign.
// Fixed 4 KB table regardless of frequency. ISR has no sinf() or floating math.
static uint16_t ditherCodes[2][SINE_LUT_SIZE];
static volatile RunState state = IDLE;
static volatile FaultReason faultReason = NO_FAULT;
static volatile uint8_t faultMotor = 255;
static volatile bool ditherRequested = false;
static uint32_t phaseStep = 0; // Q0.32 cycles/TX sample, derived from f_d/f_s
static uint32_t phaseAccumulator = 0;
static bool timerActive = false;
static uint32_t sessionStartUs = 0;
static uint8_t txDivider = 0;


static uint16_t encodeFloat(float x, float lo, float hi, uint8_t bits) {
  x = constrain(x, lo, hi);
  return static_cast<uint16_t>((x - lo) * float((1UL << bits) - 1) / (hi - lo));
}
static float decodeFloat(uint16_t raw, float lo, float hi, uint8_t bits) {
  return float(raw) * (hi - lo) / float((1UL << bits) - 1) + lo;
}

// Jia Mode1 packing remains unchanged. The separate dither-code generator
// rounds to the nearest representable MIT torque, then clamps inside the
// user-requested peak amplitude. 12-bit quantization is unavoidable.
static CAN_message_t makeZeroFrame(uint8_t i) {
  CAN_message_t m{};
  m.id = MOTOR_ID[i]; m.len = 8;
  m.flags.extended = 0; m.flags.remote = 0;
  const uint16_t p = encodeFloat(0.0f, P_MIN, P_MAX, 16);
  const uint16_t v = encodeFloat(0.0f, V_MIN, V_MAX, 12);
  const uint16_t kp = encodeFloat(0.0f, 0.0f, KP_MAX, 12);
  const uint16_t kd = encodeFloat(0.0f, 0.0f, KD_MAX, 12);
  const uint16_t t = encodeFloat(0.0f, T_MIN, T_MAX, 12);
  m.buf[0] = p >> 8;
  m.buf[1] = p & 0xff;
  m.buf[2] = v >> 4;
  m.buf[3] = ((v & 0x0f) << 4) | (kp >> 8);
  m.buf[4] = kp & 0xff;
  m.buf[5] = kd >> 4;
  m.buf[6] = ((kd & 0x0f) << 4) | (t >> 8);
  m.buf[7] = t & 0xff;
  return m;
}

static uint16_t quantizeDitherTorque(float torqueNm, uint16_t lowCode,
                                     uint16_t highCode) {
  const float scale = 4095.0f / (T_MAX - T_MIN);
  const float raw = (torqueNm - T_MIN) * scale;
  int32_t code = int32_t(floorf(raw + 0.5f));
  if (code < int32_t(lowCode)) code = lowCode;
  if (code > int32_t(highCode)) code = highCode;
  return uint16_t(code);
}

static void initializeWaveforms() {
  // Double-precision during startup only: DDS frequency is correct to
  // roughly f_sample / 2^32 (~0.00000047 Hz at 2 kHz).
  phaseStep = uint32_t(
      double(DITHER_FREQUENCY_HZ) * (4294967296.0 / double(COMMAND_HZ)) + 0.5);
  const float scale = 4095.0f / (T_MAX - T_MIN);
  const uint16_t lowCode = uint16_t(ceilf((-DITHER_AMPLITUDE_NM - T_MIN) * scale));
  const uint16_t highCode = uint16_t(floorf((DITHER_AMPLITUDE_NM - T_MIN) * scale));
  for (uint8_t i = 0; i < 2; ++i) {
    zeroFrame[i] = makeZeroFrame(i);
    const float sign = (i == 0) ? MOTOR1_DITHER_SIGN : MOTOR2_DITHER_SIGN;
    for (uint32_t k = 0; k < SINE_LUT_SIZE; ++k) {
      const float angle = 6.283185307179586f * float(k) / float(SINE_LUT_SIZE);
      const float tau = sign * DITHER_AMPLITUDE_NM * sinf(angle);
      ditherCodes[i][k] = quantizeDitherTorque(tau, lowCode, highCode);
    }
  }
}

// Lightweight conversion of one 12-bit torque code to the established
// 8-byte MIT command. Only bytes 6..7 change; position, speed, kp, kd stay 0.
static CAN_message_t ditherFrameAtPhase(uint8_t i, uint32_t phaseIndex) {
  CAN_message_t m = zeroFrame[i];
  const uint16_t t = ditherCodes[i][phaseIndex];
  m.buf[6] = (m.buf[6] & 0xf0) | (t >> 8);
  m.buf[7] = t & 0xff;
  return m;
}

static int writeFrame(uint8_t i, const CAN_message_t &msg) {
  return i == 0 ? can1.write(msg) : can2.write(msg);
}
static uint32_t txQueue(uint8_t i) {
  return i == 0 ? can1.getTXQueueCount() : can2.getTXQueueCount();
}
static uint32_t esr1(uint8_t i) {
  return FLEXCANb_ESR1(i == 0 ? CAN1 : CAN2);
}
static void setFault(uint8_t i, FaultReason reason) {
  if (state == IDLE || state == FAULT_STATE) return;
  faultMotor = i;
  faultReason = reason;
  ditherRequested = false;
  state = FAULT_STATE;  // immediately suppress future nonzero command TX
}

static bool sendPeriodFrame(uint8_t i, const CAN_message_t &m) {
  const int result = writeFrame(i, m);
  if (result != 1 && result != -1) { setFault(i, TX_FAILED); return false; }
  if (txQueue(i) >= TX_QUEUE_FAULT_COUNT) { setFault(i, TX_BACKLOG); return false; }
  return true;
}

static bool sendSpecial(uint8_t i, uint8_t code) {
  CAN_message_t m{};
  m.id = MOTOR_ID[i]; m.len = 8;
  for (uint8_t j = 0; j < 7; ++j) m.buf[j] = 0xff;
  m.buf[7] = code; // 0xFC = MIT enter, 0xFD = MIT exit
  const int result = writeFrame(i, m);
  return (result == 1 || result == -1);
}

// Jia accepted a valid CAN identifier OR a matching payload motor identifier.
// As in the working code, no onReceive()/CAN.events() are installed.
static void parseFeedback(uint8_t i, const CAN_message_t &m, uint32_t now) {
  if (m.flags.overrun) { setFault(i, RX_OVERRUN); return; }
  if (m.flags.extended || m.flags.remote || m.len < 6 || m.len > 8) return;
  if (m.id != MOTOR_ID[i] && m.buf[0] != MOTOR_ID[i]) return;
  MotorFeedback &d = motor[i];
  const uint16_t t = (uint16_t(m.buf[4] & 0x0f) << 8) | m.buf[5];
  d.torqueFeedback = decodeFloat(t, T_MIN, T_MAX, 12);
  d.motorError = (m.len >= 8) ? m.buf[7] : 0;
  d.lastRxUs = now;
  d.haveRx = true;
  ++d.rxValid;
  if (d.motorError != 0) setFault(i, RX_BAD_MOTOR_CODE);
}

static void pollMotor(uint8_t i) {
  for (uint8_t j = 0; j < RX_BUDGET_PER_POLL; ++j) {
    CAN_message_t m{};
    const int got = (i == 0) ? can1.read(m) : can2.read(m);
    if (!got) break;
    parseFeedback(i, m, micros());
    if (state == FAULT_STATE) break;
  }
}
static void drainIdleMotor(uint8_t i) {
  for (uint8_t j = 0; j < RX_BUDGET_PER_POLL; ++j) {
    CAN_message_t m{};
    if (!(i == 0 ? can1.read(m) : can2.read(m))) break;
  }
}

// Called on TX tick after RX polling. Check only control-impacting faults.
static bool checkControlSafety(uint32_t now) {
  for (uint8_t i = 0; i < 2; ++i) if (motorOn(i)) {
    const uint32_t esr = esr1(i);
    if (FLEXCAN_ESR_get_fault_code(esr) == CAN_ERROR_BUS_OFF) {
      setFault(i, CAN_BUS_OFF); return false;
    }
    const MotorFeedback &d = motor[i];
    if (state == ZERO_MODE) {
      // During startup, allow some time for the motor to respond after ENABLE.
      if (!d.haveRx && now - sessionStartUs > INITIAL_RX_TIMEOUT_US) {
        setFault(i, RX_TIMEOUT); return false;
      }
      if (d.haveRx && now - d.lastRxUs > RX_MAX_AGE_US) {
        setFault(i, RX_TIMEOUT); return false;
      }
    } else if (state == DITHER_MODE) {
      if (!d.haveRx || now - d.lastRxUs > RX_MAX_AGE_US) {
        setFault(i, RX_TIMEOUT); return false;
      }
      if (TORQUE_FEEDBACK_LIMIT_ENABLED &&
          fabsf(d.torqueFeedback) > MAX_REPORTED_TORQUE_NM) {
        setFault(i, MOTOR_TORQUE_LIMIT); return false;
      }
    }
  }
  return true;
}

static void timerISR() {
  const uint32_t tStart = micros();
  if (state == IDLE || state == FAULT_STATE) {
    if (MOTOR1_ON) drainIdleMotor(0);
    if (MOTOR2_ON) drainIdleMotor(1);
    return;
  }
  if (MOTOR1_ON) pollMotor(0);
  if (state != FAULT_STATE && MOTOR2_ON) pollMotor(1);

  if (state == ZERO_MODE || state == DITHER_MODE) {
    if (++txDivider >= TX_PERIOD_US / RX_POLL_US) {
      txDivider = 0;
      const uint32_t now = micros();
      if (!checkControlSafety(now)) return;

      if (state == ZERO_MODE && ditherRequested) {
        phaseAccumulator = 0;
        ditherRequested = false;
        state = DITHER_MODE;
      }
      // Both motors share one exact phase. Only precomputed lookup and
      // bit packing happen inside ISR; no sine/floating-point operations.
      const uint32_t phaseIndex = phaseAccumulator >> PHASE_TO_LUT_SHIFT;
      for (uint8_t i = 0; i < 2 && (state == ZERO_MODE || state == DITHER_MODE); ++i) {
        if (!motorOn(i)) continue;
        const CAN_message_t frame = (state == DITHER_MODE)
            ? ditherFrameAtPhase(i, phaseIndex) : zeroFrame[i];
        if (!sendPeriodFrame(i, frame)) break;
      }
      if (state == DITHER_MODE) phaseAccumulator += phaseStep;
    }
  }
  if (state != FAULT_STATE && micros() - tStart > RX_POLL_US) {
    setFault(MOTOR1_ON ? 0 : 1, ISR_OVERRUN);
  }
}

static void stopTimer() {
  if (timerActive) { canTimer.end(); timerActive = false; }
}
static bool startTimer() {
  if (timerActive) return true;
  canTimer.priority(TIMER_PRIORITY);
  timerActive = canTimer.begin(timerISR, RX_POLL_US);
  return timerActive;
}

static void issueStopFrames() {
  // Best effort ONLY: a disconnected/bus-off actuator may not receive either.
  for (uint8_t i = 0; i < 2; ++i) if (motorOn(i))
    (void)writeFrame(i, zeroFrame[i]);
  for (uint8_t i = 0; i < 2; ++i) if (motorOn(i))
    (void)sendSpecial(i, 0xFD);
}
static const char *faultName(FaultReason reason) {
  switch (reason) {
    case TX_FAILED: return "TX_FAILED";
    case TX_BACKLOG: return "TX_BACKLOG";
    case RX_TIMEOUT: return "RX_TIMEOUT";
    case RX_OVERRUN: return "RX_OVERRUN";
    case RX_BAD_MOTOR_CODE: return "MOTOR_ERROR";
    case MOTOR_TORQUE_LIMIT: return "TORQUE_FB_LIMIT";
    case CAN_BUS_OFF: return "BUS_OFF";
    case ISR_OVERRUN: return "ISR_OVERRUN";
    default: return "NONE";
  }
}
static void finishSession(bool faulted) {
  if (state == IDLE) return;
  state = IDLE; // suppress dither before stopping timer
  ditherRequested = false;
  stopTimer();
  issueStopFrames();
  if (faulted) {
    Serial.print("FAULT,M"); Serial.print(faultMotor + 1);
    Serial.print(','); Serial.println(faultName(faultReason));
  } else {
    Serial.println("STOPPED,MANUAL");
  }
  if (!startTimer()) Serial.println("FAULT,TIMER_START_FAILED");
}

static void startZeroMode() {
  if (state != IDLE) { Serial.println("REJECT,ALREADY_ACTIVE"); return; }
  stopTimer();
  motor[0] = MotorFeedback{};
  motor[1] = MotorFeedback{};
  ditherRequested = false;
  faultReason = NO_FAULT;
  faultMotor = 255;
  txDivider = 0; phaseAccumulator = 0;
  sessionStartUs = micros();
  // As in the working v4: send ENABLE first, do not immediately send zero.
  bool ok = true;
  for (uint8_t i = 0; i < 2; ++i) if (motorOn(i))
    if (!sendSpecial(i, 0xFC)) ok = false;
  if (!ok) {
    issueStopFrames();
    Serial.println("FAULT,MIT_ENABLE_TX_FAILED");
    if (!startTimer()) Serial.println("FAULT,TIMER_START_FAILED");
    return;
  }
  state = ZERO_MODE;
  if (!startTimer()) {
    state = IDLE;
    issueStopFrames();
    Serial.println("FAULT,TIMER_START_FAILED");
    return;
  }
  Serial.println("ZERO_STARTED,USE_D_TO_START_DITHER");
}

static void requestDither() {
  if (state != ZERO_MODE) { Serial.println("REJECT,START_ZERO_MODE_WITH_S_FIRST"); return; }
  const uint32_t now = micros();
  MotorFeedback snapshot[2];
  noInterrupts();
  snapshot[0] = motor[0]; snapshot[1] = motor[1];
  interrupts();
  for (uint8_t i = 0; i < 2; ++i) if (motorOn(i)) {
    const MotorFeedback &d = snapshot[i];
    if (!d.haveRx || d.rxValid < RX_MIN_COUNT_FOR_DITHER ||
        now - d.lastRxUs > RX_MAX_AGE_US || d.motorError != 0 ||
        txQueue(i) != 0 ||
        FLEXCAN_ESR_get_fault_code(esr1(i)) != CAN_ERROR_ACTIVE) {
      Serial.print("REJECT,M"); Serial.print(i + 1);
      Serial.println(",FEEDBACK_NOT_READY");
      return;
    }
  }
  noInterrupts();
  if (state == ZERO_MODE) ditherRequested = true;
  interrupts();
  Serial.print("DITHER_ARMED,FREQ_HZ,"); Serial.print(DITHER_FREQUENCY_HZ, 5);
  Serial.print(",AMP_PEAK_NM,"); Serial.println(DITHER_AMPLITUDE_NM, 5);
}

static void onCommand(char *c) {
  for (char *p = c; *p; ++p) *p = char(toupper((unsigned char)*p));
  if (!strcmp(c, "S")) startZeroMode();
  else if (!strcmp(c, "D")) requestDither();
  else if (!strcmp(c, "X")) finishSession(false);
  else Serial.println("COMMANDS,S,D,X");
}
static void pollSerial() {
  static char input[20];
  static uint8_t used = 0;
  while (Serial.available()) {
    char c = char(Serial.read());
    if (c == '\r' || c == '\n') {
      if (used) { input[used] = 0; used = 0; onCommand(input); }
    } else if (used < sizeof(input) - 1) {
      input[used++] = c;
    }
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  initializeWaveforms();
  if (MOTOR1_ON) {
    can1.begin(); can1.setBaudRate(CAN_BAUD);
    NVIC_SET_PRIORITY(IRQ_CAN1, CAN_IRQ_PRIORITY);
  }
  if (MOTOR2_ON) {
    can2.begin(); can2.setBaudRate(CAN_BAUD);
    NVIC_SET_PRIORITY(IRQ_CAN2, CAN_IRQ_PRIORITY);
  }
  // Keep idle 100us RX drain: avoids the previously observed startup issue.
  if (!startTimer()) Serial.println("FAULT,TIMER_START_FAILED");
  Serial.print("READY,AK80_Jia_Dither_Configurable,FREQ_HZ,");
  Serial.print(DITHER_FREQUENCY_HZ, 5);
  Serial.print(",AMP_PEAK_NM,"); Serial.print(DITHER_AMPLITUDE_NM, 5);
  Serial.print(",SAMPLE_HZ,"); Serial.print(COMMAND_HZ);
  Serial.println();
  Serial.print("M1="); Serial.print(MOTOR1_ON ? "ON" : "OFF");
  Serial.print(",M2="); Serial.println(MOTOR2_ON ? "ON" : "OFF");
  Serial.println("COMMANDS,S=ZERO,D=DITHER,X=STOP");
}
void loop() {
  // No CAN error/status/periodic serial output. Keep internal safety checks.
  pollSerial();
  // Consume error snapshots silently to keep FlexCAN_T4's bounded queue clear.
  // No serial communication diagnostics are emitted.
  for (uint8_t i = 0; i < 2; ++i) if (motorOn(i)) {
    for (uint8_t n = 0; n < 4; ++n) {
      CAN_error_t errorSnapshot{};
      if (!(i == 0 ? can1.error(errorSnapshot, false)
                   : can2.error(errorSnapshot, false))) break;
    }
  }
  const RunState s = state;
  if (s == FAULT_STATE) finishSession(true);
}
