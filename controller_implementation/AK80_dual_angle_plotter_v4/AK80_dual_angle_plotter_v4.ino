/*
 * AK80_dual_angle_plotter_v4.ino
 * Teensy 4.1, FlexCAN_T4, AK80 Jia Mode1 (MIT)
 * Based on CAN_debug_AK80_Jia_TimerISR_v4_dual_motor.
 *
 * Serial output: two numeric CSV columns, M1_deg,M2_deg, no headers/logs.
 * M1: CAN1 / ID1, M2: CAN2 / ID2.
 * 100-us ISR polls RX; 500-us ticks transmit MIT zero torque.
 * Each output averages the latest non-overlapping 2-kHz sample block
 * (200 samples/100 ms at 10 Hz). Invalid/stale data prints nan.
 *
 * AUTO_START controls MIT ENABLE on startup; zero-torque commands follow.
 * Serial input (silent): S = start if stopped, X = zero + disable/stop.
 * Supply motor power before upload/reset, or send S after powering motors.
 *
 * IMPORTANT: zero torque is NOT a physical emergency stop. Keep a
 * hardware power-off/stop method available. Do not attach a load that
 * can move dangerously in free motion.
 */

#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <IntervalTimer.h>
#include <math.h>

// ======================== USER SETTINGS ========================
constexpr uint32_t SERIAL_OUTPUT_HZ = 10;       // <-- Change this (e.g. 5, 20, 50, 100)
constexpr bool AUTO_START = true;              // true: start immediately; false: require S
constexpr uint32_t SERIAL_BAUD = 115200;
// ===============================================================

// Preserved v4 communication settings / CAN topology.
constexpr uint8_t MOTOR_ID[2] = {1, 2};
constexpr uint32_t CAN_BAUD = 1000000;
constexpr uint32_t TX_PERIOD_US = 500;         // 2 kHz each motor
constexpr uint32_t RX_POLL_PERIOD_US = 100;    // 10 kHz polling
constexpr uint8_t RX_BUDGET_PER_TICK = 4;
constexpr uint8_t TIMER_ISR_PRIORITY = 96;
constexpr uint8_t CAN_IRQ_PRIORITY = 128;
constexpr uint32_t RX_TIMEOUT_US = 500000;     // v4 timeout, 0.5 s
constexpr uint32_t FRESH_ANGLE_US = 3000;      // Do not average indefinitely held samples

constexpr float P_MIN = -12.5f, P_MAX = 12.5f; // v4 MIT position range, rad
constexpr float V_MIN = -37.5f, V_MAX = 37.5f;
constexpr float T_MIN = -32.0f, T_MAX = 32.0f;
constexpr float KP_MAX = 500.0f, KD_MAX = 5.0f;
constexpr float RAD_TO_DEG_F = 57.29577951308232f;
constexpr uint32_t CONTROL_HZ = 1000000UL / TX_PERIOD_US;
static_assert(SERIAL_OUTPUT_HZ >= 1 && SERIAL_OUTPUT_HZ <= CONTROL_HZ,
              "SERIAL_OUTPUT_HZ must be in [1, CONTROL_HZ]");
static_assert(1000000UL % TX_PERIOD_US == 0, "TX period must be an integer Hz");
static_assert(TX_PERIOD_US % RX_POLL_PERIOD_US == 0, "TX period must be a multiple of RX period");

using Can1Type = FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16>;
using Can2Type = FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16>;
static Can1Type can1;
static Can2Type can2;
static IntervalTimer canTimer;
static CAN_message_t zeroFrame[2];

struct MotorFeedback {
  float positionDeg = 0.0f;
  uint32_t lastRxUs = 0;
  bool valid = false;
};

// Accessed only by the high-priority CAN timer ISR, except while timer is stopped.
static MotorFeedback feedback[2];
static float sumDeg[2] = {0.0f, 0.0f};
static uint32_t validSampleCount[2] = {0, 0};
static uint32_t outputPhase = 0; // exact downsampling from 2-kHz ticks
static uint8_t txDivider = 0;
static uint32_t runStartUs = 0;
static volatile bool running = false;
static volatile bool faultStopRequested = false;
static bool timerActive = false;

// One completed 100-ms window. Access across ISR/loop is protected by noInterrupts().
static volatile float outputDeg[2] = {NAN, NAN};
static volatile bool outputReady = false;

static uint16_t floatToUint(float value, float lo, float hi, uint8_t bits) {
  value = constrain(value, lo, hi);
  return static_cast<uint16_t>((value - lo) * float((1UL << bits) - 1) / (hi - lo));
}

static float uintToFloat(uint16_t value, float lo, float hi, uint8_t bits) {
  return float(value) * (hi - lo) / float((1UL << bits) - 1) + lo;
}

