/*
 * CAN_debug_AK80_Jia_TimerISR_v5_m1only_100us_isr_first_error_AI.ino
 * Teensy 4.1 / FlexCAN_T4 / Jia Mode1 (MIT) / IntervalTimer
 *
 * Source policy:
 *  - CAN protocol, send packing, polling read/write: working Jia sketch.
 *  - Physical ranges, motor selection, period, diagnosis: supplied diagnostic sketch.
 *  - No onReceive(), no enableMBInterrupt(), no CAN events() for RX.
 *  - Timer ISR polls RX continuously in IDLE (no TX), drains stale mailboxes.
 *  - RUNNING: 100-us ISR timebase, RX polling every 100 us.
 *  - M1-only periodic zero TX remains at +500,+1000,... us (phase 0).
 *  - M2 is disabled for this channel comparison; its 250-us phase setting is untouched.
 *  - First S can reinitialize the CAN peripheral after motor power-on.
 *  - loop() handles Serial, prints buffered diagnostic events and stop requests.
 *
 * Commands: S, X, STATUS, HISTORY, EVENTS, RESET, HELP
 * CAUTION: MIT zero torque requires kp=kd=t=0. CAN stop is best effort.
 *          This software is not an independent emergency-stop mechanism.
 */

#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <IntervalTimer.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

// ==================== SETTINGS (from newer diagnostic program) ====================
constexpr bool MOTOR1_ON = true;            // M1-only comparison: CAN1, motor ID 1
constexpr bool MOTOR2_ON = false;           // M2 disabled for channel comparison
constexpr uint8_t MOTOR_ID[2] = {1, 2};
constexpr uint32_t CAN_BAUD = 1000000;
constexpr uint32_t COMMAND_PERIOD_US = 500; // 2 kHz torque command
constexpr uint32_t RX_POLL_PERIOD_US = 100; // 10 kHz RX polling for each motor
constexpr uint32_t TIMER_BASE_PERIOD_US = 100; // 10 kHz phase scheduler (RX every tick)
constexpr uint32_t TX_M2_PHASE_US = 250;    // M2 zero-TX follows M1 by 250 us
constexpr uint32_t PRINT_INTERVAL_MS = 1000;
constexpr uint32_t RX_WARNING_US = 5000;
constexpr uint32_t RX_TIMEOUT_US = 500000;
constexpr uint32_t HISTORY_INTERVAL_US = 10000;
constexpr uint16_t HISTORY_SIZE = 200;     // 2 s / motor
constexpr uint16_t EVENT_SIZE = 128;
constexpr uint8_t RX_BUDGET_PER_TICK = 4;  // per motor per RX-poll tick
constexpr bool STOP_ON_RX_HW_OVERRUN = false; // diagnosis: count while zero-torque; timeout/bus-off still stop
constexpr uint16_t TX_QUEUE_WARNING = 8;  // diagnostics only
constexpr uint32_t JITTER_WARNING_US = 50;
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint8_t TIMER_ISR_PRIORITY = 96; // lower number = higher priority
constexpr uint8_t CAN_IRQ_PRIORITY = 128;  // enable TX-completion processing

// ==================== POWER-UP / PRE-START DIAGNOSTICS ====================
constexpr bool IDLE_RX_DRAIN = true;         // Poll RX while waiting for S, NO TX
constexpr bool REINIT_CAN_ON_FIRST_S = false; // Verified: idle RX drain works without CAN reinit

constexpr float POSITION_MIN = -12.5f, POSITION_MAX = 12.5f;
constexpr float VELOCITY_MIN = -37.5f, VELOCITY_MAX = 37.5f; // corrected range
constexpr float TORQUE_MIN = -32.0f, TORQUE_MAX = 32.0f;
constexpr float MIT_KP_MAX = 500.0f, MIT_KD_MAX = 5.0f;
static_assert(MOTOR1_ON || MOTOR2_ON, "Enable at least one motor");
static_assert(COMMAND_PERIOD_US > 0, "Invalid timer period");
static_assert(RX_POLL_PERIOD_US > 0 && COMMAND_PERIOD_US % RX_POLL_PERIOD_US == 0,
              "COMMAND_PERIOD_US must be multiple of RX_POLL_PERIOD_US");
static_assert(TIMER_BASE_PERIOD_US > 0 && RX_POLL_PERIOD_US % TIMER_BASE_PERIOD_US == 0,
              "RX_POLL_PERIOD_US must be multiple of TIMER_BASE_PERIOD_US");
static_assert(COMMAND_PERIOD_US % TIMER_BASE_PERIOD_US == 0 &&
              (!MOTOR2_ON || (TX_M2_PHASE_US % TIMER_BASE_PERIOD_US == 0 &&
                              TX_M2_PHASE_US > 0 && TX_M2_PHASE_US < COMMAND_PERIOD_US)),
              "Command period must align; enabled M2 phase must align and be within one TX period");
static_assert(COMMAND_PERIOD_US / TIMER_BASE_PERIOD_US < 255,
              "Phase scheduler slot exceeds uint8_t");

static inline bool motorOn(uint8_t i) {
  return i == 0 ? MOTOR1_ON : (i == 1 ? MOTOR2_ON : false);
}

using Can1Type = FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16>;
using Can2Type = FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16>;
static Can1Type can1;
static Can2Type can2;
static IntervalTimer commandTimer;

enum RunState : uint8_t { IDLE, RUNNING, FAULT_STOPPED };
enum TxKind : uint8_t {
  PERIODIC_ZERO, START_ENABLE, START_ZERO, STOP_ZERO, STOP_DISABLE, TX_KIND_COUNT
};
enum FaultKind : uint8_t {
  FAULT_NONE, FAULT_TX_FAIL, FAULT_RX_TIMEOUT, FAULT_BUS_OFF,
  FAULT_RX_HW_OVERRUN, FAULT_ISR_OVERRUN, FAULT_MOTOR_ERROR,
  FAULT_CAN_ERROR
};
enum EventKind : uint8_t {
  EV_CAN_ERROR, EV_CAN_CLEAR, EV_RX_GAP_BEGIN, EV_RX_GAP_END,
  EV_RX_REJECT, EV_RX_HW_OVERRUN, EV_TX_BACKLOG, EV_TX_FAILURE,
  EV_FAULT, EV_START, EV_STOP, EV_CAN_IRQ_SNAPSHOT
};

struct MotorStats {
  uint32_t rxRaw = 0, rxValid = 0, rxRejected = 0;
  uint32_t rejectId = 0, rejectPayload = 0, rejectDlc = 0, rejectFormat = 0;
  uint32_t rxHwOverrun = 0;
  uint32_t rxNoncanonical = 0;
  bool hasBadFrame = false;
  uint32_t firstBadId = 0;
  uint8_t firstBadPayloadId = 0, firstBadLen = 0, firstBadFlags = 0;
  uint32_t lastRxUs = 0, maxRxGapUs = 0;
  bool sawRx = false, rxGapActive = false, txBacklogActive = false;
  uint32_t rxGapStartUs = 0, maxWarningDurationUs = 0;
  uint8_t motorError = 0;
  float positionRad = 0, velocityRadS = 0, torqueNm = 0;
  uint32_t txAccepted[TX_KIND_COUNT] = {0};
  uint32_t txQueued[TX_KIND_COUNT] = {0};
  uint32_t txFailures[TX_KIND_COUNT] = {0};
  int16_t lastTxResult = 0;
  uint32_t txQueueNow = 0, txQueueMax = 0;
  uint32_t esr1 = 0, esrSeen = 0, ecr = 0;
  uint32_t canErrorEvents = 0, canErrorSamples = 0;
  uint32_t canIrqSnapshots = 0, lastIrqSignature = 0, lastIrqLogUs = 0;
  uint32_t lastCanSignature = 0;
  uint8_t maxTec = 0, maxRec = 0;
  uint32_t historyStampUs = 0;
};

struct HistoryRow {
  uint32_t us, tx, txFail, rx, reject, rxHwOverrun, rxAgeUs;
  uint32_t esr1, txQueue, maxIsrUs, maxJitterUs;
  uint8_t tec, rec;
  float p, v, t;
  bool faultRow;
};

struct HistoryBuffer {
  HistoryRow data[HISTORY_SIZE];
  uint16_t head = 0, count = 0;
  bool frozen = false;
};

struct Event {
  uint32_t us, esr, ecr, value;
  uint8_t motor, kind;
};

struct FaultSnapshot {
  bool valid = false;
  FaultKind kind = FAULT_NONE;
  uint8_t motor = 255, motorError = 0;
  uint32_t us = 0, esr = 0, ecr = 0, rxAge = UINT32_MAX;
  uint32_t rxRaw = 0, rxValid = 0, tx = 0, txFail = 0;
  uint32_t txQueue = 0, maxIsr = 0, maxJitter = 0;
};

