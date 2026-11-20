/*
 * AK80_GravityCompensation.ino
 * Teensy 4.1 + FlexCAN_T4 + IntervalTimer; Jia Mode1 / MIT command frames.
 * Converted from AK80_80Hz_0p6Nm_Dither, preserving the working dual-CAN
 * 100 us RX-poll / 500 us TX architecture, safety checks and stop behavior.
 *
 * Commands (newline-terminated):
 *   S : enter MIT mode, start 2 kHz zero-torque communication
 *   G : arm gravity compensation (only with fresh feedback from BOTH motors)
 *   X : stop 2 kHz commands, request zero torque + MIT EXIT (best effort)
 *
 * IMPORTANT: Zero torque is not a mechanical brake. Gravity compensation
 * actively drives the motors and can move the arm. Physically support the
 * mechanism, use an independent emergency stop, and test with no human contact.
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

// Gravity compensation torque scale. Use 0.5 at the initial test.
constexpr float K_G = 0.75f;

// The user-specified gravity torques are the STEADY-STATE targets.
// On the initial switch from zero to gravity mode, ramp actual commands
// from zero to those targets over this time to avoid a sudden torque step.
constexpr uint32_t GRAVITY_RAMP_MS = 1000;

// Independent protection against unexpectedly large torque commands.
// With the specified geometry and K_G=0.5, the theoretical maxima are
// approximately M1=0.391 Nm and M2=1.612 Nm, so 2.0 Nm does not clip them.
constexpr float MAX_ABS_COMMAND_TORQUE_NM = 5.0f;

// Preserve the source program's feedback-torque safety option.
constexpr bool TORQUE_FEEDBACK_LIMIT_ENABLED = false;
constexpr float MAX_REPORTED_TORQUE_NM = 2.5f;

// ==================== GRAVITY MODEL (ANGLES IN rad) ====================
// Keep calibration constants separate so each can be modified independently.
constexpr float M1E       = -0.825322046f;
constexpr float M2E       = -2.424051670f;
constexpr float q1E       = -0.304722562f;
constexpr float q2E       =  0.735973234f;
constexpr float theta_off =  0.075099897f;

// Gravity coefficients [Nm].
constexpr float G1 = 2.6215f;
constexpr float G2 = 0.0542f;
static bool gravityCoefficientsValid = false;

// ==================== FIXED COMMUNICATION SETTINGS ====================
constexpr uint8_t MOTOR_ID[2] = {1, 2};
constexpr uint32_t CAN_BAUD = 1000000;
constexpr uint32_t TX_PERIOD_US = 500;     // 2 kHz per bus
constexpr uint32_t RX_POLL_US = 100;       // 10 kHz RX polling per bus
constexpr uint8_t RX_BUDGET_PER_POLL = 4;
constexpr uint8_t TIMER_PRIORITY = 96;
constexpr uint8_t CAN_IRQ_PRIORITY = 128;
constexpr uint32_t SERIAL_BAUD = 115200;

// Safety thresholds, same communication timing as the source.
constexpr uint32_t RX_MAX_AGE_US = 3000;
constexpr uint32_t INITIAL_RX_TIMEOUT_US = 500000;
constexpr uint32_t RX_MIN_COUNT_FOR_GRAVITY = 200; // 100 ms at 2 kHz
constexpr uint32_t TX_QUEUE_FAULT_COUNT = 2;

// MIT control ranges. Position feedback is decoded directly into rad.
constexpr float P_MIN = -12.5f, P_MAX = 12.5f;
constexpr float V_MIN = -37.5f, V_MAX = 37.5f;
constexpr float T_MIN = -32.0f, T_MAX = 32.0f;
constexpr float KP_MAX = 500.0f, KD_MAX = 5.0f;
constexpr uint32_t COMMAND_HZ = 1000000UL / TX_PERIOD_US;
static_assert((1000000UL % TX_PERIOD_US) == 0, "TX_PERIOD_US must divide 1 second");
static_assert((TX_PERIOD_US % RX_POLL_US) == 0, "RX_POLL_US must divide TX_PERIOD_US");
static_assert(MOTOR1_ON && MOTOR2_ON,
              "Gravity compensation requires BOTH motor position feedback streams");
static_assert(K_G >= 0.0f && K_G <= 1.0f, "K_G outside configured safe range [0, 1]");
static_assert(MAX_ABS_COMMAND_TORQUE_NM > 0.0f &&
              MAX_ABS_COMMAND_TORQUE_NM <= T_MAX, "Invalid torque command limit");

using Can1Type = FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16>;
using Can2Type = FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16>;
static Can1Type can1;
static Can2Type can2;
static IntervalTimer canTimer;

static inline bool motorOn(uint8_t i) {
  return (i == 0) ? MOTOR1_ON : (i == 1 ? MOTOR2_ON : false);
}

enum RunState : uint8_t { IDLE, ZERO_MODE, GRAVITY_MODE, FAULT_STATE };
enum FaultReason : uint8_t {
  NO_FAULT, TX_FAILED, TX_BACKLOG, RX_TIMEOUT, RX_OVERRUN,
  RX_BAD_MOTOR_CODE, MOTOR_TORQUE_LIMIT, CAN_BUS_OFF, ISR_OVERRUN,
  INVALID_ANGLE, TORQUE_COMMAND_LIMIT
};

// Avoid Arduino-generated prototype ordering issues with typed enums.
static void setFault(uint8_t i, FaultReason reason);
static const char *faultName(FaultReason reason);

struct MotorFeedback {
  uint32_t rxValid = 0;
  uint32_t lastRxUs = 0;
  bool haveRx = false;
  float positionRad = 0.0f;  // MIT feedback position, already rad
  float torqueFeedback = 0.0f;
  uint8_t motorError = 0;
};

static MotorFeedback motor[2];
static CAN_message_t zeroFrame[2];
static volatile RunState state = IDLE;
static volatile FaultReason faultReason = NO_FAULT;
static volatile uint8_t faultMotor = 255;
static volatile bool gravityRequested = false;
static bool timerActive = false;
static uint32_t sessionStartUs = 0;
static uint32_t gravityStartUs = 0;
static uint8_t txDivider = 0;

static uint16_t encodeFloat(float x, float lo, float hi, uint8_t bits) {
  x = constrain(x, lo, hi);
  return static_cast<uint16_t>((x - lo) * float((1UL << bits) - 1) / (hi - lo));
}
static float decodeFloat(uint16_t raw, float lo, float hi, uint8_t bits) {
  return float(raw) * (hi - lo) / float((1UL << bits) - 1) + lo;
}

// Preserve the original Jia MIT 8-byte packing and zero-command setup.
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

// Set MIT torque data without altering p=0, v=0, kp=0, kd=0.
// The torque command is rounded to the nearest 12-bit code.
static CAN_message_t makeTorqueFrame(uint8_t i, float torqueNm) {
  CAN_message_t m = zeroFrame[i];
  const float raw = (torqueNm - T_MIN) * (4095.0f / (T_MAX - T_MIN));
  int32_t code = static_cast<int32_t>(floorf(raw + 0.5f));
  if (code < 0) code = 0;
  if (code > 4095) code = 4095;
  const uint16_t t = static_cast<uint16_t>(code);
  m.buf[6] = (m.buf[6] & 0xf0) | (t >> 8);
  m.buf[7] = t & 0xff;
  return m;
}

static void initializeGravityModel() {
  const float maxTarget = K_G * fmaxf(fabsf(G1), fabsf(G2));
  gravityCoefficientsValid = isfinite(G1) && isfinite(G2) &&
                             isfinite(maxTarget) &&
                             maxTarget <= MAX_ABS_COMMAND_TORQUE_NM;
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
  gravityRequested = false;
  state = FAULT_STATE;  // Suppress subsequent nonzero commands immediately.
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
  m.buf[7] = code; // 0xFC enter MIT, 0xFD exit MIT
  const int result = writeFrame(i, m);
  return result == 1 || result == -1;
}

// Jia Mode1 feedback, same identity and torque decoding as the source code:
// byte 0: motor ID (when provided)
// bytes 1,2: 16-bit position, mapped from P_MIN..P_MAX directly in rad
// bytes 3,4: 12-bit velocity
// bytes 4,5: 12-bit torque
// byte 7: motor error/status, as assumed by the tested original program
static void parseFeedback(uint8_t i, const CAN_message_t &m, uint32_t now) {
  if (m.flags.overrun) { setFault(i, RX_OVERRUN); return; }
  if (m.flags.extended || m.flags.remote || m.len < 6 || m.len > 8) return;
  if (m.id != MOTOR_ID[i] && m.buf[0] != MOTOR_ID[i]) return;
  MotorFeedback &d = motor[i];
  const uint16_t p = (uint16_t(m.buf[1]) << 8) | m.buf[2];
  const uint16_t t = (uint16_t(m.buf[4] & 0x0f) << 8) | m.buf[5];
  d.positionRad = decodeFloat(p, P_MIN, P_MAX, 16);
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

static bool checkControlSafety(uint32_t now) {
  for (uint8_t i = 0; i < 2; ++i) if (motorOn(i)) {
    const uint32_t esr = esr1(i);
    if (FLEXCAN_ESR_get_fault_code(esr) == CAN_ERROR_BUS_OFF) {
      setFault(i, CAN_BUS_OFF); return false;
    }
    const MotorFeedback &d = motor[i];
    if (state == ZERO_MODE) {
      if (!d.haveRx && now - sessionStartUs > INITIAL_RX_TIMEOUT_US) {
        setFault(i, RX_TIMEOUT); return false;
      }
      if (d.haveRx && now - d.lastRxUs > RX_MAX_AGE_US) {
        setFault(i, RX_TIMEOUT); return false;
      }
    } else if (state == GRAVITY_MODE) {
      if (!d.haveRx || now - d.lastRxUs > RX_MAX_AGE_US) {
        setFault(i, RX_TIMEOUT); return false;
      }
      if (!isfinite(d.positionRad)) {
        setFault(i, INVALID_ANGLE); return false;
      }
      if (TORQUE_FEEDBACK_LIMIT_ENABLED &&
          fabsf(d.torqueFeedback) > MAX_REPORTED_TORQUE_NM) {
        setFault(i, MOTOR_TORQUE_LIMIT); return false;
      }
    }
  }
  return true;
}

// Both motor torques depend on the latest motor feedback angles.
static bool calculateGravityTorques(float &tauM1, float &tauM2) {
  const float M1raw = motor[0].positionRad;  // AK80 M1 feedback [rad]
  const float M2raw = motor[1].positionRad;  // AK80 M2 feedback [rad]

  // AK80 feedback angles -> link angles [rad].
  const float theta1 = -(M2raw - M2E) + q1E;
  const float theta2 =  (M1raw - M1E) - q2E - theta_off;
  if (!isfinite(theta1) || !isfinite(theta2)) return false;

  // Exact requested steady-state gravity compensation [Nm]:
  // tauG_M1 =  G2 * cos(theta2) * K_G
  // tauG_M2 = -G1 * cos(theta1) * K_G
  const float tauG_M1 =  G2 * cosf(theta2) * K_G;
  const float tauG_M2 = -G1 * cosf(theta1) * K_G;
  if (!isfinite(tauG_M1) || !isfinite(tauG_M2)) return false;

  tauM1 = tauG_M1;
  tauM2 = tauG_M2;
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

  if (state == ZERO_MODE || state == GRAVITY_MODE) {
    if (++txDivider >= TX_PERIOD_US / RX_POLL_US) {
      txDivider = 0;
      const uint32_t now = micros();
      if (!checkControlSafety(now)) return;

      if (state == ZERO_MODE && gravityRequested) {
        gravityRequested = false;
        gravityStartUs = now;
        state = GRAVITY_MODE;
      }

      float commandedTorque[2] = {0.0f, 0.0f};
      if (state == GRAVITY_MODE) {
        if (!calculateGravityTorques(commandedTorque[0], commandedTorque[1])) {
          setFault(0, INVALID_ANGLE);
          return;
        }
        // Apply an independent torque ceiling to the full targets BEFORE ramp.
        for (uint8_t i = 0; i < 2; ++i) {
          if (fabsf(commandedTorque[i]) > MAX_ABS_COMMAND_TORQUE_NM) {
            setFault(i, TORQUE_COMMAND_LIMIT);
            return;
          }
        }
        // Output ramp; at/after GRAVITY_RAMP_MS, the command matches
        // tauG_M1 and tauG_M2 exactly, apart from MIT torque quantization.
        if (GRAVITY_RAMP_MS != 0) {
          const uint32_t elapsedUs = now - gravityStartUs;
          const uint32_t rampUs = GRAVITY_RAMP_MS * 1000UL;
          const float ramp = (elapsedUs >= rampUs) ? 1.0f :
                             float(elapsedUs) / float(rampUs);
          commandedTorque[0] *= ramp;
          commandedTorque[1] *= ramp;
        }
      }

      for (uint8_t i = 0; i < 2 &&
           (state == ZERO_MODE || state == GRAVITY_MODE); ++i) {
        if (!motorOn(i)) continue;
        const CAN_message_t frame = (state == GRAVITY_MODE)
            ? makeTorqueFrame(i, commandedTorque[i]) : zeroFrame[i];
        if (!sendPeriodFrame(i, frame)) break;
      }
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
  // Best effort only; a disconnected or bus-off motor may not receive these.
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
    case INVALID_ANGLE: return "INVALID_ANGLE";
    case TORQUE_COMMAND_LIMIT: return "TORQUE_CMD_LIMIT";
    default: return "NONE";
  }
}
static void finishSession(bool faulted) {
  if (state == IDLE) return;
  state = IDLE;
  gravityRequested = false;
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
  gravityRequested = false;
  faultReason = NO_FAULT;
  faultMotor = 255;
  txDivider = 0;
  sessionStartUs = micros();
  // As in the working original: send ENABLE first, not zero immediately.
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
  Serial.println("ZERO_STARTED,USE_G_TO_START_GRAVITY");
}

static void requestGravity() {
  if (state != ZERO_MODE) {
    Serial.println("REJECT,START_ZERO_MODE_WITH_S_FIRST"); return;
  }
  if (!gravityCoefficientsValid) {
    Serial.println("REJECT,GRAVITY_MODEL_INVALID_OR_TORQUE_LIMIT"); return;
  }
  const uint32_t now = micros();
  MotorFeedback snapshot[2];
  noInterrupts();
  snapshot[0] = motor[0]; snapshot[1] = motor[1];
  interrupts();
  for (uint8_t i = 0; i < 2; ++i) if (motorOn(i)) {
    const MotorFeedback &d = snapshot[i];
    if (!d.haveRx || d.rxValid < RX_MIN_COUNT_FOR_GRAVITY ||
        now - d.lastRxUs > RX_MAX_AGE_US || d.motorError != 0 ||
        !isfinite(d.positionRad) ||
        txQueue(i) != 0 ||
        FLEXCAN_ESR_get_fault_code(esr1(i)) != CAN_ERROR_ACTIVE) {
      Serial.print("REJECT,M"); Serial.print(i + 1);
      Serial.println(",FEEDBACK_NOT_READY");
      return;
    }
  }
  noInterrupts();
  if (state == ZERO_MODE) gravityRequested = true;
  interrupts();
  Serial.print("GRAVITY_ARMED,K_G,"); Serial.print(K_G, 3);
  Serial.print(",RAMP_MS,"); Serial.println(GRAVITY_RAMP_MS);
}

static void onCommand(char *c) {
  for (char *p = c; *p; ++p) *p = char(toupper((unsigned char)*p));
  if (!strcmp(c, "S")) startZeroMode();
  else if (!strcmp(c, "G")) requestGravity();
  else if (!strcmp(c, "X")) finishSession(false);
  else Serial.println("COMMANDS,S,G,X");
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
  for (uint8_t i = 0; i < 2; ++i) zeroFrame[i] = makeZeroFrame(i);
  initializeGravityModel();
  if (MOTOR1_ON) {
    can1.begin(); can1.setBaudRate(CAN_BAUD);
    NVIC_SET_PRIORITY(IRQ_CAN1, CAN_IRQ_PRIORITY);
  }
  if (MOTOR2_ON) {
    can2.begin(); can2.setBaudRate(CAN_BAUD);
    NVIC_SET_PRIORITY(IRQ_CAN2, CAN_IRQ_PRIORITY);
  }
  // Preserve idle 100 us drain to avoid the original startup RX issue.
  if (!startTimer()) Serial.println("FAULT,TIMER_START_FAILED");
  Serial.print("READY,AK80_Jia_GravityComp,K_G,"); Serial.print(K_G, 3);
  Serial.print(",TX_HZ,"); Serial.print(COMMAND_HZ);
  Serial.print(",RAMP_MS,"); Serial.print(GRAVITY_RAMP_MS);
  Serial.print(",MAX_CMD_NM,"); Serial.println(MAX_ABS_COMMAND_TORQUE_NM, 3);
  Serial.print("G1_NM,"); Serial.print(G1, 6);
  Serial.print(",G2_NM,"); Serial.println(G2, 6);
  if (!gravityCoefficientsValid)
    Serial.println("FAULT,INVALID_GRAVITY_MODEL_OR_COMMAND_TORQUE_LIMIT");
  Serial.println("M1=ON,M2=ON");
  Serial.println("COMMANDS,S=ZERO,G=GRAVITY,X=STOP");
}
void loop() {
  pollSerial();
  // Preserve the bounded FlexCAN error-queue drain from the original program.
  for (uint8_t i = 0; i < 2; ++i) if (motorOn(i)) {
    for (uint8_t n = 0; n < 4; ++n) {
      CAN_error_t errorSnapshot{};
      if (!(i == 0 ? can1.error(errorSnapshot, false)
                   : can2.error(errorSnapshot, false))) break;
    }
  }
  if (state == FAULT_STATE) finishSession(true);
}