// Exact Jia Mode1 zero-torque packing used in v4: p=v=kp=kd=t=0.
static void prepareZeroFrames() {
  const uint16_t p = floatToUint(0.0f, P_MIN, P_MAX, 16);
  const uint16_t v = floatToUint(0.0f, V_MIN, V_MAX, 12);
  const uint16_t kp = floatToUint(0.0f, 0.0f, KP_MAX, 12);
  const uint16_t kd = floatToUint(0.0f, 0.0f, KD_MAX, 12);
  const uint16_t t = floatToUint(0.0f, T_MIN, T_MAX, 12);

  for (uint8_t i = 0; i < 2; ++i) {
    CAN_message_t &m = zeroFrame[i];
    m = CAN_message_t{};
    m.id = MOTOR_ID[i];
    m.len = 8;
    m.flags.extended = 0;
    m.flags.remote = 0;
    m.buf[0] = p >> 8;
    m.buf[1] = p & 0xFF;
    m.buf[2] = v >> 4;
    m.buf[3] = ((v & 0x0F) << 4) | (kp >> 8);
    m.buf[4] = kp & 0xFF;
    m.buf[5] = kd >> 4;
    m.buf[6] = ((kd & 0x0F) << 4) | (t >> 8);
    m.buf[7] = t & 0xFF;
  }
}

static int writeRaw(uint8_t i, const CAN_message_t &m) {
  return i == 0 ? can1.write(m) : can2.write(m);
}

// v4's accepted return values (1: mailbox, -1: library TX queue).
static bool txAccepted(int result) {
  return result == 1 || result == -1;
}

static bool sendSpecial(uint8_t i, uint8_t code) {
  CAN_message_t m{};
  m.id = MOTOR_ID[i];
  m.len = 8;
  m.flags.extended = 0;
  m.flags.remote = 0;
  for (uint8_t b = 0; b < 7; ++b) m.buf[b] = 0xFF;
  m.buf[7] = code;                    // FC = MIT enable, FD = MIT exit
  return txAccepted(writeRaw(i, m));
}

static void pollMotor(uint8_t i, bool decode) {
  for (uint8_t n = 0; n < RX_BUDGET_PER_TICK; ++n) {
    CAN_message_t msg{};
    const int got = i == 0 ? can1.read(msg) : can2.read(msg);
    if (!got) break;
    if (!decode) continue;           // Drain old RX while IDLE (as in v4).

    // v4's Jia Mode1 acceptance: CAN ID OR first payload byte equals motor ID.
    if (msg.flags.extended || msg.flags.remote || msg.len < 6 || msg.len > 8)
      continue;
    if (msg.id != MOTOR_ID[i] && msg.buf[0] != MOTOR_ID[i])
      continue;

    const uint16_t rawP = (uint16_t(msg.buf[1]) << 8) | msg.buf[2];
    feedback[i].positionDeg = uintToFloat(rawP, P_MIN, P_MAX, 16) * RAD_TO_DEG_F;
    feedback[i].lastRxUs = micros();
    feedback[i].valid = true;

    // Preserve v4's protection against motor-reported faults.
    if (msg.len >= 8 && msg.buf[7] != 0) {
      running = false;
      faultStopRequested = true;
      return;
    }
  }
}

static void finishAveragingWindow(uint32_t now) {
  // Zero-order-held feedback, averaged over each consecutive block of
  // 2-kHz control samples (200 samples per 10-Hz output interval).
  float avg[2];
  for (uint8_t i = 0; i < 2; ++i) {
    const bool freshNow = running && feedback[i].valid &&
                          (uint32_t)(now - feedback[i].lastRxUs) <= FRESH_ANGLE_US;
    avg[i] = (freshNow && validSampleCount[i]) ?
             (sumDeg[i] / float(validSampleCount[i])) : NAN;
    sumDeg[i] = 0.0f;
    validSampleCount[i] = 0;
  }
  outputDeg[0] = avg[0];
  outputDeg[1] = avg[1];
  outputReady = true;
}