struct CanErrorSnapshot {
  bool valid = false;
  // This is when loop() dequeued the driver's error record, not the wire-time
  // of the original CAN error (which the driver record does not expose).
  uint32_t detectedUs = 0, errorEsr = 0, errorEcr = 0;
  uint32_t currentEsr = 0, currentEcr = 0;
  uint32_t tx[2] = {0, 0}, txFail[2] = {0, 0}, txQueue[2] = {0, 0};
  uint32_t rxRaw[2] = {0, 0}, rxValid[2] = {0, 0};
  uint32_t rxAgeUs[2] = {UINT32_MAX, UINT32_MAX};
  uint32_t rxGapMaxUs[2] = {0, 0}, rxHwOverrun[2] = {0, 0};
  uint32_t isrMaxUs = 0, jitterMaxUs = 0;
};

struct TxFailure {
  bool valid = false;
  uint32_t us = 0, esr = 0, ecr = 0, rxAge = UINT32_MAX;
  uint32_t queueBefore = 0, queueAfter = 0;
  int result = 0;
  uint8_t motor = 255, kind = 0;
};

// Explicit prototypes keep Arduino sketch preprocessing from moving function
// declarations ahead of the custom enum/struct types above.
static inline bool motorOn(uint8_t i);
static uint16_t floatToUint(float value, float lo, float hi, uint8_t bits);
static float uintToFloat(uint16_t value, float lo, float hi, uint8_t bits);
static uint32_t busBase(uint8_t i);
static uint32_t ecrValue(uint8_t i);
static uint32_t esrValue(uint8_t i);
static uint32_t txQueue(uint8_t i);
static int writeRaw(uint8_t i, const CAN_message_t &m);
static void pushEvent(EventKind kind, uint8_t i, uint32_t us,
                      uint32_t esr, uint32_t ecr, uint32_t value);
static void addHistory(uint8_t i, uint32_t now, bool asFault);
static void latchFault(FaultKind kind, uint8_t i, uint32_t now);
static bool sendFrame(uint8_t i, const CAN_message_t &msg, TxKind kind);
static bool sendSpecial(uint8_t i, uint8_t code, TxKind kind);
static bool sendZeroTorque(uint8_t i, TxKind kind);
static void parseFeedback(uint8_t i, const CAN_message_t &msg, uint32_t now);
static void readMotor(uint8_t i);
static void readIdleMotor(uint8_t i);
static bool startIdleTimer();
static void reinitSelectedCan();
static void sampleCan(uint8_t i, uint32_t now);
static void commandISR();
static void recordPeriodicTxApi(uint8_t i, uint32_t now);
static const char *stateName(RunState s);
static const char *faultName(FaultKind k);
static const char *txKindName(uint8_t k);
static const char *eventName(uint8_t k);
static const char *canStateName(uint32_t esr);
static void printErrorNames(uint32_t esr);
static void eventPrefix(uint32_t timeUs, uint8_t motor);
static void printEvent(const Event &ev);
static void printNewEvents(uint8_t budget);
static void drainCanErrorSnapshots();
static void discardOldCanErrors();
static void printStatus();
static void printHistory();
static void printEvents();
static void printFaultReport();
static void sendStopRequests();
static void stopTimer();
static void serviceFault();
static void resetSession();
static void stopManually();
static void beginSession();
static void handleCommand(char *line);
static void readSerial();
void setup();
void loop();

static MotorStats motors[2];
static HistoryBuffer histories[2];
static Event events[EVENT_SIZE];
static volatile uint32_t eventWriteSeq = 0;
static uint32_t eventPrintSeq = 0;
static uint32_t eventLostForPrint = 0;
static FaultSnapshot fault;
static CanErrorSnapshot firstCanError;
static TxFailure firstTxFailure;
static volatile RunState state = IDLE;
static volatile bool faultPending = false;
static bool timerActive = false, faultOutputDone = false;
static bool firstStartSinceBoot = true;
static uint32_t idleRxRaw[2] = {0, 0};
static uint32_t idleRxHwOverrun[2] = {0, 0};
static uint32_t idleLastRxUs[2] = {0, 0};
static uint32_t idleLastId[2] = {0, 0};
static volatile uint32_t idleTimerTicks = 0;
static volatile uint32_t idleMaxIsrUs = 0;
static uint32_t reinitCount = 0;
static uint32_t sessionStartUs = 0, lastPrintMs = 0;
static uint32_t prevTx[2] = {0, 0}, prevRx[2] = {0, 0};
static volatile uint32_t ticks = 0, maxIsrUs = 0, isrOverruns = 0;
static volatile uint32_t rxPollTicks = 0, maxRxPollUs = 0, rxPollOverruns = 0;
static volatile uint32_t maxRxPollGapUs = 0;
static uint8_t phaseSlot = 0; // cyclic TIMER_BASE ticks for one 500-us TX period
static uint32_t lastRxPollUs = 0;
// Software API-call timestamps (NOT actual transmission timestamps on CANH/CANL).
static volatile uint32_t lastTxApiUs[2] = {0, 0};
static volatile uint32_t txApiMaxPeriodErrorUs[2] = {0, 0};
static volatile uint32_t txApiPhaseLastUs = 0;
static volatile uint32_t txApiPhaseMaxErrorUs = 0;
static volatile uint32_t txApiPhaseSamples = 0;
static volatile uint32_t maxTickGapUs = 0, maxJitterUs = 0, lateTicks = 0;
static uint32_t lastTickUs = 0;

// ==================== BASE CONVERSIONS ====================
static uint16_t floatToUint(float value, float lo, float hi, uint8_t bits) {
  value = constrain(value, lo, hi);
  return static_cast<uint16_t>((value - lo) * float((1UL << bits) - 1) / (hi - lo));
}

static float uintToFloat(uint16_t value, float lo, float hi, uint8_t bits) {
  return float(value) * (hi - lo) / float((1UL << bits) - 1) + lo;
}

static uint32_t busBase(uint8_t i) { return i == 0 ? CAN1 : CAN2; }
static uint32_t ecrValue(uint8_t i) { return FLEXCANb_ECR(busBase(i)); }
static uint32_t esrValue(uint8_t i) { return FLEXCANb_ESR1(busBase(i)); }
static uint32_t txQueue(uint8_t i) {
  return i == 0 ? can1.getTXQueueCount() : can2.getTXQueueCount();
}
static int writeRaw(uint8_t i, const CAN_message_t &m) {
  return i == 0 ? can1.write(m) : can2.write(m);
}

static const uint32_t CAN_ERROR_MASK =
  FLEXCAN_ESR_ACK_ERR | FLEXCAN_ESR_BIT0_ERR | FLEXCAN_ESR_BIT1_ERR |
  FLEXCAN_ESR_STF_ERR | FLEXCAN_ESR_FRM_ERR | FLEXCAN_ESR_CRC_ERR;

// ISR only while running. loop() can reset these buffers only after timer.end().
static void pushEvent(EventKind kind, uint8_t i, uint32_t us,
                      uint32_t esr, uint32_t ecr, uint32_t value = 0) {
  Event &ev = events[eventWriteSeq % EVENT_SIZE];
  ev.us = us; ev.motor = i; ev.kind = static_cast<uint8_t>(kind);
  ev.esr = esr; ev.ecr = ecr; ev.value = value;
  asm volatile("" ::: "memory");
  ++eventWriteSeq;
}

static void addHistory(uint8_t i, uint32_t now, bool asFault = false) {
  HistoryBuffer &hb = histories[i];
  if (hb.frozen && !asFault) return;
  MotorStats &d = motors[i];
  HistoryRow &h = hb.data[hb.head];
  h.us = now;
  h.tx = d.txAccepted[PERIODIC_ZERO];
  h.txFail = d.txFailures[PERIODIC_ZERO];
  h.rx = d.rxValid;
  h.reject = d.rxRejected;
  h.rxHwOverrun = d.rxHwOverrun;
  h.rxAgeUs = d.sawRx ? now - d.lastRxUs : UINT32_MAX;
  h.esr1 = d.esr1;
  h.txQueue = d.txQueueNow;
  h.maxIsrUs = maxIsrUs;
  h.maxJitterUs = maxJitterUs;
  h.tec = d.ecr & 255U;
  h.rec = (d.ecr >> 8) & 255U;
  h.p = d.positionRad; h.v = d.velocityRadS; h.t = d.torqueNm;
  h.faultRow = asFault;
  hb.head = (hb.head + 1) % HISTORY_SIZE;
  if (hb.count < HISTORY_SIZE) ++hb.count;
}

static void latchFault(FaultKind kind, uint8_t i, uint32_t now) {
  if (state != RUNNING) return;
  MotorStats &d = motors[i];
  fault.valid = true;
  fault.kind = kind; fault.motor = i; fault.motorError = d.motorError;
  fault.us = now; fault.esr = d.esr1; fault.ecr = d.ecr;
  fault.rxAge = d.sawRx ? (now - d.lastRxUs) : UINT32_MAX;
  fault.rxRaw = d.rxRaw; fault.rxValid = d.rxValid;
  fault.tx = d.txAccepted[PERIODIC_ZERO];
  fault.txFail = d.txFailures[PERIODIC_ZERO];
  fault.txQueue = d.txQueueNow;
  const uint32_t partialIsrUs = now - lastTickUs;
  fault.maxIsr = (partialIsrUs > maxIsrUs) ? partialIsrUs : maxIsrUs;
  fault.maxJitter = maxJitterUs;
  pushEvent(EV_FAULT, i, now, d.esr1, d.ecr, static_cast<uint32_t>(kind));
  for (uint8_t n = 0; n < 2; ++n) if (motorOn(n)) {
    addHistory(n, now, true);
    histories[n].frozen = true;
  }
  state = FAULT_STOPPED; // immediately stop all periodic TX
  faultPending = true;
}

