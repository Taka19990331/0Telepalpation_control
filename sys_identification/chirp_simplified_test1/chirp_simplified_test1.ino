#include <Arduino.h>
#include <util/atomic.h>
#include <IntervalTimer.h>
#include <SdFat.h>
#include <FlexCAN_T4.h>
#include "chirp_config.h"
#include "motor_protocol.h"
#include "reference_average.h"

#if !defined(ARDUINO_TEENSY41)
#error "Select Teensy 4.1 with built-in SD socket"
#endif

constexpr CAN_DEV_TABLE MOTOR_CAN_BUS = IS_60 ? CAN3 : (IS_80A ? CAN1 : CAN2);
FlexCAN_T4<MOTOR_CAN_BUS, RX_SIZE_256, TX_SIZE_16> MotorCan;
constexpr FLEXCAN_MAILBOX TX_MAILBOX = MB8;
constexpr uint32_t TX_BIT = 1u << 8;
struct Feedback {
  float positionRad, speedRadS, torqueEstimateNm;
  uint32_t rxUs, sequence, error, raw;
};
Feedback feedback = {};
volatile uint32_t rxFault = COMPLETE;
volatile RunState runState = IDLE;
volatile uint32_t head = 0, tail = 0, status = COMPLETE, stopFlags = 0;
volatile bool startRequested = false, userStopRequested = false, ioStopRequested = false;
bool recording = false, cardReady = false, timerReady = false, canReady = false;
bool ioFailed = false;
volatile bool restartLocked = false;
uint32_t firstUs = 0, previousTickUs = 0, waitStartedUs = 0, stopStartedUs = 0;
uint32_t highWater = 0, maxWriteUs = 0, stopStage = 0;
ReferenceAverage referenceAverage;
uint32_t averageRows = 0, sweepStartIndex = 0;
bool controlling() { return runState == AVERAGING || runState == RUNNING; }
bool entered = false, zeroSent = false, zeroBeforeEnter = false, abortPending = false;
float holdDeg = NAN;
DMAMEM Sample ring[RING_SIZE];
SdFs sd;
FsFile logFile;
IntervalTimer controlTimer;
Header runHeader;
char filename[16], lastSaved[16] = {};

// Preserve interrupt state in foreground critical sections. CAN RX and timer have
// equal priority and cannot preempt each other: feedback copies inside the timer
// are coherent. Do not raise CAN priority without adding snapshot protection.
uint32_t lockInterrupts() { uint32_t p = __get_primask(); __disable_irq(); return p; }
void unlockInterrupts(uint32_t p) { if (!p) __enable_irq(); }

bool txIdle() {
  return FLEXCAN_get_code(FLEXCANb_MBn_CS(MOTOR_CAN_BUS, 8)) == FLEXCAN_MB_CODE_TX_INACTIVE;
}
bool sendFrame(const CAN_message_t &frame) {
  if (NO_MOTOR_TEST) return false; // No CAN write at all in test mode.
  // Installed FlexCAN_T4::write(MB, msg) queues on a busy MB. Precheck the only
  // TX mailbox under the sole timer writer, with seq=false, to avoid that path.
  if (!txIdle() || MotorCan.getTXQueueCount()) return false;
  return MotorCan.write(TX_MAILBOX, frame) != 0;
}
bool sendTorque(float torque, float limit = TORQUE_LIMIT_NM) {
  const MotorFrame frame=torqueFrame(torque, limit);
  CAN_message_t m = {};
  m.id=frame.id; m.flags.extended=frame.extended; m.len=frame.len;
  memcpy(m.buf,frame.data,8);
  return sendFrame(m);
}
bool sendSpecial(uint8_t code) {
  if (IS_60) return false; // AK60 has no verified AK80 FC/FD commands.
  CAN_message_t m = {}; m.id = MOTOR_ID; m.len = 8;
  for (unsigned i = 0; i < 7; ++i) m.buf[i] = 0xff;
  m.buf[7] = code;
  return sendFrame(m);
}