static void timerISR() {
  const uint32_t isrStartUs = micros();
  const bool wasRunning = running;
  pollMotor(0, wasRunning);
  if (running || !wasRunning) pollMotor(1, running);

  if (++txDivider >= TX_PERIOD_US / RX_POLL_PERIOD_US) {
    txDivider = 0;
    const uint32_t sampleUs = micros();
    if (running) {
      // Bus-off / lost RX / TX rejection: stop zero commands and request MIT EXIT.
      for (uint8_t i = 0; i < 2; ++i) {
        const uint32_t esr = FLEXCANb_ESR1(i == 0 ? CAN1 : CAN2);
        if (FLEXCAN_ESR_get_fault_code(esr) == CAN_ERROR_BUS_OFF ||
            (uint32_t)(sampleUs - (feedback[i].valid ? feedback[i].lastRxUs : runStartUs))
                >= RX_TIMEOUT_US) {
          running = false;
          faultStopRequested = true;
        }
      }
      if (running) {
        for (uint8_t i = 0; i < 2; ++i) {
          if (!txAccepted(writeRaw(i, zeroFrame[i]))) {
            running = false;
            faultStopRequested = true;
            break;
          }
        }
      }
    }

    // Sample angle at exactly the 2-kHz control rate, independent of RX burst rate.
    // A missing feedback sample is excluded (not replaced with a false zero).
    for (uint8_t i = 0; i < 2; ++i) {
      if (running && feedback[i].valid &&
          (uint32_t)(sampleUs - feedback[i].lastRxUs) <= FRESH_ANGLE_US) {
        sumDeg[i] += feedback[i].positionDeg;
        ++validSampleCount[i];
      }
    }

    // Count controller ticks rather than using micros(), ensuring exactly
    // 200 averaged samples at 10 Hz and correct floor/ceil block sizes
    // for arbitrary integer output frequencies up to 2 kHz.
    outputPhase += SERIAL_OUTPUT_HZ;
    if (outputPhase >= CONTROL_HZ) {
      outputPhase -= CONTROL_HZ;
      finishAveragingWindow(sampleUs);
    }
  }
  // Restore v4 behavior: an ISR overrun is a fault, not a silent loss
  // of the requested 10-kHz RX-polling schedule.
  if (running && (uint32_t)(micros() - isrStartUs) > RX_POLL_PERIOD_US) {
    running = false;
    faultStopRequested = true;
  }
}

static void resetFeedbackAndAverage() {
  feedback[0] = MotorFeedback{};
  feedback[1] = MotorFeedback{};
  sumDeg[0] = sumDeg[1] = 0.0f;
  validSampleCount[0] = validSampleCount[1] = 0;
  txDivider = 0;
  outputPhase = 0;
  noInterrupts();
  outputDeg[0] = outputDeg[1] = NAN;
  outputReady = false;
  interrupts();
}

static void startTimer() {
  canTimer.priority(TIMER_ISR_PRIORITY);
  timerActive = canTimer.begin(timerISR, RX_POLL_PERIOD_US);
}

static void stopTimer() {
  if (timerActive) {
    canTimer.end();
    timerActive = false;
  }
}

static void startRun() {
  if (running) return;  // Match v4: S while RUNNING is a no-op.
  stopTimer();
  running = false;
  faultStopRequested = false;
  resetFeedbackAndAverage();
  pollMotor(0, false);                // Discard stale mailbox data before enable.
  pollMotor(1, false);
  const bool ok1 = sendSpecial(0, 0xFC);
  const bool ok2 = sendSpecial(1, 0xFC);
  // v4 sends the first periodic ZERO 500 us after the ENABLE frames.
  runStartUs = micros();
  running = ok1 && ok2;
  if (!running) {
    writeRaw(0, zeroFrame[0]);
    writeRaw(1, zeroFrame[1]);
    sendSpecial(0, 0xFD);
    sendSpecial(1, 0xFD);
  }
  startTimer();
}

static void stopRun() {
  stopTimer();                        // No concurrent TX during the stop sequence.
  running = false;
  faultStopRequested = false;
  writeRaw(0, zeroFrame[0]);         // Best-effort zero before MIT exit.
  writeRaw(1, zeroFrame[1]);
  sendSpecial(0, 0xFD);
  sendSpecial(1, 0xFD);
  startTimer();                      // IDLE still drains CAN RX; print nan,nan.
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  prepareZeroFrames();
  can1.begin();
  can1.setBaudRate(CAN_BAUD);
  NVIC_SET_PRIORITY(IRQ_CAN1, CAN_IRQ_PRIORITY);
  can2.begin();
  can2.setBaudRate(CAN_BAUD);
  NVIC_SET_PRIORITY(IRQ_CAN2, CAN_IRQ_PRIORITY);
  // Intentionally no onReceive(), enableMBInterrupt(), or CAN events().
  resetFeedbackAndAverage();
  if (AUTO_START) startRun();
  else startTimer();
}

void loop() {
  // Silent, optional commands. No CSV-breaking text is ever printed.
  while (Serial.available()) {
    const char c = char(Serial.read());
    if (c == 'S' || c == 's') startRun();
    else if (c == 'X' || c == 'x') stopRun();
  }
  if (faultStopRequested) stopRun();

  if (outputReady) {
    float a1, a2;
    noInterrupts();
    a1 = outputDeg[0];
    a2 = outputDeg[1];
    outputReady = false;
    interrupts();
    Serial.print(a1, 4);
    Serial.print(',');
    Serial.println(a2, 4);
  }
}