// ==================== CAN TX (Jia Mode1) ====================
static bool sendFrame(uint8_t i, const CAN_message_t &msg, TxKind kind) {
  MotorStats &d = motors[i];
  const uint32_t before = txQueue(i);
  const int result = writeRaw(i, msg);
  const uint32_t now = micros();
  const uint32_t after = txQueue(i);
  d.lastTxResult = static_cast<int16_t>(result);
  d.txQueueNow = after;
  if (after > d.txQueueMax) d.txQueueMax = after;

  // FlexCAN_T4 may queue a frame. 1 = mailbox accepted; -1 = queued in
  // library versions exposing that code. 0 / -2 = not accepted.
  // CAUTION: some versions convert internal negative queue results to bool 1.
  const bool accepted = (result == 1 || result == -1);
  if (accepted) {
    ++d.txAccepted[kind];
    if (result == -1) ++d.txQueued[kind];
  } else {
    ++d.txFailures[kind];
    if (!firstTxFailure.valid) {
      firstTxFailure.valid = true;
      firstTxFailure.us = now; firstTxFailure.motor = i;
      firstTxFailure.kind = kind; firstTxFailure.result = result;
      firstTxFailure.queueBefore = before; firstTxFailure.queueAfter = after;
      firstTxFailure.esr = esrValue(i); firstTxFailure.ecr = ecrValue(i);
      firstTxFailure.rxAge = d.sawRx ? (now - d.lastRxUs) : UINT32_MAX;
    }
    pushEvent(EV_TX_FAILURE, i, now, esrValue(i), ecrValue(i),
              static_cast<uint32_t>(result));
    if (kind == PERIODIC_ZERO) {
      d.esr1 = esrValue(i); d.ecr = ecrValue(i);
      latchFault(FAULT_TX_FAIL, i, now);
    }
  }
  if (kind == PERIODIC_ZERO) {
    if (after >= TX_QUEUE_WARNING && !d.txBacklogActive) {
      d.txBacklogActive = true;
      pushEvent(EV_TX_BACKLOG, i, now, esrValue(i), ecrValue(i), after);
    } else if (after < TX_QUEUE_WARNING) {
      d.txBacklogActive = false;
    }
  }
  return accepted;
}

static bool sendSpecial(uint8_t i, uint8_t code, TxKind kind) {
  CAN_message_t msg{};
  msg.id = MOTOR_ID[i]; msg.len = 8;
  msg.flags.extended = 0; msg.flags.remote = 0;
  for (uint8_t k = 0; k < 7; ++k) msg.buf[k] = 0xFF;
  msg.buf[7] = code; // 0xFC enter MIT, 0xFD exit MIT
  return sendFrame(i, msg, kind);
}

static bool sendZeroTorque(uint8_t i, TxKind kind) {
  const uint16_t p = floatToUint(0, POSITION_MIN, POSITION_MAX, 16);
  const uint16_t v = floatToUint(0, VELOCITY_MIN, VELOCITY_MAX, 12);
  const uint16_t kp = floatToUint(0, 0, MIT_KP_MAX, 12);
  const uint16_t kd = floatToUint(0, 0, MIT_KD_MAX, 12);
  const uint16_t t = floatToUint(0, TORQUE_MIN, TORQUE_MAX, 12);
  CAN_message_t msg{};
  msg.id = MOTOR_ID[i]; msg.len = 8;
  msg.flags.extended = 0; msg.flags.remote = 0;
  msg.buf[0] = p >> 8;
  msg.buf[1] = p & 0xFF;
  msg.buf[2] = v >> 4;
  msg.buf[3] = ((v & 0x0F) << 4) | (kp >> 8);
  msg.buf[4] = kp & 0xFF;
  msg.buf[5] = kd >> 4;
  msg.buf[6] = ((kd & 0x0F) << 4) | (t >> 8);
  msg.buf[7] = t & 0xFF;
  return sendFrame(i, msg, kind);
}

// ==================== CAN RX (polling, called ONLY in timer ISR) ====================
static void parseFeedback(uint8_t i, const CAN_message_t &msg, uint32_t now) {
  MotorStats &d = motors[i];
  ++d.rxRaw;
  if (msg.flags.overrun) {
    ++d.rxHwOverrun;
    pushEvent(EV_RX_HW_OVERRUN, i, now, esrValue(i), ecrValue(i), d.rxHwOverrun);
    // Hardware overrun indicates lost RX frame(s). During zero-torque diagnosis,
    // allow the test to continue when explicitly configured, to quantify it.
    if (STOP_ON_RX_HW_OVERRUN) {
      d.esr1 = esrValue(i); d.ecr = ecrValue(i);
      latchFault(FAULT_RX_HW_OVERRUN, i, now);
      return;
    }
  }

  // Preserve Jia Mode1's acceptance rule: correct CAN ID OR payload ID.
  // The newer diagnostic sketch required BOTH and could reject working Jia RX.
  // The bus remains dedicated to a single motor; print noncanonical counts.
  const bool idOK = (msg.id == MOTOR_ID[i]);
  const bool payloadOK = (msg.buf[0] == MOTOR_ID[i]);
  if (msg.flags.extended || msg.flags.remote) {
    ++d.rejectFormat;
  } else if (msg.len < 6 || msg.len > 8) {
    ++d.rejectDlc;
  } else if (!idOK && !payloadOK) {
    ++d.rejectId;
  } else {
    if (!idOK || !payloadOK) ++d.rxNoncanonical;
    const uint16_t p = (uint16_t(msg.buf[1]) << 8) | msg.buf[2];
    const uint16_t v = (uint16_t(msg.buf[3]) << 4) | (msg.buf[4] >> 4);
    const uint16_t t = (uint16_t(msg.buf[4] & 0x0F) << 8) | msg.buf[5];
    d.positionRad = uintToFloat(p, POSITION_MIN, POSITION_MAX, 16);
    d.velocityRadS = uintToFloat(v, VELOCITY_MIN, VELOCITY_MAX, 12);
    d.torqueNm = uintToFloat(t, TORQUE_MIN, TORQUE_MAX, 12);
    d.motorError = msg.len >= 8 ? msg.buf[7] : 0;
    if (d.sawRx) {
      const uint32_t gap = now - d.lastRxUs;
      if (gap > d.maxRxGapUs) d.maxRxGapUs = gap;
    }
    if (d.rxGapActive) {
      const uint32_t duration = now - d.rxGapStartUs;
      if (duration > d.maxWarningDurationUs) d.maxWarningDurationUs = duration;
      pushEvent(EV_RX_GAP_END, i, now, d.esr1, d.ecr, duration);
      d.rxGapActive = false;
    }
    d.lastRxUs = now;
    d.sawRx = true;
    ++d.rxValid;
    if (d.motorError != 0) latchFault(FAULT_MOTOR_ERROR, i, now);
    return;
  }

  ++d.rxRejected;
  if (!d.hasBadFrame) {
    d.hasBadFrame = true;
    d.firstBadId = msg.id;
    d.firstBadPayloadId = msg.buf[0];
    d.firstBadLen = msg.len;
    d.firstBadFlags = (msg.flags.extended ? 1 : 0) |
                      (msg.flags.remote ? 2 : 0) |
                      (msg.flags.overrun ? 4 : 0);
  }
  if (d.rxRejected == 1 || (d.rxRejected % 100) == 0)
    pushEvent(EV_RX_REJECT, i, now, esrValue(i), ecrValue(i), d.rxRejected);
}

static void readMotor(uint8_t i) {
  for (uint8_t n = 0; n < RX_BUDGET_PER_TICK && state == RUNNING; ++n) {
    // FlexCAN_T4 only sets flags.overrun on overrun; reset for each read.
    CAN_message_t msg{};
    const int got = (i == 0) ? can1.read(msg) : can2.read(msg);
    if (!got) break;
    parseFeedback(i, msg, micros());
  }
}

// During IDLE / FAULT_STOPPED, drain only: no motor commands, no feedback
// parsing and no session timeout. This prevents stale RX mailbox content
// from surviving until the first S after powering the motor later.
static void readIdleMotor(uint8_t i) {
  for (uint8_t n = 0; n < RX_BUDGET_PER_TICK; ++n) {
    CAN_message_t msg{};
    const int got = (i == 0) ? can1.read(msg) : can2.read(msg);
    if (!got) break;
    ++idleRxRaw[i];
    if (msg.flags.overrun) ++idleRxHwOverrun[i];
    idleLastRxUs[i] = micros();
    idleLastId[i] = msg.id;
  }
}