void receiveISR(const CAN_message_t &m) {
  const uint32_t now = micros(); // callback timestamp, not hardware bus timestamp.
  if (runState != WAIT_FEEDBACK && !controlling()) return;
  MotorFrame frame={m.id,m.flags.extended,m.flags.remote,m.flags.overrun,m.len,{}};
  memcpy(frame.data,m.buf,8);
  const MotorFeedback decoded=decodeMotorFeedback(frame);
  if (!decoded.valid) return;
  if (m.flags.overrun) { rxFault = CAN_ERROR; return; }
  Feedback next = {};
  next.positionRad=decoded.positionRad; next.speedRadS=decoded.speedRadS;
  next.torqueEstimateNm=decoded.torqueEstimateNm; next.raw=decoded.raw;
  // Both torque fields are internal estimates, never external sensor readings.
  next.rxUs=now; next.sequence=feedback.sequence+1; next.error=decoded.error;
  if (next.error) rxFault = MOTOR_ERROR;
  // NO 360-degree or 25-rad unwrapping: actual firmware wrap is unknown.
  // Within the encoded multi-turn range raw position is used directly. Refuse
  // boundary approach or a nonphysical jump rather than invent a revolution.
  if (next.positionRad <= POSITION_MIN_RAD + POSITION_BOUNDARY_MARGIN_RAD ||
      next.positionRad >= POSITION_MAX_RAD - POSITION_BOUNDARY_MARGIN_RAD)
    rxFault = POSITION_INVALID;
  if (feedback.sequence) {
    const float bound = VELOCITY_MAX_RAD_S * ((now - feedback.rxUs) * 1e-6f) +
                        POSITION_JUMP_TOLERANCE_RAD;
    if (fabsf(next.positionRad - feedback.positionRad) > bound) rxFault = POSITION_INVALID;
  }
  feedback = next;
  if (runState == AVERAGING && now-firstUs < PRE_STEPS*PERIOD_US && rxFault == COMPLETE &&
      !referenceAverage.observe(next.positionRad, next.sequence)) rxFault = REFERENCE_INVALID;
  __asm__ volatile("dmb" ::: "memory");
}

void beginStop(uint32_t reason, uint32_t now) {
  if (runState == STOPPING || runState == STOPPED || runState == IDLE) return;
  if (status == COMPLETE) status = reason; // Retain first cause.
  runState = STOPPING; stopStartedUs = now; stopStage = 0; abortPending = false;
  if (NO_MOTOR_TEST) { stopFlags = 16; runState = STOPPED; return; }
  // Cancel a pending old command before requesting zero/FD. One bounded register
  // operation, never wait in ISR. Already transmitted frames cannot be undone.
  if (!txIdle()) {
    FLEXCANb_MBn_CS(MOTOR_CAN_BUS, 8) = FLEXCAN_MB_CS_CODE(FLEXCAN_MB_CODE_TX_ABORT);
    abortPending = true;
  }
}
void serviceStop(uint32_t now) {
  if (NO_MOTOR_TEST) { stopFlags = 16; runState = STOPPED; return; }
  if (abortPending) {
    const uint32_t code = FLEXCAN_get_code(FLEXCANb_MBn_CS(MOTOR_CAN_BUS, 8));
    if ((FLEXCANb_IFLAG1(MOTOR_CAN_BUS) & TX_BIT) &&
        (code == FLEXCAN_MB_CODE_TX_ABORT || code == FLEXCAN_MB_CODE_TX_INACTIVE)) {
      FLEXCANb_IFLAG1(MOTOR_CAN_BUS) = TX_BIT;
      FLEXCANb_MBn_CS(MOTOR_CAN_BUS, 8) = FLEXCAN_MB_CS_CODE(FLEXCAN_MB_CODE_TX_INACTIVE);
      abortPending = false;
    }
  }
  if (!abortPending && txIdle()) {
    if (stopStage == 0 && sendTorque(0)) { stopFlags |= 1; stopStage = 1; }
    else if (stopStage == 1) {
      stopFlags |= 2;
      if (IS_60) runState=STOPPED; // Zero TX only, NOT mode exit or brake holding.
      else if (sendSpecial(0xfd)) { stopFlags |= 4; stopStage = 2; }
    } else if (stopStage == 2) { stopFlags |= 8; runState = STOPPED; }
  }
  if (runState != STOPPED && now - stopStartedUs >= STOP_TIMEOUT_US) {
    // Keep the original cause, expose failed stop in stopFlags, lock restart.
    if (status == COMPLETE) status = STOP_FAILED;
    restartLocked = true;
    if (!txIdle()) FLEXCANb_MBn_CS(MOTOR_CAN_BUS, 8) = FLEXCAN_MB_CS_CODE(FLEXCAN_MB_CODE_TX_ABORT);
    runState = STOPPED;
  }
}

