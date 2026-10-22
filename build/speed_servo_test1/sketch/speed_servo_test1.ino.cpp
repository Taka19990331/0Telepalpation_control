#line 1 "C:\\0Telepalpation_control\\servo_mode\\speed_servo_test1\\speed_servo_test1.ino"
#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <IntervalTimer.h>
#include <math.h>

// AK80-8 Servo Mode velocity-loop test for Teensy 4.1.
// The motor must already be configured for Servo Mode and calibrated.

constexpr uint8_t MOTOR_ID = 2;
constexpr uint32_t CAN_BAUD = 1000000;
constexpr uint32_t CONTROL_PERIOD_US = 10000;

constexpr int32_t TARGET_SPEED_ERPM = 200;
constexpr uint32_t RAMP_UP_US = 1000000;
constexpr uint32_t HOLD_US = 2000000;
constexpr uint32_t RAMP_DOWN_US = 1000000;
constexpr uint32_t TEST_DURATION_US = RAMP_UP_US + HOLD_US + RAMP_DOWN_US;
constexpr uint32_t FEEDBACK_TIMEOUT_US = 200000;
constexpr uint16_t LOG_CAPACITY = TEST_DURATION_US / CONTROL_PERIOD_US + 1;

static_assert(LOG_CAPACITY >= 401, "Log must fit the full 4-second trial");

constexpr uint8_t CAN_PACKET_SET_RPM = 3;
constexpr uint32_t SERVO_FEEDBACK_FUNCTION_ID = 0x29;

FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> Can2;
IntervalTimer controlTimer;

struct Feedback {
  float positionDeg;
  int32_t velocityErpm;
  float currentA;
  int8_t temperatureC;
  uint8_t errorCode;
  uint32_t receivedUs;
  bool valid;
};

struct LogRow {
  uint32_t timestampUs;
  int32_t commandErpm;
  float positionDeg;
  int32_t velocityErpm;
  float currentA;
  int8_t temperatureC;
  uint8_t errorCode;
  uint32_t feedbackTimestampUs;
  uint8_t feedbackValid;
};

LogRow logRows[LOG_CAPACITY];
uint16_t logCount = 0;
Feedback latestFeedback = {};
uint32_t receivedFrameCount = 0;
uint32_t ignoredCanFrameCount = 0;
uint32_t lastCanId = 0;
uint32_t lastCanFrameUs = 0;
uint8_t lastCanLength = 0;
uint8_t lastCanExtended = 0;
uint8_t lastCanRemote = 0;
uint8_t lastCanData[8] = {};

// The timer ISR only publishes a timestamped tick. CAN and Serial APIs run in loop().
volatile bool tickPending = false;
volatile bool tickOverrun = false;
volatile uint32_t tickTimestampUs = 0;
bool timerReady = false;
bool trialRunning = false;
bool reportPending = false;
bool zeroHold = false;
bool reportHeaderPrinted = false;
uint16_t reportIndex = 0;
uint32_t trialStartUs = 0;
uint32_t txFailureCount = 0;
uint32_t maxTxDelayUs = 0;
uint32_t abortedTicks = 0;
const char *trialResult = "NOT_STARTED";