static void sampleCan(uint8_t i, uint32_t now) {
  MotorStats &d = motors[i];
  d.ecr = ecrValue(i); d.esr1 = esrValue(i);
  const uint8_t tec = d.ecr & 255U, rec = (d.ecr >> 8) & 255U;
  if (tec > d.maxTec) d.maxTec = tec;
  if (rec > d.maxRec) d.maxRec = rec;
  const uint32_t errors = d.esr1 & CAN_ERROR_MASK;
  if (errors) { ++d.canErrorSamples; d.esrSeen |= d.esr1; }
  const uint32_t signature = errors |
    (uint32_t(FLEXCAN_ESR_get_fault_code(d.esr1)) << 24);
  if (signature != d.lastCanSignature) {
    ++d.canErrorEvents;
    pushEvent(signature ? EV_CAN_ERROR : EV_CAN_CLEAR, i, now,
              d.esr1, d.ecr, signature);
    d.lastCanSignature = signature;
  }
  if (FLEXCAN_ESR_get_fault_code(d.esr1) == CAN_ERROR_BUS_OFF) {
    latchFault(FAULT_BUS_OFF, i, now);
    return;
  }
  const uint32_t age = d.sawRx ? now - d.lastRxUs : now - sessionStartUs;
  if (age >= RX_WARNING_US && !d.rxGapActive) {
    d.rxGapActive = true; d.rxGapStartUs = now;
    pushEvent(EV_RX_GAP_BEGIN, i, now, d.esr1, d.ecr, age);
  }
  if (age >= RX_TIMEOUT_US) latchFault(FAULT_RX_TIMEOUT, i, now);
}

// ==================== HIGH PRIORITY TIMER ISR ====================
// Records when software submits each periodic frame. This is NOT a measurement
// of the CAN bus wire start time: mailbox/TX queue and retransmissions can
// delay the physical frame, especially during ACK/BIT/STUFF errors.
static void recordPeriodicTxApi(uint8_t i, uint32_t now) {
  const uint32_t last = lastTxApiUs[i];
  if (last != 0) {
    const uint32_t dt = now - last;
    const uint32_t error = dt > COMMAND_PERIOD_US ?
      dt - COMMAND_PERIOD_US : COMMAND_PERIOD_US - dt;
    if (error > txApiMaxPeriodErrorUs[i]) txApiMaxPeriodErrorUs[i] = error;
  }
  lastTxApiUs[i] = now;
  if (i == 1 && MOTOR1_ON && MOTOR2_ON && lastTxApiUs[0] != 0) {
    const uint32_t dt = now - lastTxApiUs[0];
    if (dt < COMMAND_PERIOD_US) {
      txApiPhaseLastUs = dt;
      const uint32_t error = dt > TX_M2_PHASE_US ?
        dt - TX_M2_PHASE_US : TX_M2_PHASE_US - dt;
      if (error > txApiPhaseMaxErrorUs) txApiPhaseMaxErrorUs = error;
      ++txApiPhaseSamples;
    }
  }
}

// Timer runs at TIMER_BASE_PERIOD_US in RUNNING; M1-only keeps its 500-us phase-0 TX.
// IDLE uses the original 100-us timer and drains RX without transmitting.
static void commandISR() {
  const uint32_t begin = micros();
  if (state != RUNNING) {
    ++idleTimerTicks;
    if (IDLE_RX_DRAIN) {
      if (MOTOR1_ON) readIdleMotor(0);
      if (MOTOR2_ON) readIdleMotor(1);
    }
    const uint32_t idleDuration = micros() - begin;
    if (idleDuration > idleMaxIsrUs) idleMaxIsrUs = idleDuration;
    return;
  }

  // One timebase period advances by one slot; 500 us = 5 slots here.
  if (++phaseSlot >= COMMAND_PERIOD_US / TIMER_BASE_PERIOD_US) phaseSlot = 0;
  const bool rxDue = (phaseSlot % (RX_POLL_PERIOD_US / TIMER_BASE_PERIOD_US) == 0);
  const bool m1CycleDue = (phaseSlot == 0); // first 500 us, then every 500 us
  const bool m2PhaseDue = (phaseSlot == TX_M2_PHASE_US / TIMER_BASE_PERIOD_US &&
                           ticks != 0); // first 750 us, then every 500 us

  if (rxDue) {
    ++rxPollTicks;
    if (lastRxPollUs != 0) {
      const uint32_t gap = begin - lastRxPollUs;
      if (gap > maxRxPollGapUs) maxRxPollGapUs = gap;
    }
    lastRxPollUs = begin;
    // Preserve the proven Jia read/polling mechanism at the unchanged 100-us cadence.
    if (MOTOR1_ON) readMotor(0);
    if (state == RUNNING && MOTOR2_ON) readMotor(1);
  }

  if (m1CycleDue) {
    ++ticks;
    if (lastTickUs != 0) {
      const uint32_t delta = begin - lastTickUs;
      if (delta > maxTickGapUs) maxTickGapUs = delta;
      const uint32_t jitter = delta > COMMAND_PERIOD_US ?
        delta - COMMAND_PERIOD_US : COMMAND_PERIOD_US - delta;
      if (jitter > maxJitterUs) maxJitterUs = jitter;
      if (jitter > JITTER_WARNING_US) ++lateTicks;
    }
    lastTickUs = begin;
    if (state == RUNNING && MOTOR1_ON) {
      sampleCan(0, micros());
      if (state == RUNNING) {
        recordPeriodicTxApi(0, micros());
        sendZeroTorque(0, PERIODIC_ZERO);
      }
    }
    // Store histories at the original 500-us control-cycle boundary.
    const uint32_t now = micros();
    if (state == RUNNING) {
      for (uint8_t i = 0; i < 2; ++i) if (motorOn(i)) {
        MotorStats &d = motors[i];
        if (now - d.historyStampUs >= HISTORY_INTERVAL_US) {
          d.historyStampUs = now;
          addHistory(i, now);
        }
      }
    }
  }

  if (m2PhaseDue && state == RUNNING && MOTOR2_ON) {
    sampleCan(1, micros());
    if (state == RUNNING) {
      recordPeriodicTxApi(1, micros());
      sendZeroTorque(1, PERIODIC_ZERO);
    }
  }

  const uint32_t duration = micros() - begin;
  if (duration > maxIsrUs) maxIsrUs = duration;
  if (duration > maxRxPollUs) maxRxPollUs = duration;
  // Running deadline is now 50 us, not 100 us. Fail safe if it is exceeded.
  if (duration > TIMER_BASE_PERIOD_US) {
    ++rxPollOverruns;
    ++isrOverruns;
    if (state == RUNNING)
      latchFault(FAULT_ISR_OVERRUN, MOTOR1_ON ? 0 : 1, micros());
  }
}