void simulateFeedback(uint32_t now, uint32_t index) {
  const double t = index * SAMPLING_PERIOD_S;
  feedback.positionRad = (5.0 * exp(-2*t) + 0.05 * sin(0.7*t)) * (3.141592653589793 / 180);
  feedback.speedRadS = (-10.0 * exp(-2*t) + 0.035 * cos(0.7*t)) * (3.141592653589793 / 180);
  feedback.torqueEstimateNm = 0; feedback.raw = 0;
  feedback.rxUs = now; feedback.sequence++; feedback.error = 0;
}
// Fault/user-stop snapshots preserve the actual error byte and feedback age.
// A full ring cannot accept a final row: its footer still records RING_FULL.
void stopBeforeSample(uint32_t reason, uint32_t entry) {
  const bool hadTarget = controlling();
  beginStop(reason, entry);
  if (runState == STOPPING) serviceStop(entry);
  if (!hadTarget || head-tail >= RING_SIZE || head >= EXPECTED_ROWS) return;
  const Feedback fb = feedback;
  Sample row = {};
  row.index = head; row.actualUs = entry-firstUs; row.phase = STOP_ROW;
  row.positionDeg = fb.positionRad*(180.0f/PI); row.speedRadS = fb.speedRadS;
  row.feedbackTorqueEstimateNm = fb.torqueEstimateNm;
  row.feedbackAgeUs = entry-fb.rxUs; row.feedbackSequence = fb.sequence;
  row.motorError = fb.error; row.feedbackRaw = fb.raw;
  row.txAccepted = (stopFlags & 1) ? 1 : 0;
  ring[head % RING_SIZE] = row;
  ring[head % RING_SIZE].executionUs = micros()-entry;
  __asm__ volatile("dmb" ::: "memory");
  head++;
}
void controlISR() {
  const uint32_t entry = micros();
  if (startRequested && runState == IDLE) {
    startRequested = false; feedback = {}; rxFault = COMPLETE;
    status = COMPLETE; stopFlags = 0; holdDeg = NAN; averageRows = sweepStartIndex = 0; referenceAverage.reset();
    entered = zeroSent = zeroBeforeEnter = false; waitStartedUs = entry; runState = WAIT_FEEDBACK;
  }
  if (runState == IDLE || runState == STOPPED) {
    // Existing AK60 protocol has no verified disable command. Continue zero
    // torque keepalive after stopping, including during SD/USB download.
    if (IS_60 && !NO_MOTOR_TEST && canReady && txIdle()) sendTorque(0);
    return;
  }
  if (ioStopRequested) { ioStopRequested = false; stopBeforeSample(SD_ERROR, entry); return; }
  if (userStopRequested) { userStopRequested = false; stopBeforeSample(USER_ABORT, entry); return; }
  if (runState == STOPPING) { serviceStop(entry); return; }
  if (!NO_MOTOR_TEST && (FLEXCANb_ESR1(MOTOR_CAN_BUS) & FLEXCAN_ESR_FLT_CONF_MASK))
    { stopBeforeSample(CAN_ERROR, entry); return; } // error-passive/bus-off
  if (rxFault != COMPLETE) { stopBeforeSample(rxFault, entry); return; }
  if (runState == STOPPING) { serviceStop(entry); return; }
  if (NO_MOTOR_TEST) {
    simulateFeedback(entry, head);
    if (runState == AVERAGING && entry-firstUs < PRE_STEPS*PERIOD_US &&
        !referenceAverage.observe(feedback.positionRad, feedback.sequence)) {
      stopBeforeSample(REFERENCE_INVALID, entry); return;
    }
  }
  if (runState == WAIT_FEEDBACK) {
    if (entry - waitStartedUs >= START_TIMEOUT_US) { beginStop(START_TIMEOUT, entry); return; }
    if (!NO_MOTOR_TEST && !zeroBeforeEnter) {
      // Request zero before entry, then repeat after FC. Whether disabled
      // firmware latches the first request is unverified.
      if (!sendTorque(0)) beginStop(TX_FAILED, entry);
      else zeroBeforeEnter = true;
      return;
    }
    if (!NO_MOTOR_TEST && !IS_60 && !entered) {
      if (!sendSpecial(0xfc)) beginStop(TX_FAILED, entry);
      else entered = true;
      return;
    }
    if (!NO_MOTOR_TEST && !zeroSent) {
      if (!sendTorque(0)) beginStop(TX_FAILED, entry);
      else zeroSent = true;
      return;
    }
    if (!feedback.sequence || entry - feedback.rxUs > FEEDBACK_TIMEOUT_US) {
      if (!NO_MOTOR_TEST && !sendTorque(0)) beginStop(TX_FAILED, entry);
      return;
    }
    // Capture initial position under zero commanded torque; no homing/PD here.
    firstUs = previousTickUs = entry;
    // Hardware average starts with responses received after this timestamp.
    // Test feedback is generated at this exact timestamp.
    if (NO_MOTOR_TEST && !referenceAverage.observe(feedback.positionRad, feedback.sequence)) {
      beginStop(REFERENCE_INVALID, entry); return;
    }
    runState = AVERAGING;
  }
  const Feedback fb = feedback; // Same-priority ISR snapshot, never a mixed frame.
  const uint32_t age = entry - fb.rxUs;
  if (age > FEEDBACK_TIMEOUT_US) { stopBeforeSample(FEEDBACK_LOST, entry); return; }
  if (head && entry - previousTickUs > MAX_CONTROL_GAP_US) {
    stopBeforeSample(CONTROL_DEADLINE, entry); return;
  }
  previousTickUs = entry;
  if (!isfinite(fb.positionRad) || !isfinite(fb.speedRadS) || !isfinite(fb.torqueEstimateNm)) {
    stopBeforeSample(NONFINITE_CONTROL, entry); return;
  }
  const uint32_t index = head;
  if (index >= EXPECTED_ROWS) { stopBeforeSample(RECORD_LIMIT, entry); return; }
  if (runState == AVERAGING && index >= PRE_STEPS+MAX_AVERAGE_EXTRA_STEPS) {
    stopBeforeSample(CONTROL_DEADLINE, entry); return;
  }
  if (runState == AVERAGING && index >= PRE_STEPS && entry-firstUs >= PRE_STEPS*PERIOD_US) {
    if (!referenceAverage.valid()) { stopBeforeSample(REFERENCE_INVALID, entry); return; }
    holdDeg = referenceAverage.meanRad()*(180.0f/PI);
    if (!isfinite(holdDeg)) { stopBeforeSample(REFERENCE_INVALID, entry); return; }
    sweepStartIndex = index; runState = RUNNING;
  }
  if (index - tail >= RING_SIZE) { stopBeforeSample(RING_FULL, entry); return; }
  Sample row = {};
  row.index = index; row.actualUs = entry-firstUs; row.phase = runState == AVERAGING ? PRE_HOLD : phaseAt(PRE_STEPS+index-sweepStartIndex);
  row.positionDeg = fb.positionRad*(180.0f/PI); row.speedRadS = fb.speedRadS;
  row.feedbackTorqueEstimateNm = fb.torqueEstimateNm;
  row.feedbackAgeUs = age; row.feedbackSequence = fb.sequence;
  row.motorError = fb.error; row.feedbackRaw = fb.raw;
  row.chirpNm = row.phase == SWEEP ? calculateChirp((index-sweepStartIndex)*SAMPLING_PERIOD_S) : 0;
  // Strictly zero command throughout averaging: no damping, spring, or PI.
  row.pdNm = (row.phase == PRE_HOLD || row.phase == STOP_ROW) ? 0 :
      pdTorque(holdDeg, row.positionDeg, row.speedRadS);
  const float raw = row.chirpNm+row.pdNm;
  if (!isfinite(raw) || !isfinite(row.pdNm) || !isfinite(row.chirpNm)) {
    stopBeforeSample(NONFINITE_CONTROL, entry); return;
  }
  row.limited = fabsf(raw) > TORQUE_LIMIT_NM;
  row.commandNm = fmaxf(-TORQUE_LIMIT_NM, fminf(TORQUE_LIMIT_NM, raw));
  if (row.limited) { row.commandNm = 0; row.phase = STOP_ROW; beginStop(TORQUE_LIMIT, entry); }
  else if (row.phase == STOP_ROW) beginStop(COMPLETE, entry);
  else if (!NO_MOTOR_TEST) {
    row.txAccepted = sendTorque(row.commandNm);
    if (!row.txAccepted) beginStop(TX_FAILED, entry);
  }
  if (runState == STOPPING) {
    serviceStop(entry);
    // STOP_ROW's txAccepted denotes the zero request, if submitted on this tick.
    if (row.phase == STOP_ROW && (stopFlags & 1)) row.txAccepted = 1;
  }
  // Response is the latest snapshot BEFORE this row's send request. MIT has no
  // command sequence ID: this is NOT a synchronous response to this row's torque.
  ring[index % RING_SIZE] = row;
  const uint32_t execution = micros() - entry;
  ring[index % RING_SIZE].executionUs = execution;
  __asm__ volatile("dmb" ::: "memory");
  head = index + 1;
  if (row.phase == PRE_HOLD) averageRows++;
  if (execution >= PERIOD_US && controlling()) beginStop(CONTROL_DEADLINE, micros());
}