#line 79 "C:\\0Telepalpation_control\\servo_mode\\speed_servo_test1\\speed_servo_test1.ino"
void controlTickISR();
#line 89 "C:\\0Telepalpation_control\\servo_mode\\speed_servo_test1\\speed_servo_test1.ino"
int16_t readInt16BigEndian(const uint8_t high, const uint8_t low);
#line 93 "C:\\0Telepalpation_control\\servo_mode\\speed_servo_test1\\speed_servo_test1.ino"
void writeInt32BigEndian(uint8_t *buffer, const int32_t value);
#line 101 "C:\\0Telepalpation_control\\servo_mode\\speed_servo_test1\\speed_servo_test1.ino"
constexpr uint32_t makeServoExtendedId(const uint8_t functionId, const uint8_t motorId);
#line 105 "C:\\0Telepalpation_control\\servo_mode\\speed_servo_test1\\speed_servo_test1.ino"
bool sendSpeedCommand(const int32_t erpm);
#line 116 "C:\\0Telepalpation_control\\servo_mode\\speed_servo_test1\\speed_servo_test1.ino"
void readCanFeedback();
#line 149 "C:\\0Telepalpation_control\\servo_mode\\speed_servo_test1\\speed_servo_test1.ino"
bool feedbackIsFresh();
#line 155 "C:\\0Telepalpation_control\\servo_mode\\speed_servo_test1\\speed_servo_test1.ino"
void printFeedbackDiagnostics();
#line 184 "C:\\0Telepalpation_control\\servo_mode\\speed_servo_test1\\speed_servo_test1.ino"
void saveLogRow(const uint32_t timestampUs, const int32_t commandErpm);
#line 200 "C:\\0Telepalpation_control\\servo_mode\\speed_servo_test1\\speed_servo_test1.ino"
int32_t speedCommandAt(const uint32_t elapsedUs);
#line 214 "C:\\0Telepalpation_control\\servo_mode\\speed_servo_test1\\speed_servo_test1.ino"
void finishTrial(const char *result, const bool attemptZeroNow);
#line 226 "C:\\0Telepalpation_control\\servo_mode\\speed_servo_test1\\speed_servo_test1.ino"
void startTrial();
#line 263 "C:\\0Telepalpation_control\\servo_mode\\speed_servo_test1\\speed_servo_test1.ino"
void handlePendingTick();
#line 319 "C:\\0Telepalpation_control\\servo_mode\\speed_servo_test1\\speed_servo_test1.ino"
void serviceSerialInput();
#line 330 "C:\\0Telepalpation_control\\servo_mode\\speed_servo_test1\\speed_servo_test1.ino"
void serviceReport();
#line 367 "C:\\0Telepalpation_control\\servo_mode\\speed_servo_test1\\speed_servo_test1.ino"
void setup();
#line 378 "C:\\0Telepalpation_control\\servo_mode\\speed_servo_test1\\speed_servo_test1.ino"
void loop();
#line 79 "C:\\0Telepalpation_control\\servo_mode\\speed_servo_test1\\speed_servo_test1.ino"
void controlTickISR() {
  const uint32_t now = micros();
  if (tickPending) {
    tickOverrun = true;
  } else {
    tickTimestampUs = now;
    tickPending = true;
  }
}

int16_t readInt16BigEndian(const uint8_t high, const uint8_t low) {
  return static_cast<int16_t>((static_cast<uint16_t>(high) << 8) | low);
}

void writeInt32BigEndian(uint8_t *buffer, const int32_t value) {
  const uint32_t bits = static_cast<uint32_t>(value);
  buffer[0] = static_cast<uint8_t>(bits >> 24);
  buffer[1] = static_cast<uint8_t>(bits >> 16);
  buffer[2] = static_cast<uint8_t>(bits >> 8);
  buffer[3] = static_cast<uint8_t>(bits);
}

constexpr uint32_t makeServoExtendedId(const uint8_t functionId, const uint8_t motorId) {
  return (static_cast<uint32_t>(functionId) << 8) | motorId;
}

bool sendSpeedCommand(const int32_t erpm) {
  CAN_message_t message = {};
  message.id = makeServoExtendedId(CAN_PACKET_SET_RPM, MOTOR_ID);
  message.flags.extended = 1;
  message.len = 4;
  writeInt32BigEndian(message.buf, erpm);
  const bool accepted = Can2.write(message);
  if (!accepted) ++txFailureCount;
  return accepted;
}

void readCanFeedback() {
  CAN_message_t message;
  constexpr uint32_t expectedId = makeServoExtendedId(
      SERVO_FEEDBACK_FUNCTION_ID, MOTOR_ID);
  while (Can2.read(message)) {
    ++receivedFrameCount;
    lastCanId = message.id;
    lastCanFrameUs = micros();
    lastCanLength = message.len;
    lastCanExtended = message.flags.extended ? 1 : 0;
    lastCanRemote = message.flags.remote ? 1 : 0;
    for (uint8_t i = 0; i < 8; ++i) {
      lastCanData[i] = i < message.len ? message.buf[i] : 0;
    }
    if (!message.flags.extended || message.flags.remote || message.len != 8 ||
        message.id != expectedId) {
      ++ignoredCanFrameCount;
      continue;
    }

    const int16_t positionRaw = readInt16BigEndian(message.buf[0], message.buf[1]);
    const int16_t speedRaw = readInt16BigEndian(message.buf[2], message.buf[3]);
    const int16_t currentRaw = readInt16BigEndian(message.buf[4], message.buf[5]);
    latestFeedback.positionDeg = static_cast<float>(positionRaw) * 0.1f;
    latestFeedback.velocityErpm = static_cast<int32_t>(speedRaw) * 10;
    latestFeedback.currentA = static_cast<float>(currentRaw) * 0.01f;
    latestFeedback.temperatureC = static_cast<int8_t>(message.buf[6]);
    latestFeedback.errorCode = message.buf[7];
    latestFeedback.receivedUs = micros();
    latestFeedback.valid = true;
  }
}