// ==================== PRINTING / NON-REALTIME LOOP ====================
static const char *stateName(RunState s) {
  return s == RUNNING ? "RUNNING" : (s == FAULT_STOPPED ? "FAULT_STOPPED" : "IDLE");
}
static const char *faultName(FaultKind k) {
  switch (k) {
    case FAULT_TX_FAIL: return "TX_FAIL";
    case FAULT_RX_TIMEOUT: return "RX_TIMEOUT";
    case FAULT_BUS_OFF: return "BUS_OFF";
    case FAULT_RX_HW_OVERRUN: return "RX_HW_OVERRUN";
    case FAULT_ISR_OVERRUN: return "ISR_OVERRUN";
    case FAULT_MOTOR_ERROR: return "MOTOR_ERROR";
    case FAULT_CAN_ERROR: return "CAN_ERROR";
    default: return "NONE";
  }
}
static const char *txKindName(uint8_t k) {
  switch (k) {
    case PERIODIC_ZERO: return "PERIODIC_ZERO";
    case START_ENABLE: return "START_ENABLE";
    case START_ZERO: return "START_ZERO";
    case STOP_ZERO: return "STOP_ZERO";
    case STOP_DISABLE: return "STOP_DISABLE";
    default: return "UNKNOWN";
  }
}
static const char *eventName(uint8_t k) {
  switch (k) {
    case EV_CAN_ERROR: return "CAN_ERROR";
    case EV_CAN_CLEAR: return "CAN_ERROR_BITS_CLEAR";
    case EV_RX_GAP_BEGIN: return "RX_GAP_BEGIN";
    case EV_RX_GAP_END: return "RX_GAP_END";
    case EV_RX_REJECT: return "RX_REJECT";
    case EV_RX_HW_OVERRUN: return "RX_HW_OVERRUN";
    case EV_TX_BACKLOG: return "TX_BACKLOG";
    case EV_TX_FAILURE: return "TX_FAILURE";
    case EV_FAULT: return "FAULT";
    case EV_START: return "START";
    case EV_STOP: return "STOP";
    case EV_CAN_IRQ_SNAPSHOT: return "CAN_IRQ_ERROR_SNAPSHOT";
    default: return "UNKNOWN";
  }
}
static const char *canStateName(uint32_t esr) {
  switch (FLEXCAN_ESR_get_fault_code(esr)) {
    case CAN_ERROR_ACTIVE: return "ERROR_ACTIVE";
    case CAN_ERROR_PASSIVE: return "ERROR_PASSIVE";
    case CAN_ERROR_BUS_OFF: return "BUS_OFF";
    default: return "UNKNOWN";
  }
}
static void printErrorNames(uint32_t esr) {
  bool any = false;
#define PRINT_ERR(mask, name) if (esr & (mask)) { if (any) Serial.print('|'); Serial.print(name); any = true; }
  PRINT_ERR(FLEXCAN_ESR_ACK_ERR, "ACK");
  PRINT_ERR(FLEXCAN_ESR_BIT0_ERR, "BIT0");
  PRINT_ERR(FLEXCAN_ESR_BIT1_ERR, "BIT1");
  PRINT_ERR(FLEXCAN_ESR_STF_ERR, "STUFF");
  PRINT_ERR(FLEXCAN_ESR_FRM_ERR, "FORM");
  PRINT_ERR(FLEXCAN_ESR_CRC_ERR, "CRC");
#undef PRINT_ERR
  if (!any) Serial.print('-');
}
static void eventPrefix(uint32_t timeUs, uint8_t motor) {
  Serial.print('[');
  Serial.print((timeUs - sessionStartUs) / 1000000.0f, 3);
  Serial.print("s] M"); Serial.print(motor + 1); Serial.print(' ');
}
static void printEvent(const Event &ev) {
  eventPrefix(ev.us, ev.motor);
  Serial.print(eventName(ev.kind));
  if (ev.kind == EV_RX_GAP_BEGIN) {
    Serial.print(" RX_AGE_ms="); Serial.print(ev.value / 1000.0f, 3);
  } else if (ev.kind == EV_RX_GAP_END) {
    Serial.print(" WARNING_WINDOW_ms="); Serial.print(ev.value / 1000.0f, 3);
  } else if (ev.kind == EV_FAULT) {
    Serial.print(" REASON="); Serial.print(faultName(static_cast<FaultKind>(ev.value)));
  } else if (ev.kind == EV_TX_FAILURE) {
    Serial.print(" RESULT="); Serial.print(static_cast<int32_t>(ev.value));
  } else if (ev.kind == EV_TX_BACKLOG || ev.kind == EV_RX_REJECT ||
             ev.kind == EV_RX_HW_OVERRUN) {
    Serial.print(" COUNT="); Serial.print(ev.value);
  }
  Serial.print(" TEC/REC="); Serial.print(ev.ecr & 255U);
  Serial.print('/'); Serial.print((ev.ecr >> 8) & 255U);
  Serial.print(" ESR1=0x"); Serial.print(ev.esr, HEX);
  Serial.print(" STATE="); Serial.print(canStateName(ev.esr));
  Serial.print(" ERR="); printErrorNames(ev.esr);
  Serial.println();
}

static void printNewEvents(uint8_t budget = 8) {
  while (budget--) {
    Event ev{};
    noInterrupts();
    const uint32_t end = eventWriteSeq;
    if (eventPrintSeq == end) { interrupts(); break; }
    if (end - eventPrintSeq > EVENT_SIZE) {
      eventLostForPrint += end - eventPrintSeq - EVENT_SIZE;
      eventPrintSeq = end - EVENT_SIZE;
    }
    ev = events[eventPrintSeq % EVENT_SIZE];
    ++eventPrintSeq;
    interrupts();
    printEvent(ev);
  }
}

// Read FlexCAN_T4's queued hardware error snapshots in loop(). This is
// diagnostic-queue consumption only: no CAN RX/TX is performed from loop().
// The queue preserves some short-lived ESR1 errors missed by 500-us polling.
static void drainCanErrorSnapshots() {
  if (state != RUNNING) return;
  for (uint8_t i = 0; i < 2 && state == RUNNING; ++i) if (motorOn(i)) {
    for (uint8_t n = 0; n < 4 && state == RUNNING; ++n) {
      CAN_error_t err;
      const bool available = (i == 0) ? can1.error(err, false) : can2.error(err, false);
      if (!available) break;
      const uint32_t now = micros();
      const uint32_t signature = (err.ESR1 & CAN_ERROR_MASK) |
        (uint32_t(FLEXCAN_ESR_get_fault_code(err.ESR1)) << 24);
      noInterrupts();
      MotorStats &d = motors[i];
      ++d.canIrqSnapshots;
      d.esrSeen |= err.ESR1;
      const bool important = signature != 0 &&
        (signature != d.lastIrqSignature || now - d.lastIrqLogUs >= 10000);
      if (important) {
        pushEvent(EV_CAN_IRQ_SNAPSHOT, i, now, err.ESR1, err.ECR, signature);
        d.lastIrqLogUs = now;
        ++d.canErrorEvents;
      }
      d.lastIrqSignature = signature;
      const uint8_t tec = err.ECR & 255U, rec = (err.ECR >> 8) & 255U;
      if (tec > d.maxTec) d.maxTec = tec;
      if (rec > d.maxRec) d.maxRec = rec;
      const bool busOff = FLEXCAN_ESR_get_fault_code(err.ESR1) == CAN_ERROR_BUS_OFF;
      if (!firstCanError.valid && (signature != 0 || busOff) && state == RUNNING) {
        firstCanError.valid = true;
        firstCanError.detectedUs = now;
        firstCanError.errorEsr = err.ESR1;
        firstCanError.errorEcr = err.ECR;
        firstCanError.currentEsr = esrValue(i);
        firstCanError.currentEcr = ecrValue(i);
        firstCanError.isrMaxUs = maxIsrUs;
        firstCanError.jitterMaxUs = maxJitterUs;
        for (uint8_t m = 0; m < 2; ++m) {
          if (!motorOn(m)) continue;
          const MotorStats &md = motors[m];
          firstCanError.tx[m] = md.txAccepted[PERIODIC_ZERO];
          firstCanError.txFail[m] = md.txFailures[PERIODIC_ZERO];
          firstCanError.txQueue[m] = txQueue(m);
          firstCanError.rxRaw[m] = md.rxRaw;
          firstCanError.rxValid[m] = md.rxValid;
          firstCanError.rxAgeUs[m] = md.sawRx ? now - md.lastRxUs : UINT32_MAX;
          firstCanError.rxGapMaxUs[m] = md.maxRxGapUs;
          firstCanError.rxHwOverrun[m] = md.rxHwOverrun;
        }
        // Latch the diagnosis only after the event and immediate counters are
        // copied. state=FAULT_STOPPED prevents further periodic TX in commandISR.
        d.esr1 = firstCanError.currentEsr;
        d.ecr = firstCanError.currentEcr;
        d.txQueueNow = firstCanError.txQueue[i];
        latchFault(FAULT_CAN_ERROR, i, now);
      }
      if (FLEXCAN_ESR_get_fault_code(err.ESR1) == CAN_ERROR_BUS_OFF &&
          state == RUNNING) {
        d.esr1 = err.ESR1; d.ecr = err.ECR;
        latchFault(FAULT_BUS_OFF, i, now);
      }
      interrupts();
    }
  }
}

// Discard diagnostic error snapshots accumulated before this test.
// Does not clear the underlying hardware ESR1 register.
static void discardOldCanErrors() {
  for (uint8_t i = 0; i < 2; ++i) if (motorOn(i)) {
    for (uint8_t k = 0; k < 64; ++k) {
      CAN_error_t stale;
      if (!(i == 0 ? can1.error(stale, false) : can2.error(stale, false))) break;
    }
  }
}