Header makeHeader() {
  Header h = {{'C','H','I','R','P','S','I','1'}, PERIOD_US, DURATION_US,
    PRE_STEPS*PERIOD_US, POST_STEPS*PERIOD_US, EXPECTED_ROWS, sizeof(Sample),
    MOTOR_ID, CAN_BITRATE, NO_MOTOR_TEST, 0, FEEDBACK_TIMEOUT_US,
    MOTOR_PROFILE_DOCUMENTED, VERIFIED_MOTOR_TIMEOUT_MS,
    START_FREQUENCY_HZ, END_FREQUENCY_HZ, START_AMPLITUDE_NM, END_AMPLITUDE_NM,
    AMPLITUDE_EXPONENT, KP_PD, KD_PD, NAN, TORQUE_LIMIT_NM,
    POSITION_MIN_RAD, POSITION_MAX_RAD, VELOCITY_MAX_RAD_S, MIT_TORQUE_MAX_NM,
    PRE_STEPS*PERIOD_US, 0, 0, NAN, NAN, 2, CAN_PORT_NUMBER, MOTOR_PROTOCOL};
  return h;
}

void startRun() {
  if (recording || !cardReady || !timerReady || restartLocked) return;
  if (!NO_MOTOR_TEST && !canReady) {
    Serial.println("ERROR,CAN_INIT"); return;
  }
  bool found = false;
  for (uint32_t n = 0; n < 10000; ++n) {
    snprintf(filename, sizeof(filename), "SI%04lu.BIN", (unsigned long)n);
    if (!sd.exists(filename)) { found = true; break; }
  }
  if (!found || !logFile.open(filename, O_RDWR | O_CREAT | O_EXCL)) {
    Serial.println("ERROR,FILE_CREATE"); return;
  }
  strcpy(lastSaved, filename); // Never silently download an older run after a new failure.
  runHeader = makeHeader();
  if (!logFile.preAllocate(LOG_CAPACITY_BYTES) ||
      logFile.write(&runHeader, sizeof(runHeader)) != sizeof(runHeader) || !logFile.sync()) {
    logFile.close(); Serial.println("ERROR,FILE_PREPARE"); return;
  }
  const uint32_t p = lockInterrupts();
  head = tail = 0; status = COMPLETE; stopFlags = 0;
  userStopRequested = ioStopRequested = false;
  highWater = maxWriteUs = 0; ioFailed = false; recording = true;
  startRequested = true;
  unlockInterrupts(p);
  Serial.print("START,"); Serial.println(filename);
}
void serviceLog() {
  if (!recording) return;
  const uint32_t available = head - tail;
  __asm__ volatile("dmb" ::: "memory");
  if (available > highWater) highWater = available;
  const bool stopped = runState == STOPPED;
  if (!ioFailed && available && (available >= BATCH || stopped)) {
    uint32_t rows = available < BATCH ? available : BATCH;
    const uint32_t offset = tail % RING_SIZE;
    if (rows > RING_SIZE - offset) rows = RING_SIZE - offset;
    const uint32_t started = micros(), bytes = rows * sizeof(Sample);
    // tail is not released until SD write returns. CAN and timer interrupts
    // remain enabled throughout foreground writes; no events() dependency.
    const size_t written = logFile.write(&ring[offset], bytes);
    const uint32_t elapsed = micros() - started;
    if (elapsed > maxWriteUs) maxWriteUs = elapsed;
    if (written != bytes) {
      ioFailed = true;
      ioStopRequested = true; // Timer executes motor stop even during later SD work.
      const uint32_t p = lockInterrupts();
      if (status == COMPLETE) status = SD_ERROR;
      unlockInterrupts(p);
    } else {
      __asm__ volatile("dmb" ::: "memory"); tail += rows;
    }
  }
  // Do not close/drain/final-sync before zero + exit processing has finished.
  if (runState != STOPPED || (!ioFailed && tail != head)) return;
  const uint32_t rows = ioFailed ? tail : head;
  runHeader.holdDeg = isfinite(holdDeg) ? holdDeg :
      (referenceAverage.count ? referenceAverage.meanRad()*(180.0f/PI) : NAN);
  runHeader.referenceSamples = referenceAverage.count;
  runHeader.referenceMinRad = referenceAverage.count ? referenceAverage.minRad : NAN;
  runHeader.referenceMaxRad = referenceAverage.count ? referenceAverage.maxRad : NAN;
  runHeader.averageRows = averageRows < rows ? averageRows : rows;
  const Footer footer = {{'E','N','D','S','I','0','0','1'}, rows, status, stopFlags};
  // On partial write, discard that entire batch and try to finalize the known
  // committed prefix. A failing card may also reject this best-effort recovery.
  const bool ok = logFile.seekSet(sizeof(Header) + uint64_t(rows) * sizeof(Sample)) &&
      logFile.write(&footer, sizeof(footer)) == sizeof(footer) && logFile.truncate() &&
      logFile.seekSet(0) && logFile.write(&runHeader, sizeof(runHeader)) == sizeof(runHeader) &&
      logFile.sync();
  logFile.close(); recording = false;
  if (ok) strcpy(lastSaved, filename);
  if (!ok || ioFailed) cardReady = false;
  Serial.print(ok ? "SAVED," : "ERROR,FINALIZE,"); Serial.print(filename);
  Serial.print(",Rows,"); Serial.print(rows);
  Serial.print(",Status,"); Serial.print(status);
  Serial.print(",StopFlags,"); Serial.print(stopFlags);
  Serial.print(",BufferObservedMax_rows,"); Serial.print(highWater);
  Serial.print(",WriteMax_us,"); Serial.println(maxWriteUs);
  Serial.println(restartLocked ? "STOP_UNCONFIRMED; POWER_OFF_AND_CHECK_BUS" : "READY: s=start, x=stop, d=download");
  if (IS_60 && !NO_MOTOR_TEST) Serial.println("AK60_STOP_IS_ZERO_TORQUE_KEEPALIVE_NOT_MODE_EXIT");
  runState = IDLE;
}