bool feedbackIsFresh() {
  return latestFeedback.valid &&
         static_cast<uint32_t>(micros() - latestFeedback.receivedUs) <=
             FEEDBACK_TIMEOUT_US;
}

void printFeedbackDiagnostics() {
  constexpr uint32_t expectedId = makeServoExtendedId(
      SERVO_FEEDBACK_FUNCTION_ID, MOTOR_ID);
  Serial.print("Feedback check: expected extended ID 0x");
  Serial.print(expectedId, HEX);
  Serial.print(" (CAN2, ");
  Serial.print(CAN_BAUD);
  Serial.println(" bps, DLC 8).");
  Serial.print("RX frames="); Serial.print(receivedFrameCount);
  Serial.print(", ignored="); Serial.println(ignoredCanFrameCount);
  if (receivedFrameCount == 0) {
    Serial.println("No CAN frames received. Check CAN2 wiring, transceiver, ground, bitrate, and Servo Mode upload setting.");
    return;
  }
  Serial.print("Last RX: age_us=");
  Serial.print(static_cast<uint32_t>(micros() - lastCanFrameUs));
  Serial.print(" ID=0x"); Serial.print(lastCanId, HEX);
  Serial.print(" extended="); Serial.print(lastCanExtended);
  Serial.print(" remote="); Serial.print(lastCanRemote);
  Serial.print(" DLC="); Serial.print(lastCanLength);
  Serial.print(" DATA=");
  for (uint8_t i = 0; i < lastCanLength && i < 8; ++i) {
    if (lastCanData[i] < 0x10) Serial.print('0');
    Serial.print(lastCanData[i], HEX);
    if (i + 1 < lastCanLength && i < 7) Serial.print(' ');
  }
  Serial.println();
}

void saveLogRow(const uint32_t timestampUs, const int32_t commandErpm) {
  if (logCount >= LOG_CAPACITY) return;
  LogRow &row = logRows[logCount++];
  row.timestampUs = timestampUs;
  row.commandErpm = commandErpm;
  row.feedbackValid = feedbackIsFresh() ? 1 : 0;
  row.positionDeg = row.feedbackValid ? latestFeedback.positionDeg : NAN;
  row.velocityErpm = row.feedbackValid ? latestFeedback.velocityErpm : 0;
  row.currentA = row.feedbackValid ? latestFeedback.currentA : NAN;
  row.temperatureC = row.feedbackValid ? latestFeedback.temperatureC : 0;
  row.errorCode = row.feedbackValid ? latestFeedback.errorCode : 0;
  row.feedbackTimestampUs = row.feedbackValid
      ? static_cast<uint32_t>(latestFeedback.receivedUs - trialStartUs)
      : 0;
}

int32_t speedCommandAt(const uint32_t elapsedUs) {
  if (elapsedUs < RAMP_UP_US) {
    return static_cast<int32_t>(
        (static_cast<int64_t>(TARGET_SPEED_ERPM) * elapsedUs) / RAMP_UP_US);
  }
  if (elapsedUs < RAMP_UP_US + HOLD_US) return TARGET_SPEED_ERPM;
  if (elapsedUs < TEST_DURATION_US) {
    const uint32_t remainingUs = TEST_DURATION_US - elapsedUs;
    return static_cast<int32_t>(
        (static_cast<int64_t>(TARGET_SPEED_ERPM) * remainingUs) / RAMP_DOWN_US);
  }
  return 0;
}

void finishTrial(const char *result, const bool attemptZeroNow) {
  trialRunning = false;
  zeroHold = true;
  trialResult = result;
  if (attemptZeroNow && !sendSpeedCommand(0)) {
    trialResult = "STOP_TX_FAILED";
  }
  reportPending = true;
  reportHeaderPrinted = false;
  reportIndex = 0;
}

void startTrial() {
  if (!timerReady) {
    Serial.println("Cannot start: 100 Hz timer unavailable.");
    return;
  }
  if (trialRunning || reportPending) return;
  if (!feedbackIsFresh()) {
    Serial.println("Cannot start: waiting for fresh Servo Mode feedback.");
    printFeedbackDiagnostics();
    return;
  }
  if (latestFeedback.errorCode != 0) {
    Serial.print("Cannot start: motor error code ");
    Serial.println(latestFeedback.errorCode);
    return;
  }

  noInterrupts();
  tickPending = false;
  tickOverrun = false;
  interrupts();

  logCount = 0;
  txFailureCount = 0;
  maxTxDelayUs = 0;
  abortedTicks = 0;
  trialStartUs = micros();
  trialResult = "COMPLETED";
  if (!sendSpeedCommand(0)) {
    finishTrial("START_TX_FAILED", false);
    return;
  }
  saveLogRow(0, 0);
  zeroHold = true;
  trialRunning = true;
}