static void printStatus() {
  MotorStats snap[2];
  uint32_t snapTicks, snapMaxIsr, snapOverruns, snapGap, snapJitter, snapLate;
  uint32_t snapRxPollTicks, snapRxPollMaxGap, snapRxPollOverruns;
  RunState snapState;
  noInterrupts();
  snap[0] = motors[0]; snap[1] = motors[1];
  snapTicks = ticks; snapMaxIsr = maxIsrUs; snapOverruns = isrOverruns;
  snapGap = maxTickGapUs; snapJitter = maxJitterUs; snapLate = lateTicks;
  snapRxPollTicks = rxPollTicks; snapRxPollMaxGap = maxRxPollGapUs; snapRxPollOverruns = rxPollOverruns;
  snapState = state;
  interrupts();
  const uint32_t now = micros();
  Serial.print("STATE,"); Serial.print(stateName(snapState));
  Serial.print(",PERIOD_US,"); Serial.print(COMMAND_PERIOD_US);
  Serial.print(",RX_POLL_US,"); Serial.print(RX_POLL_PERIOD_US);
  Serial.print(",RX_POLL_TICKS,"); Serial.print(snapRxPollTicks);
  Serial.print(",RX_POLL_GAP_MAX_US,"); Serial.print(snapRxPollMaxGap);
  Serial.print(",RX_POLL_OVERRUNS,"); Serial.print(snapRxPollOverruns);
  Serial.print(",TICKS,"); Serial.print(snapTicks);
  Serial.print(",ISR_MAX_US,"); Serial.print(snapMaxIsr);
  Serial.print(",OVERRUN,"); Serial.print(snapOverruns);
  Serial.print(",TICK_GAP_MAX_US,"); Serial.print(snapGap);
  Serial.print(",JITTER_MAX_US,"); Serial.print(snapJitter);
  Serial.print(",JITTER_GT_50US,"); Serial.print(snapLate);
  Serial.print(",TX_API_PHASE_LAST_US,"); Serial.print(txApiPhaseLastUs);
  Serial.print(",TX_API_PHASE_MAX_ERR_US,"); Serial.print(txApiPhaseMaxErrorUs);
  Serial.print(",TX_API_PHASE_SAMPLES,"); Serial.print(txApiPhaseSamples);
  Serial.print(",EVENT_PRINT_LOST,"); Serial.println(eventLostForPrint);
  Serial.print("PRESTART,IDLE_RX_DRAIN,"); Serial.print(IDLE_RX_DRAIN ? 1 : 0);
  Serial.print(",FIRST_S_PENDING,"); Serial.print(firstStartSinceBoot ? 1 : 0);
  Serial.print(",CAN_REINIT_COUNT,"); Serial.print(reinitCount);
  Serial.print(",IDLE_TIMER_TICKS,"); Serial.print(idleTimerTicks);
  Serial.print(",IDLE_ISR_MAX_US,"); Serial.println(idleMaxIsrUs);
  for (uint8_t i=0; i<2; ++i) if (motorOn(i)) {
    Serial.print("PRESTART_M"); Serial.print(i+1);
    Serial.print(",IDLE_RX_RAW,"); Serial.print(idleRxRaw[i]);
    Serial.print(",IDLE_RX_OVERRUN,"); Serial.print(idleRxHwOverrun[i]);
    Serial.print(",IDLE_LAST_ID,0x"); Serial.print(idleLastId[i], HEX);
    Serial.print(",IDLE_LAST_RX_US,"); Serial.println(idleLastRxUs[i]);
  }
  for (uint8_t i = 0; i < 2; ++i) if (motorOn(i)) {
    const MotorStats &d = snap[i];
    Serial.print("M"); Serial.print(i + 1);
    Serial.print(" TX:"); Serial.print(d.txAccepted[PERIODIC_ZERO]);
    Serial.print(" RX:"); Serial.print(d.rxValid);
    Serial.print(" RX_RAW:"); Serial.print(d.rxRaw);
    Serial.print(" TX_FAIL:"); Serial.print(d.txFailures[PERIODIC_ZERO]);
    Serial.print(" TX_QUEUED:"); Serial.print(d.txQueued[PERIODIC_ZERO]);
    Serial.print(" TXQ:"); Serial.print(d.txQueueNow);
    Serial.print(" TXQ_MAX:"); Serial.print(d.txQueueMax);
    Serial.print(" TX_API_PERIOD_MAX_ERR_US:"); Serial.print(txApiMaxPeriodErrorUs[i]);
    Serial.print(" AGE_ms:");
    if (d.sawRx) Serial.print((now - d.lastRxUs) / 1000.0f, 2);
    else Serial.print("NEVER");
    Serial.print(" RX_GAP_MAX_us:"); Serial.print(d.maxRxGapUs);
    Serial.print(" RX_REJECT:"); Serial.print(d.rxRejected);
    Serial.print(" RX_HW_OVERRUN:"); Serial.print(d.rxHwOverrun);
    Serial.print(" RX_NONCANONICAL:"); Serial.print(d.rxNoncanonical);
    Serial.print(" TEC/REC:"); Serial.print(d.ecr & 255U);
    Serial.print('/'); Serial.print((d.ecr >> 8) & 255U);
    Serial.print(" MAX:"); Serial.print(d.maxTec);
    Serial.print('/'); Serial.print(d.maxRec);
    Serial.print(" MOTOR_ERROR:"); Serial.print(d.motorError);
    Serial.print(" ESR1:0x"); Serial.print(d.esr1, HEX);
    Serial.print(" ERROR_SEEN:0x"); Serial.print(d.esrSeen, HEX);
    Serial.print(" CAN_EVENTS:"); Serial.print(d.canErrorEvents);
    Serial.print(" CAN_IRQ_SNAPSHOTS:"); Serial.println(d.canIrqSnapshots);
    Serial.print("  PHYSICAL M"); Serial.print(i + 1);
    Serial.print(" P_rad:"); Serial.print(d.positionRad, 6);
    Serial.print(" V_rad_s:"); Serial.print(d.velocityRadS, 6);
    Serial.print(" T_Nm:"); Serial.println(d.torqueNm, 6);
    Serial.print("  START_FAIL(enable/zero):");
    Serial.print(d.txFailures[START_ENABLE]); Serial.print('/');
    Serial.print(d.txFailures[START_ZERO]);
    Serial.print(" STOP_FAIL(zero/disable):");
    Serial.print(d.txFailures[STOP_ZERO]); Serial.print('/');
    Serial.print(d.txFailures[STOP_DISABLE]);
    Serial.print(" REJECT(id/payload/dlc/format):");
    Serial.print(d.rejectId); Serial.print('/');
    Serial.print(d.rejectPayload); Serial.print('/');
    Serial.print(d.rejectDlc); Serial.print('/');
    Serial.println(d.rejectFormat);
    if (d.hasBadFrame) {
      Serial.print("  FIRST_REJECT M"); Serial.print(i+1);
      Serial.print(" ID=0x"); Serial.print(d.firstBadId, HEX);
      Serial.print(" BUF0=0x"); Serial.print(d.firstBadPayloadId, HEX);
      Serial.print(" DLC="); Serial.print(d.firstBadLen);
      Serial.print(" FLAGS(ext/rtr/ovr)="); Serial.println(d.firstBadFlags);
    }
  }
}

static void printHistory() {
  // One-motor snapshot buffer ensures printing cannot corrupt the history being read.
  static HistoryRow rows[HISTORY_SIZE];
  for (uint8_t i = 0; i < 2; ++i) if (motorOn(i)) {
    uint16_t n, start;
    bool frozen;
    noInterrupts();
    n = histories[i].count;
    start = (histories[i].head + HISTORY_SIZE - n) % HISTORY_SIZE;
    frozen = histories[i].frozen;
    interrupts();
    for (uint16_t k = 0; k < n; ++k) {
      noInterrupts();
      rows[k] = histories[i].data[(start + k) % HISTORY_SIZE];
      interrupts();
    }
    Serial.print("HISTORY,M"); Serial.print(i+1);
    Serial.print(",COUNT,"); Serial.print(n);
    Serial.print(",FROZEN,"); Serial.println(frozen ? 1 : 0);
    Serial.println("HIST,t_ms,motor,fault,tx,tx_fail,rx,rx_reject,rx_hw_overrun,rx_age_us,TEC,REC,TXQ,ESR1,isr_max_us,jitter_max_us,p_rad,v_rad_s,t_Nm");
    for (uint16_t k = 0; k < n; ++k) {
      const HistoryRow &h = rows[k];
      Serial.print("HIST,"); Serial.print((h.us-sessionStartUs)/1000.0f, 3);
      Serial.print(','); Serial.print(i+1);
      Serial.print(','); Serial.print(h.faultRow ? 1 : 0);
      Serial.print(','); Serial.print(h.tx);
      Serial.print(','); Serial.print(h.txFail);
      Serial.print(','); Serial.print(h.rx);
      Serial.print(','); Serial.print(h.reject);
      Serial.print(','); Serial.print(h.rxHwOverrun);
      Serial.print(','); Serial.print(h.rxAgeUs);
      Serial.print(','); Serial.print(h.tec);
      Serial.print(','); Serial.print(h.rec);
      Serial.print(','); Serial.print(h.txQueue);
      Serial.print(",0x"); Serial.print(h.esr1, HEX);
      Serial.print(','); Serial.print(h.maxIsrUs);
      Serial.print(','); Serial.print(h.maxJitterUs);
      Serial.print(','); Serial.print(h.p, 6);
      Serial.print(','); Serial.print(h.v, 6);
      Serial.print(','); Serial.println(h.t, 6);
    }
  }
}

static void printEvents() {
  uint32_t end;
  noInterrupts(); end = eventWriteSeq; interrupts();
  const uint32_t start = end > EVENT_SIZE ? end - EVENT_SIZE : 0;
  Serial.print("EVENTS,count="); Serial.println(end - start);
  for (uint32_t seq = start; seq < end; ++seq) {
    Event ev{};
    noInterrupts(); ev = events[seq % EVENT_SIZE]; interrupts();
    printEvent(ev);
  }
}