void printConfig(const Header &h) {
  Serial.print("CONFIG,Hz,"); Serial.print(1000000.0/h.periodUs, 9);
  Serial.print(",Sweep_us,"); Serial.print(h.sweepUs);
  Serial.print(",Pre_us,"); Serial.print(h.preUs);
  Serial.print(",Post_us,"); Serial.print(h.postUs);
  Serial.print(",Duration_s,"); Serial.print(h.sweepUs/1e6, 9);
  Serial.print(",PreHold_s,"); Serial.print(h.preUs/1e6, 9);
  Serial.print(",PostHold_s,"); Serial.print(h.postUs/1e6, 9);
  Serial.print(",F0_Hz,"); Serial.print(h.f0, 9);
  Serial.print(",F1_Hz,"); Serial.print(h.f1, 9);
  Serial.print(",A0_Nm,"); Serial.print(h.amp0, 9);
  Serial.print(",A1_Nm,"); Serial.print(h.amp1, 9);
  Serial.print(",AmplitudeExponent,"); Serial.print(h.amplitudeExponent, 9);
  Serial.print(",KP_Nm_per_deg,"); Serial.print(h.kpNmPerDeg, 9);
  Serial.print(",KD_Nm_per_rad_s,"); Serial.print(h.kdNmPerRadS, 9);
  Serial.print(",Hold_deg,"); Serial.print(h.holdDeg, 9);
  Serial.print(",TorqueLimit_Nm,"); Serial.print(h.torqueLimitNm, 9);
  Serial.print(",Period_us,"); Serial.print(h.periodUs);
  Serial.print(",RowBytes,"); Serial.print(h.rowBytes);
  Serial.print(",ExpectedRows,"); Serial.print(h.expectedRows);
  Serial.print(",MotorID,"); Serial.print(h.motorId);
  Serial.print(",CAN_bitrate,"); Serial.print(h.canBitrate);
  Serial.print(",NoMotor,"); Serial.print(h.noMotor);
  Serial.print(",PositionPolicy,"); Serial.print(h.positionPolicy);
  Serial.print(",FeedbackTimeout_us,"); Serial.print(h.feedbackTimeoutUs);
  Serial.print(",ProfileDocumented,"); Serial.print(h.profileDocumented);
  Serial.print(",MotorTimeout_ms,"); Serial.print(h.motorTimeoutMs);
  Serial.print(",PositionMin_rad,"); Serial.print(h.positionMinRad, 9);
  Serial.print(",PositionMax_rad,"); Serial.print(h.positionMaxRad, 9);
  Serial.print(",VelocityMax_rad_s,"); Serial.print(h.velocityMaxRadS, 9);
  Serial.print(",MITTorqueMax_Nm,"); Serial.print(h.mitTorqueMaxNm, 9);
  Serial.print(",Average_us,"); Serial.print(h.averageUs);
  Serial.print(",ReferenceSamples,"); Serial.print(h.referenceSamples);
  Serial.print(",AverageRows,"); Serial.print(h.averageRows);
  Serial.print(",ReferenceMin_rad,"); Serial.print(h.referenceMinRad,9);
  Serial.print(",ReferenceMax_rad,"); Serial.print(h.referenceMaxRad,9);
  Serial.print(",HoldPolicy,"); Serial.print(h.holdPolicy);
  Serial.print(",CANPort,"); Serial.print(h.canPort);
  Serial.print(",MotorProtocol,"); Serial.println(h.motorProtocol);
}
void downloadLog() {
  if (recording) { Serial.println("ERROR,BUSY"); return; }
  // cardReady=false after write trouble does not prohibit a best-effort read.
  if (!lastSaved[0]) for (int n = 9999; n >= 0; --n) {
    snprintf(filename, sizeof(filename), "SI%04d.BIN", n);
    if (sd.exists(filename)) { strcpy(lastSaved, filename); break; }
  }
  FsFile input;
  if (!lastSaved[0] || !input.open(lastSaved, O_RDONLY)) {
    Serial.println("ERROR,NO_SAVED_FILE"); return;
  }
  Header h; Footer f;
  bool valid = input.read(&h, sizeof(h)) == sizeof(h) && memcmp(h.magic, "CHIRPSI1", 8) == 0 &&
      h.rowBytes == sizeof(Sample) && h.periodUs && h.expectedRows &&
      h.holdPolicy == 2 && h.averageUs == h.preUs && h.averageRows <= h.expectedRows &&
      h.averageRows <= h.preUs/h.periodUs+MAX_AVERAGE_EXTRA_STEPS &&
      uint64_t(h.expectedRows-1-MAX_AVERAGE_EXTRA_STEPS)*h.periodUs == uint64_t(h.preUs)+h.sweepUs+h.postUs;
  if (valid) valid = input.fileSize() >= sizeof(Header)+sizeof(Footer) &&
      input.seekSet(input.fileSize()-sizeof(Footer)) && input.read(&f,sizeof(f)) == sizeof(f) &&
      memcmp(f.magic,"ENDSI001",8) == 0 && f.rows <= h.expectedRows &&
      input.fileSize() == sizeof(Header)+uint64_t(f.rows)*sizeof(Sample)+sizeof(Footer);
  if (!valid || !input.seekSet(sizeof(Header))) {
    input.close(); Serial.println("ERROR,SD_FILE_INCOMPLETE_OR_UNSUPPORTED"); return;
  }
  Serial.print("SOURCE_FILE,"); Serial.println(lastSaved);
  Serial.print("CHIRP_BEGIN,VERSION,SI1,REASON,"); Serial.println(f.status == COMPLETE ? "COMPLETE" : "ABORTED");
  printConfig(h);
  Serial.print("RESULT,Status,"); Serial.print(f.status);
  Serial.print(",StopFlags,"); Serial.println(f.stopFlags);
  Serial.println("DATA,index,nominal_us,actual_us,period_us,phase,position_deg,speed_rad_s,feedback_torque_estimate_Nm,chirp_Nm,pd_Nm,command_Nm,torque_limited,feedback_age_us,feedback_sequence,can_tx_accepted,ISR_us,motor_error,feedback_raw");
  uint32_t previous = 0;
  for (uint32_t i=0; i<f.rows; ++i) {
    Sample r;
    if (input.read(&r,sizeof(r)) != sizeof(r)) { Serial.println("ERROR,SD_READ"); break; }
    Serial.print("DATA,"); Serial.print(r.index);
    Serial.print(','); Serial.print(uint64_t(r.index)*h.periodUs);
    Serial.print(','); Serial.print(r.actualUs);
    Serial.print(','); Serial.print(i ? r.actualUs-previous : 0);
    Serial.print(','); Serial.print(r.phase);
    const float values[] = {r.positionDeg,r.speedRadS,r.feedbackTorqueEstimateNm,r.chirpNm,r.pdNm,r.commandNm};
    for (float value : values) { Serial.print(','); Serial.print(value,9); }
    const uint32_t fields[] = {r.limited,r.feedbackAgeUs,r.feedbackSequence,r.txAccepted,r.executionUs,r.motorError,r.feedbackRaw};
    for (uint32_t value : fields) { Serial.print(','); Serial.print(value); }
    Serial.println(); previous=r.actualUs;
    if (!Serial) { input.close(); return; }
  }
  input.close(); Serial.print("SUMMARY,Count,"); Serial.println(f.rows); Serial.println("CHIRP_END");
}