void handlePendingTick() {
  bool pending;
  bool overrun;
  uint32_t tickUs;
  noInterrupts();
  pending = tickPending;
  tickPending = false;
  overrun = tickOverrun;
  tickOverrun = false;
  tickUs = tickTimestampUs;
  interrupts();

  if (!pending) return;
  if (trialRunning && overrun) {
    ++abortedTicks;
    finishTrial("CONTROL_TICK_OVERRUN", true);
    return;
  }

  if (!trialRunning) {
    if (zeroHold && !sendSpeedCommand(0)) {
      trialResult = "ZERO_HOLD_TX_FAILED";
    }
    return;
  }

  if (!feedbackIsFresh()) {
    finishTrial("FEEDBACK_TIMEOUT", true);
    return;
  }
  if (latestFeedback.errorCode != 0) {
    finishTrial("MOTOR_ERROR", true);
    return;
  }

  const uint32_t elapsedUs = static_cast<uint32_t>(logCount) * CONTROL_PERIOD_US;
  const uint32_t nowUs = micros();
  const uint32_t txDelayUs = static_cast<uint32_t>(nowUs - tickUs);
  if (txDelayUs > maxTxDelayUs) maxTxDelayUs = txDelayUs;
  const int32_t commandErpm = speedCommandAt(elapsedUs);
  if (!sendSpeedCommand(commandErpm)) {
    finishTrial("CAN_TX_FAILED", true);
    return;
  }
  saveLogRow(static_cast<uint32_t>(nowUs - trialStartUs), commandErpm);

  if (elapsedUs >= TEST_DURATION_US) {
    trialRunning = false;
    trialResult = "COMPLETED";
    zeroHold = true;
    reportPending = true;
    reportHeaderPrinted = false;
    reportIndex = 0;
  }
}

void serviceSerialInput() {
  while (Serial.available() > 0) {
    const char input = static_cast<char>(Serial.read());
    if (input == 'g' || input == 'G') {
      startTrial();
    } else if ((input == 'x' || input == 'X') && trialRunning) {
      finishTrial("USER_STOP", true);
    }
  }
}

void serviceReport() {
  if (!reportPending) return;
  if (!reportHeaderPrinted) {
    Serial.print("# result=");
    Serial.print(trialResult);
    Serial.print(",tx_failures=");
    Serial.print(txFailureCount);
    Serial.print(",control_tick_overruns=");
    Serial.print(abortedTicks);
    Serial.print(",max_tx_delay_us=");
    Serial.println(maxTxDelayUs);
    Serial.println("timestamp_us,command_erpm,position_deg,velocity_erpm,current_A,temperature_C,error_code,feedback_timestamp_us,feedback_valid");
    reportHeaderPrinted = true;
    return;
  }
  if (reportIndex < logCount) {
    const LogRow &row = logRows[reportIndex++];
    Serial.print(row.timestampUs); Serial.print(',');
    Serial.print(row.commandErpm); Serial.print(',');
    if (row.feedbackValid) Serial.print(row.positionDeg, 1); else Serial.print("nan");
    Serial.print(',');
    if (row.feedbackValid) Serial.print(row.velocityErpm); else Serial.print("nan");
    Serial.print(',');
    if (row.feedbackValid) Serial.print(row.currentA, 2); else Serial.print("nan");
    Serial.print(',');
    if (row.feedbackValid) Serial.print(row.temperatureC); else Serial.print("nan");
    Serial.print(',');
    if (row.feedbackValid) Serial.print(row.errorCode); else Serial.print("nan");
    Serial.print(',');
    Serial.print(row.feedbackTimestampUs); Serial.print(',');
    Serial.println(row.feedbackValid);
    return;
  }
  reportPending = false;
  Serial.println("Report complete. Press g to run again; x stops an active run.");
}

void setup() {
  Serial.begin(115200);
  Can2.begin();
  Can2.setBaudRate(CAN_BAUD);
  timerReady = controlTimer.begin(controlTickISR, CONTROL_PERIOD_US);
  Serial.println("AK80-8 Servo Mode velocity test ready.");
  Serial.println("Select/configure Servo Mode and confirm feedback; press g to start.");
  Serial.println("Press x during a trial to command zero ERPM and stop.");
  if (!timerReady) Serial.println("ERROR: IntervalTimer could not be started.");
}

void loop() {
  readCanFeedback();
  serviceSerialInput();
  handlePendingTick();
  serviceReport();
}