static void printFaultReport() {
  if (!fault.valid) return;
  FaultSnapshot f;
  TxFailure tx;
  noInterrupts(); f = fault; tx = firstTxFailure; interrupts();
  eventPrefix(f.us, f.motor);
  Serial.print("ERROR "); Serial.println(faultName(f.kind));
  Serial.print("  CAN="); Serial.print(f.motor+1);
  Serial.print(" MOTOR_ID="); Serial.println(MOTOR_ID[f.motor]);
  Serial.print("  ESR1=0x"); Serial.print(f.esr, HEX);
  Serial.print(" TEC/REC="); Serial.print(f.ecr & 255U);
  Serial.print('/'); Serial.println((f.ecr>>8) & 255U);
  Serial.print("  CAN_STATE="); Serial.print(canStateName(f.esr));
  Serial.print(" ERR="); printErrorNames(f.esr); Serial.println();
  Serial.print("  RX_AGE_US="); Serial.print(f.rxAge);
  Serial.print(" RX_RAW/VALID="); Serial.print(f.rxRaw);
  Serial.print('/'); Serial.println(f.rxValid);
  Serial.print("  TX_ACCEPTED/FAIL="); Serial.print(f.tx);
  Serial.print('/'); Serial.print(f.txFail);
  Serial.print(" TX_QUEUE="); Serial.println(f.txQueue);
  Serial.print("  MOTOR_ERROR="); Serial.println(f.motorError);
  Serial.print("  ISR_MAX_US="); Serial.print(f.maxIsr);
  Serial.print(" JITTER_MAX_US="); Serial.println(f.maxJitter);
  if (f.kind == FAULT_CAN_ERROR && firstCanError.valid) {
    const CanErrorSnapshot &s = firstCanError;
    Serial.print("  CAN_ERROR_DETECTED_US="); Serial.print(s.detectedUs);
    Serial.println(" (driver dequeue time; actual wire error time unavailable)");
    Serial.print("  CAN_ERROR_TYPE="); printErrorNames(s.errorEsr); Serial.println();
    Serial.print("  CAN_ERROR_SNAPSHOT_ESR1=0x"); Serial.print(s.errorEsr, HEX);
    Serial.print(" ECR=0x"); Serial.println(s.errorEcr, HEX);
    Serial.print("  CAN_ERROR_CURRENT_ESR1=0x"); Serial.print(s.currentEsr, HEX);
    Serial.print(" ECR=0x"); Serial.println(s.currentEcr, HEX);
    for (uint8_t m = 0; m < 2; ++m) if (motorOn(m)) {
      Serial.print("  FIRST_CAN_ERROR_M"); Serial.print(m + 1);
      Serial.print(" TX="); Serial.print(s.tx[m]);
      Serial.print(" TX_FAIL="); Serial.print(s.txFail[m]);
      Serial.print(" TXQ="); Serial.print(s.txQueue[m]);
      Serial.print(" RX_RAW="); Serial.print(s.rxRaw[m]);
      Serial.print(" RX_VALID="); Serial.print(s.rxValid[m]);
      Serial.print(" RX_AGE_US="); Serial.print(s.rxAgeUs[m]);
      Serial.print(" RX_GAP_MAX_US="); Serial.print(s.rxGapMaxUs[m]);
      Serial.print(" RX_HW_OVERRUN="); Serial.println(s.rxHwOverrun[m]);
    }
    Serial.print("  FIRST_CAN_ERROR_ISR_MAX_US="); Serial.print(s.isrMaxUs);
    Serial.print(" JITTER_MAX_US="); Serial.println(s.jitterMaxUs);
  }
  if (tx.valid) {
    Serial.print("  FIRST_TX_FAILURE: t_ms=");
    Serial.print((tx.us-sessionStartUs)/1000.0f, 3);
    Serial.print(" M"); Serial.print(tx.motor+1);
    Serial.print(" KIND="); Serial.print(txKindName(tx.kind));
    Serial.print(" RETURN="); Serial.print(tx.result);
    Serial.print(" TXQ="); Serial.print(tx.queueBefore);
    Serial.print("->"); Serial.print(tx.queueAfter);
    Serial.print(" RX_AGE_US="); Serial.println(tx.rxAge);
  }
  Serial.println("  >>> SAFETY STOP: periodic command disabled; stop CAN frames best effort");
}

// Called from loop only after the periodic timer is stopped.
static void sendStopRequests() {
  bool zeroOK[2] = {false, false};
  bool exitOK[2] = {false, false};
  // Send to BOTH motors before printing: Serial must never delay the peer stop.
  for (uint8_t i = 0; i < 2; ++i) if (motorOn(i))
    zeroOK[i] = sendZeroTorque(i, STOP_ZERO);
  for (uint8_t i = 0; i < 2; ++i) if (motorOn(i))
    exitOK[i] = sendSpecial(i, 0xFD, STOP_DISABLE);
  for (uint8_t i = 0; i < 2; ++i) if (motorOn(i)) {
    Serial.print("M"); Serial.print(i+1);
    Serial.print(" STOP_ZERO_TX_ACCEPTED="); Serial.print(zeroOK[i] ? 1 : 0);
    Serial.print(" STOP_DISABLE_TX_ACCEPTED="); Serial.println(exitOK[i] ? 1 : 0);
  }
}

static void stopTimer() {
  if (timerActive) { commandTimer.end(); timerActive = false; }
}

// The same timer is kept active in IDLE; it *only* drains RX there.
// The caller must set state to IDLE or FAULT_STOPPED before invoking.
static bool startIdleTimer() {
  if (timerActive) return true;
  commandTimer.priority(TIMER_ISR_PRIORITY);
  timerActive = commandTimer.begin(commandISR, RX_POLL_PERIOD_US);
  return timerActive;
}

// Reinitialize ONLY at the beginning of the first S after upload/reset,
// while the timer is stopped. This reproduces the successful case in which
// the motor's 48-V power was already on when Teensy firmware started.
// Later S commands preserve the known-good Jia configuration.
static void reinitSelectedCan() {
  if (MOTOR1_ON) {
    can1.begin();
    can1.setBaudRate(CAN_BAUD);
    NVIC_SET_PRIORITY(IRQ_CAN1, CAN_IRQ_PRIORITY);
  }
  if (MOTOR2_ON) {
    can2.begin();
    can2.setBaudRate(CAN_BAUD);
    NVIC_SET_PRIORITY(IRQ_CAN2, CAN_IRQ_PRIORITY);
  }
  ++reinitCount;
}

static void serviceFault() {
  if (!faultPending || faultOutputDone) return;
  stopTimer();
  faultPending = false;
  // Attempt the stop frames BEFORE any potentially blocking Serial output.
  sendStopRequests();
  printFaultReport();
  printNewEvents(32);
  faultOutputDone = true;
  // Resume RX-drain mode after the stop frames have been sent.
  if (!startIdleTimer()) Serial.println("ERROR,IDLE_RX_TIMER_START_FAILED");
}

static void resetSession() {
  stopTimer();
  state = IDLE; faultPending = false; faultOutputDone = false;
  motors[0] = MotorStats{}; motors[1] = MotorStats{};
  histories[0] = HistoryBuffer{}; histories[1] = HistoryBuffer{};
  fault = FaultSnapshot{}; firstTxFailure = TxFailure{};
  firstCanError = CanErrorSnapshot{};
  eventWriteSeq = 0; eventPrintSeq = 0; eventLostForPrint = 0;
  ticks = 0; maxIsrUs = 0; isrOverruns = 0;
  rxPollTicks = 0; maxRxPollUs = 0; rxPollOverruns = 0;
  maxRxPollGapUs = 0; phaseSlot = 0; lastRxPollUs = 0;
  lastTxApiUs[0] = lastTxApiUs[1] = 0;
  txApiMaxPeriodErrorUs[0] = txApiMaxPeriodErrorUs[1] = 0;
  txApiPhaseLastUs = txApiPhaseMaxErrorUs = txApiPhaseSamples = 0;
  maxTickGapUs = 0; maxJitterUs = 0; lateTicks = 0;
  lastTickUs = 0;
  prevTx[0] = prevTx[1] = prevRx[0] = prevRx[1] = 0;
  sessionStartUs = micros();
  for (uint8_t i = 0; i < 2; ++i) motors[i].historyStampUs = sessionStartUs;
}

static void stopManually() {
  if (state == IDLE) return;
  const RunState old = state;
  state = IDLE;
  stopTimer();
  for (uint8_t i = 0; i < 2; ++i) histories[i].frozen = true;
  sendStopRequests();
  pushEvent(EV_STOP, MOTOR1_ON ? 0 : 1, micros(), 0, 0);
  Serial.println(old == RUNNING ? "TEST STOPPED" : "FAULT STOP REQUEST RETRIED");
  if (!startIdleTimer()) Serial.println("ERROR,IDLE_RX_TIMER_START_FAILED");
}