bool setupCAN() {
  MotorCan.begin(); MotorCan.setBaudRate(CAN_BITRATE); MotorCan.setMaxMB(16);
  for (unsigned i=0; i<16; ++i) MotorCan.setMB(FLEXCAN_MAILBOX(i), RX, INACTIVE);
  MotorCan.setMB(MB0, RX, IS_60 ? EXT : STD);
  MotorCan.setMBFilter(REJECT_ALL);
  MotorCan.setMB(TX_MAILBOX, TX, STD); // Only one TX mailbox, no delayed command chain.
  // setMB(TX) enables its IRQ; disableMBInterrupt(TX) is a keeper in this
  // library and does NOT clear it. Clear the mask directly for polled aborts.
  FLEXCANb_IMASK1(MOTOR_CAN_BUS) &= ~TX_BIT;
  MotorCan.setMBFilter(MB0, IS_60 ? ((0x29u<<8)|MOTOR_ID) : MOTOR_ID);
  MotorCan.onReceive(MB0, receiveISR); MotorCan.enableMBInterrupt(MB0);
  // TX interrupt stays disabled: controlISR polls completion/abort state.
  NVIC_SET_PRIORITY(IS_60 ? IRQ_CAN3 : (IS_80A ? IRQ_CAN1 : IRQ_CAN2), ISR_PRIORITY);
  // Enable hardware TX-abort support while frozen, with bounded setup waits.
  FLEXCANb_MCR(MOTOR_CAN_BUS) |= FLEXCAN_MCR_FRZ | FLEXCAN_MCR_HALT;
  uint32_t started = micros();
  while (!(FLEXCANb_MCR(MOTOR_CAN_BUS) & FLEXCAN_MCR_FRZ_ACK)) if (micros()-started>10000) return false;
  FLEXCANb_MCR(MOTOR_CAN_BUS) |= FLEXCAN_MCR_AEN;
  FLEXCANb_MCR(MOTOR_CAN_BUS) &= ~FLEXCAN_MCR_HALT;
  started = micros();
  while (FLEXCANb_MCR(MOTOR_CAN_BUS) & FLEXCAN_MCR_FRZ_ACK) if (micros()-started>10000) return false;
  return true;
}
void setup() {
  Serial.begin(115200);
  const uint32_t started = millis();
  while (!Serial && millis()-started < 5000) {}
  if (!NO_MOTOR_TEST) canReady = setupCAN();
  cardReady = sd.begin(SdioConfig(FIFO_SDIO));
  controlTimer.priority(ISR_PRIORITY);
  timerReady = controlTimer.begin(controlISR, PERIOD_US); // Always running, including stops.
  Serial.print(NO_MOTOR_TEST ? "CHIRP_SIMPLIFIED_TEST1,NO_MOTOR_TEST,MOTOR," : "CHIRP_SIMPLIFIED_TEST1,MOTOR,");
  Serial.print(MOTOR_SELECT); Serial.print(",CAN,"); Serial.print(CAN_PORT_NUMBER);
  Serial.print(",ID,"); Serial.println(MOTOR_ID);
  Serial.println(cardReady && timerReady ? "SD_OK; READY: s=start, x=stop, d=download" : "ERROR,SD_OR_TIMER_INIT");
}
void loop() {
  bool start = false;
  for (unsigned i=0; i<32 && Serial.available(); ++i) {
    const char c=Serial.read();
    if (c=='x' || c=='X') { userStopRequested=true; start=false; }
    else if ((c=='s' || c=='S') && !userStopRequested) start=true;
    else if (c=='d' || c=='D') downloadLog();
  }
  if (start && !recording) startRun();
  if (!recording) userStopRequested=false;
  serviceLog(); // Never call MotorCan.events(): direct ISR callbacks also run in SD writes.
}