static void beginSession() {
  if (state == RUNNING) { Serial.println("ALREADY_RUNNING"); return; }
  if (!faultOutputDone && state == FAULT_STOPPED) serviceFault();

  // Stop the idle poller before touching the CAN controller or stats.
  stopTimer();
  for (uint8_t i = 0; i < 2; ++i) if (motorOn(i)) {
    const uint32_t ecr = ecrValue(i), esr = esrValue(i);
    Serial.print("PRESTART_SNAPSHOT,M"); Serial.print(i+1);
    Serial.print(",IDLE_RX_RAW,"); Serial.print(idleRxRaw[i]);
    Serial.print(",IDLE_RX_HW_OVERRUN,"); Serial.print(idleRxHwOverrun[i]);
    Serial.print(",TEC/REC,"); Serial.print(ecr & 255U);
    Serial.print('/'); Serial.print((ecr >> 8) & 255U);
    Serial.print(",ESR1,0x"); Serial.println(esr, HEX);
  }
  discardOldCanErrors();
  resetSession();
  if (firstStartSinceBoot && REINIT_CAN_ON_FIRST_S) {
    reinitSelectedCan();
    Serial.println("PRESTART,CAN_REINITIALIZED_ON_FIRST_S,1");
  }
  firstStartSinceBoot = false;
  discardOldCanErrors();
  // Drain any mailbox frames present after CAN initialization.
  if (MOTOR1_ON) readIdleMotor(0);
  if (MOTOR2_ON) readIdleMotor(1);
  bool ok = true;
  // Match the working Jia sketch: send ENABLE only now, not ZERO immediately.
  // First M1 zero at +500 us, first M2 zero at +750 us; then 2 kHz each.
  for (uint8_t i = 0; i < 2; ++i) if (motorOn(i)) {
    // Separate the two MIT_ENABLE API calls as well. Do not delay stops.
    if (i == 1 && MOTOR1_ON && MOTOR2_ON) delayMicroseconds(TX_M2_PHASE_US);
    if (!sendSpecial(i, 0xFC, START_ENABLE)) ok = false;
  }
  if (!ok) {
    Serial.println("START_ERROR: TX API rejected enable or zero frame");
    state = FAULT_STOPPED;
    sendStopRequests();
    faultOutputDone = true;
    if (!startIdleTimer()) Serial.println("ERROR,IDLE_RX_TIMER_START_FAILED");
    return;
  }
  // Startup event must be written before ISR starts using the same ring.
  pushEvent(EV_START, MOTOR1_ON ? 0 : 1, micros(), 0, 0);
  state = RUNNING;
  lastPrintMs = millis();
  commandTimer.priority(TIMER_ISR_PRIORITY);
  if (!commandTimer.begin(commandISR, TIMER_BASE_PERIOD_US)) {
    state = FAULT_STOPPED;
    Serial.println("START_ERROR: IntervalTimer.begin() failed");
    sendStopRequests();
    faultOutputDone = true;
    if (!startIdleTimer()) Serial.println("ERROR,IDLE_RX_TIMER_START_FAILED");
    return;
  }
  timerActive = true;
  Serial.print("TEST STARTED,PERIOD_US,"); Serial.print(COMMAND_PERIOD_US);
  Serial.print(",RX_POLL_US,"); Serial.print(RX_POLL_PERIOD_US);
  Serial.print(",TIMER_BASE_US,"); Serial.print(TIMER_BASE_PERIOD_US);
  Serial.print(",M2_PHASE_US,"); Serial.println(TX_M2_PHASE_US);
}

static void handleCommand(char *line) {
  for (char *p = line; *p; ++p) *p = char(toupper((unsigned char)*p));
  if (!strcmp(line, "S")) beginSession();
  else if (!strcmp(line, "X")) { serviceFault(); stopManually(); }
  else if (!strcmp(line, "STATUS")) printStatus();
  else if (!strcmp(line, "HISTORY")) {
    if (state == RUNNING) Serial.println("HISTORY available after X/fault only (avoid blocking stop commands)");
    else printHistory();
  }
  else if (!strcmp(line, "EVENTS")) {
    if (state == RUNNING) Serial.println("EVENTS dump available after X/fault only; live events print automatically");
    else printEvents();
  }
  else if (!strcmp(line, "RESET")) {
    if (state == RUNNING) Serial.println("RESET only allowed when stopped");
    else {
      resetSession();
      Serial.println("STATISTICS RESET");
      if (!startIdleTimer()) Serial.println("ERROR,IDLE_RX_TIMER_START_FAILED");
    }
  } else if (!strcmp(line, "HELP"))
    Serial.println("COMMANDS: S=start, X=stop, STATUS, HISTORY, EVENTS, RESET, HELP");
  else Serial.println("Unknown command. Type HELP");
}

static void readSerial() {
  static char line[32];
  static uint8_t used = 0;
  while (Serial.available()) {
    const char c = char(Serial.read());
    if (c == '\r' || c == '\n') {
      if (used) { line[used] = 0; handleCommand(line); used = 0; }
    } else if (used < sizeof(line)-1) line[used++] = c;
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  resetSession();
  if (MOTOR1_ON) {
    can1.begin();
    can1.setBaudRate(CAN_BAUD);
    NVIC_SET_PRIORITY(IRQ_CAN1, CAN_IRQ_PRIORITY);
  }
  if (MOTOR2_ON) {
    can2.begin();
    can2.setBaudRate(CAN_BAUD);
    NVIC_SET_PRIORITY(IRQ_CAN2, CAN_IRQ_PRIORITY);
  }
  // Keep the default FlexCAN_T4 polling-mailbox configuration.
  // Do NOT install an RX callback or enable an RX mailbox interrupt.
  Serial.println("READY,CAN_debug_AK80_Jia_TimerISR_v5_M1_ONLY_100US_ISR_FIRST_ERROR_AI");
  Serial.print("M1=CAN1/ID1/"); Serial.print(MOTOR1_ON ? "ON" : "OFF");
  Serial.print(",M2=CAN2/ID2/"); Serial.print(MOTOR2_ON ? "ON" : "OFF");
  Serial.print(",CAN_BAUD,"); Serial.print(CAN_BAUD);
  Serial.print(",TX_PERIOD_US,"); Serial.print(COMMAND_PERIOD_US);
  Serial.print(",RX_POLL_PERIOD_US,"); Serial.print(RX_POLL_PERIOD_US);
  Serial.print(",TIMER_BASE_US,"); Serial.print(TIMER_BASE_PERIOD_US);
  Serial.print(",TX_M2_PHASE_US,"); Serial.println(TX_M2_PHASE_US);
  Serial.println("Jia Mode1 dual CAN: RUNNING ISR 50us, RX 100us, TX M1=500us/M2=500us+250us offset");
  Serial.println("NOTE: offset is for CAN write API calls; actual wire timings can differ if TX queues/retransmits");
  Serial.println("P range -12.5..12.5 rad; V range -37.5..37.5 rad/s; T range -32..32 Nm");
  Serial.println("COMMANDS: S, X, STATUS, HISTORY, EVENTS, RESET, HELP");
  Serial.println("WARNING: RX HW overrun is logged, not fatal in zero-torque diagnosis; timeout/bus-off still stop.");
  Serial.println("WARNING: CAN errors may persist as latched ESR1 bits; RX gap window is separately measured.");
  Serial.println("WARNING: CAN stop is best effort; physical emergency stop required.");
  Serial.println("DUAL_TEST: any motor fault stops periodic TX for BOTH motors.");
  Serial.print("PRESTART,IDLE_RX_DRAIN,"); Serial.print(IDLE_RX_DRAIN ? 1 : 0);
  Serial.print(",REINIT_ON_FIRST_S,"); Serial.println(REINIT_CAN_ON_FIRST_S ? 1 : 0);
  if (!startIdleTimer()) Serial.println("ERROR,IDLE_RX_TIMER_START_FAILED");
}

void loop() {
  // All hardware CAN RX, periodic TX and periodic diagnosis live in commandISR().
  // Only slow/non-real-time serial logging and stop/start operations live here.
  serviceFault();
  readSerial();
  drainCanErrorSnapshots();
  printNewEvents(8);
  if (state == RUNNING && millis() - lastPrintMs >= PRINT_INTERVAL_MS) {
    lastPrintMs = millis();
    MotorStats d[2];
    noInterrupts(); d[0] = motors[0]; d[1] = motors[1]; interrupts();
    Serial.print("[ "); Serial.print((micros()-sessionStartUs)/1000000.0f, 1);
    Serial.print("s] PERIOD SUMMARY ISR_MAX_US="); Serial.print(maxIsrUs);
    Serial.print(" RX_POLL_OVERRUNS="); Serial.print(rxPollOverruns);
    Serial.print(" JITTER_MAX_US="); Serial.print(maxJitterUs);
    Serial.print(" TX_API_PHASE_LAST_US="); Serial.print(txApiPhaseLastUs);
    Serial.print(" TX_API_PHASE_MAX_ERR_US="); Serial.println(txApiPhaseMaxErrorUs);
    for (uint8_t i = 0; i < 2; ++i) if (motorOn(i)) {
      const uint32_t tx = d[i].txAccepted[PERIODIC_ZERO];
      const uint32_t rx = d[i].rxValid;
      Serial.print(" M"); Serial.print(i+1);
      Serial.print(" TX="); Serial.print(tx-prevTx[i]);
      Serial.print(" RX="); Serial.print(rx-prevRx[i]);
      Serial.print(" TX_FAIL="); Serial.print(d[i].txFailures[PERIODIC_ZERO]);
      Serial.print(" RX_GAP_MAX_US="); Serial.print(d[i].maxRxGapUs);
      Serial.print(" TXQ_MAX="); Serial.print(d[i].txQueueMax);
      Serial.print(" TX_API_PERIOD_MAX_ERR_US="); Serial.print(txApiMaxPeriodErrorUs[i]);
      Serial.print(" TEC/REC="); Serial.print(d[i].ecr & 255U);
      Serial.print('/'); Serial.println((d[i].ecr >> 8) & 255U);
      prevTx[i] = tx; prevRx[i] = rx;
    }
  }
  serviceFault();
}
