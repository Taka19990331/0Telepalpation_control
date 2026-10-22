/*
 * AK80 MIT torque-mode validation against a separately logged Leptrino sensor.
 *
 * The uploaded discrete position controller is retained as a holding
 * controller. A logarithmic torque chirp is injected after the controller:
 *
 *   tau_command = tau_position_hold + tau_chirp
 *
 * The sketch logs the MIT internal torque feedback. The Leptrino LGR software
 * logs the external Mx/My/Mz torque in a separate CSV. MATLAB aligns the two
 * clocks using either the LGR101U/IO DI channel or correlation.
 */

#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <math.h>

enum TestState : uint8_t {
  STATE_IDLE = 0,
  STATE_INTERNAL_ZEROING = 1,
  STATE_START_DELAY = 2,
  STATE_CHIRP = 3,
  STATE_POST_ZERO = 4,
  STATE_DONE = 5,
  STATE_ABORTED = 6
};

const char *stateName(TestState state);
void enterState(TestState newState);
void abortTest(const char *reason);

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can1;

// ============================================================
// 1. USER SETTINGS
// ============================================================

const uint8_t MOTOR_ID = 2;
const uint32_t CAN_BAUD = 1000000;
const uint32_t SERIAL_BAUD = 921600;

// Existing discrete position controller from the uploaded sketch.
// Designed for approximately Ts = 0.0003 s.
const float CPOS_B0 = 216.2607249f;
const float CPOS_B1 = -429.7119067f;
const float CPOS_B2 = 213.4633874f;
const float CPOS_A1 = -1.929761384f;
const float CPOS_A2 = 0.9297613839f;

const uint32_t CONTROL_INTERVAL_US = 333;   // approximately 3 kHz
const uint32_t LOG_INTERVAL_US = 2000;      // 500 Hz motor CSV

const float INTERNAL_ZERO_TIME_S = 2.0f;
const float START_DELAY_S = 2.0f;
const float CHIRP_DURATION_S = 20.0f;
const float POST_ZERO_TIME_S = 2.0f;

// Chirp is a torque command in N*m, not current.
const float CHIRP_START_FREQUENCY_HZ = 1.0f;
const float CHIRP_END_FREQUENCY_HZ = 20.0f;
const float CHIRP_TORQUE_AMPLITUDE_NM = 2.5f;

// Holding controller and total command limits.
const float MAX_POSITION_TORQUE_NM = 4.00f;
const float MAX_TOTAL_COMMAND_TORQUE_NM = 4.75f;

// Change to -1 if MIT torque feedback uses the opposite sign convention.
const float INTERNAL_TORQUE_SIGN = 1.0f;

// Synchronization output. It is asserted for the complete chirp interval.
// For LGR101U/IO, do not connect this pin directly until the DI voltage,
// polarity, grounding, and isolation requirements have been verified.
const uint8_t SYNC_OUTPUT_PIN = 6;
const uint8_t SYNC_ACTIVE_LEVEL = HIGH;
const uint8_t SYNC_INACTIVE_LEVEL = LOW;

// Protection.
const float MAX_POSITION_ERROR_DEG = 10.0f;
const float MAX_ABSOLUTE_SPEED_RAD_S = 8.0f;
const float MAX_INTERNAL_TORQUE_FEEDBACK_NM = 8.0f;
const float MAX_TEMPERATURE_C = 10000.0f;
const uint32_t FEEDBACK_TIMEOUT_US = 50000;

const bool SET_MOTOR_ZERO_ON_BOOT = false;
const bool AUTO_START = true;

// After the first valid MIT feedback frame, wait before beginning the test.
// Start Leptrino logging before powering/resetting the Teensy.
const uint32_t AUTO_START_DELAY_MS = 5000;

// ============================================================
// 2. MIT MODE RANGES
// ============================================================

const float P_MIN = -12.5f;
const float P_MAX = 12.5f;
const float V_MIN = -37.5f;
const float V_MAX = 37.5f;
const float KP_MIN = 0.0f;
const float KP_MAX = 500.0f;
const float KD_MIN = 0.0f;
const float KD_MAX = 5.0f;
const float T_MIN = -32.0f;
const float T_MAX = 32.0f;

const float RAD2DEG = 57.2957795131f;

// ============================================================
// 3. STATE AND SIGNALS
// ============================================================

elapsedMicros controlTimerUs;
elapsedMicros logTimerUs;
elapsedMillis heartbeatTimerMs;

TestState testState = STATE_IDLE;
uint32_t stateStartUs = 0;
uint32_t chirpStartUs = 0;
uint32_t lastFeedbackUs = 0;

bool feedbackReady = false;
bool commandSaturated = false;

bool autoStartCountdownActive = false;
bool autoStartConsumed = false;
uint32_t autoStartCountdownMs = 0;

float latestPosRawRad = 0.0f;
float latestPosRad = 0.0f;
float lastPosRawRad = 0.0f;
float latestSpeedRadS = 0.0f;
float latestTorqueRawNm = 0.0f;
float latestTempC = 0.0f;
uint8_t latestErrorCode = 0;

float holdPositionRad = 0.0f;
float positionErrorRad = 0.0f;
float positionTorqueNm = 0.0f;
float chirpTorqueNm = 0.0f;
float totalTorqueCommandNm = 0.0f;
float instantaneousFrequencyHz = 0.0f;

float internalTorqueBiasNm = 0.0f;
float internalTorqueZeroedNm = 0.0f;
double internalZeroTorqueSum = 0.0;
uint32_t internalZeroCount = 0;

uint32_t txCount = 0;
uint32_t rxCount = 0;

String serialCommand;

// ============================================================
// 4. SECOND-ORDER IIR POSITION CONTROLLER
// ============================================================

struct IIR2 {
  float b0, b1, b2;
  float a1, a2;
  float x1, x2;
  float y1, y2;
};

IIR2 cpos;

void resetIIR(IIR2 &f) {
  f.x1 = 0.0f;
  f.x2 = 0.0f;
  f.y1 = 0.0f;
  f.y2 = 0.0f;
}

float runIIR(IIR2 &f, float x) {
  float y = f.b0 * x + f.b1 * f.x1 + f.b2 * f.x2
          - f.a1 * f.y1 - f.a2 * f.y2;

  f.x2 = f.x1;
  f.x1 = x;
  f.y2 = f.y1;
  f.y1 = y;
  return y;
}

void setupPositionController() {
  cpos.b0 = CPOS_B0;
  cpos.b1 = CPOS_B1;
  cpos.b2 = CPOS_B2;
  cpos.a1 = CPOS_A1;
  cpos.a2 = CPOS_A2;
  resetIIR(cpos);
}

// ============================================================
// 5. MIT CAN HELPERS AND FEEDBACK
// ============================================================

uint16_t floatToUint(float x, float xMin, float xMax, int bits) {
  float xConstrained = constrain(x, xMin, xMax);
  return (uint16_t)((xConstrained - xMin)
      * ((float)((1UL << bits) - 1)) / (xMax - xMin));
}

float uintToFloat(uint16_t xInt, float xMin, float xMax, int bits) {
  return ((float)xInt) * (xMax - xMin)
      / ((float)((1UL << bits) - 1)) + xMin;
}

void sendMitCommand(float p, float v, float kp, float kd, float tau) {
  uint16_t pInt = floatToUint(p, P_MIN, P_MAX, 16);
  uint16_t vInt = floatToUint(v, V_MIN, V_MAX, 12);
  uint16_t kpInt = floatToUint(kp, KP_MIN, KP_MAX, 12);
  uint16_t kdInt = floatToUint(kd, KD_MIN, KD_MAX, 12);
  uint16_t tInt = floatToUint(tau, T_MIN, T_MAX, 12);

  CAN_message_t msg;
  msg.id = MOTOR_ID;
  msg.len = 8;
  msg.buf[0] = pInt >> 8;
  msg.buf[1] = pInt & 0xFF;
  msg.buf[2] = vInt >> 4;
  msg.buf[3] = ((vInt & 0xF) << 4) | (kpInt >> 8);
  msg.buf[4] = kpInt & 0xFF;
  msg.buf[5] = kdInt >> 4;
  msg.buf[6] = ((kdInt & 0xF) << 4) | (tInt >> 8);
  msg.buf[7] = tInt & 0xFF;

  Can1.write(msg);
  txCount++;
}

void sendSpecialCommand(uint8_t lastByte) {
  CAN_message_t msg;
  msg.id = MOTOR_ID;
  msg.len = 8;
  for (int i = 0; i < 7; i++) {
    msg.buf[i] = 0xFF;
  }
  msg.buf[7] = lastByte;
  Can1.write(msg);
}

void enterMitMode() {
  sendSpecialCommand(0xFC);
}

void zeroMotor() {
  sendSpecialCommand(0xFE);
}

void updateUnwrappedPosition(float rawRad) {
  const float wrap = P_MAX - P_MIN;

  if (!feedbackReady) {
    latestPosRad = rawRad;
    lastPosRawRad = rawRad;
    feedbackReady = true;
    return;
  }

  float difference = rawRad - lastPosRawRad;

  if (difference > wrap / 2.0f) {
    difference -= wrap;
  }
  if (difference < -wrap / 2.0f) {
    difference += wrap;
  }

  latestPosRad += difference;
  lastPosRawRad = rawRad;
}

void parseFeedback(const CAN_message_t &msg) {
  if (msg.len < 6) {
    return;
  }

  if (msg.id != MOTOR_ID && msg.buf[0] != MOTOR_ID) {
    return;
  }

  uint16_t pInt = ((uint16_t)msg.buf[1] << 8) | msg.buf[2];
  uint16_t vInt = ((uint16_t)msg.buf[3] << 4)
                | (msg.buf[4] >> 4);
  uint16_t tInt = (((uint16_t)msg.buf[4] & 0x0F) << 8)
                | msg.buf[5];

  latestPosRawRad = uintToFloat(pInt, P_MIN, P_MAX, 16);
  latestSpeedRadS = uintToFloat(vInt, V_MIN, V_MAX, 12);
  latestTorqueRawNm = uintToFloat(tInt, T_MIN, T_MAX, 12);

  if (msg.len >= 7) {
    latestTempC = (float)msg.buf[6];
  }
  if (msg.len >= 8) {
    latestErrorCode = msg.buf[7];
  }

  updateUnwrappedPosition(latestPosRawRad);
  lastFeedbackUs = micros();
  rxCount++;
}

void readCanMessages() {
  CAN_message_t msg;
  while (Can1.read(msg)) {
    parseFeedback(msg);
  }
}

// ============================================================
// 6. TEST STATE MACHINE AND CHIRP
// ============================================================

float elapsedStateSeconds() {
  return (float)((uint32_t)(micros() - stateStartUs)) * 1.0e-6f;
}

float elapsedChirpSeconds() {
  return (float)((uint32_t)(micros() - chirpStartUs)) * 1.0e-6f;
}

const char *stateName(TestState state) {
  switch (state) {
    case STATE_IDLE: return "IDLE";
    case STATE_INTERNAL_ZEROING: return "INTERNAL_ZEROING";
    case STATE_START_DELAY: return "START_DELAY";
    case STATE_CHIRP: return "CHIRP";
    case STATE_POST_ZERO: return "POST_ZERO";
    case STATE_DONE: return "DONE";
    case STATE_ABORTED: return "ABORTED";
  }
  return "UNKNOWN";
}

void enterState(TestState newState) {
  testState = newState;
  stateStartUs = micros();

  Serial.print("EVENT,STATE,");
  Serial.print((uint8_t)testState);
  Serial.print(",");
  Serial.println(stateName(testState));
}

void setSyncActive(bool active) {
  digitalWrite(
      SYNC_OUTPUT_PIN,
      active ? SYNC_ACTIVE_LEVEL : SYNC_INACTIVE_LEVEL
  );
}

void setZeroTorqueCommand() {
  positionTorqueNm = 0.0f;
  chirpTorqueNm = 0.0f;
  totalTorqueCommandNm = 0.0f;
  instantaneousFrequencyHz = 0.0f;
  commandSaturated = false;
  sendMitCommand(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}

float calculateLogChirpTorque(float timeS, float &frequencyHz) {
  const float f0 = CHIRP_START_FREQUENCY_HZ;
  const float f1 = CHIRP_END_FREQUENCY_HZ;
  const float duration = CHIRP_DURATION_S;

  float clampedTime = constrain(timeS, 0.0f, duration);

  if (fabsf(f1 - f0) < 1.0e-6f) {
    frequencyHz = f0;
    return CHIRP_TORQUE_AMPLITUDE_NM
        * sinf(2.0f * PI * f0 * clampedTime);
  }

  float ratio = f1 / f0;
  float ratioPower = powf(ratio, clampedTime / duration);
  frequencyHz = f0 * ratioPower;

  float phaseRad = 2.0f * PI * f0 * duration
      * (ratioPower - 1.0f) / logf(ratio);

  return CHIRP_TORQUE_AMPLITUDE_NM * sinf(phaseRad);
}

void updateHoldingTorque() {
  positionErrorRad = holdPositionRad - latestPosRad;
  positionTorqueNm = runIIR(cpos, positionErrorRad);
  positionTorqueNm = constrain(
      positionTorqueNm,
      -MAX_POSITION_TORQUE_NM,
       MAX_POSITION_TORQUE_NM
  );
}

void beginTest() {
  if (!feedbackReady) {
    Serial.println("EVENT,START_REJECTED,NO_FEEDBACK");
    return;
  }

  if (latestErrorCode != 0) {
    Serial.println("EVENT,START_REJECTED,MOTOR_ERROR");
    return;
  }

  resetIIR(cpos);
  internalZeroTorqueSum = 0.0;
  internalZeroCount = 0;
  internalTorqueBiasNm = 0.0f;
  internalTorqueZeroedNm = 0.0f;
  positionErrorRad = 0.0f;
  setSyncActive(false);

  enterState(STATE_INTERNAL_ZEROING);
  Serial.println("EVENT,TEST_STARTED");
}

void abortTest(const char *reason) {
  setZeroTorqueCommand();
  setSyncActive(false);
  testState = STATE_ABORTED;
  stateStartUs = micros();

  Serial.print("EVENT,ABORT,");
  Serial.println(reason);
}

void stopTest() {
  setZeroTorqueCommand();
  setSyncActive(false);
  enterState(STATE_IDLE);
  Serial.println("EVENT,STOPPED");
}

void runSafetyChecks() {
  bool active = testState == STATE_INTERNAL_ZEROING
      || testState == STATE_START_DELAY
      || testState == STATE_CHIRP
      || testState == STATE_POST_ZERO;

  if (!active) {
    return;
  }

  if (!feedbackReady) {
    abortTest("NO_FEEDBACK");
    return;
  }

  if ((uint32_t)(micros() - lastFeedbackUs) > FEEDBACK_TIMEOUT_US) {
    abortTest("FEEDBACK_TIMEOUT");
    return;
  }

  if (latestErrorCode != 0) {
    abortTest("MOTOR_ERROR");
    return;
  }

  if (latestTempC >= MAX_TEMPERATURE_C) {
    abortTest("OVER_TEMPERATURE");
    return;
  }

  if (fabsf(latestSpeedRadS) > MAX_ABSOLUTE_SPEED_RAD_S) {
    abortTest("SPEED_LIMIT");
    return;
  }

  if (fabsf(positionErrorRad * RAD2DEG) > MAX_POSITION_ERROR_DEG) {
    abortTest("POSITION_ERROR_LIMIT");
    return;
  }

  if (fabsf(internalTorqueZeroedNm)
      > MAX_INTERNAL_TORQUE_FEEDBACK_NM) {
    abortTest("INTERNAL_TORQUE_LIMIT");
    return;
  }
}

void updateControl() {
  internalTorqueZeroedNm = INTERNAL_TORQUE_SIGN
      * (latestTorqueRawNm - internalTorqueBiasNm);

  float stateTimeS = elapsedStateSeconds();

  switch (testState) {
    case STATE_IDLE:
    case STATE_DONE:
    case STATE_ABORTED:
      setZeroTorqueCommand();
      break;

    case STATE_INTERNAL_ZEROING:
      setZeroTorqueCommand();
      internalZeroTorqueSum += latestTorqueRawNm;
      internalZeroCount++;

      if (stateTimeS >= INTERNAL_ZERO_TIME_S
          && internalZeroCount > 100) {
        internalTorqueBiasNm = (float)(
            internalZeroTorqueSum / (double)internalZeroCount
        );

        holdPositionRad = latestPosRad;
        positionErrorRad = 0.0f;
        resetIIR(cpos);

        Serial.print("EVENT,INTERNAL_ZERO_NM,");
        Serial.println(internalTorqueBiasNm, 7);
        Serial.print("EVENT,HOLD_POSITION_RAD,");
        Serial.println(holdPositionRad, 7);

        enterState(STATE_START_DELAY);
      }
      break;

    case STATE_START_DELAY:
      updateHoldingTorque();
      chirpTorqueNm = 0.0f;
      instantaneousFrequencyHz = 0.0f;
      totalTorqueCommandNm = positionTorqueNm;

      sendMitCommand(
          0.0f, 0.0f, 0.0f, 0.0f, totalTorqueCommandNm
      );

      if (stateTimeS >= START_DELAY_S) {
        chirpStartUs = micros();
        setSyncActive(true);
        enterState(STATE_CHIRP);
        Serial.println("EVENT,CHIRP_STARTED");
      }
      break;

    case STATE_CHIRP: {
      updateHoldingTorque();

      float chirpTimeS = elapsedChirpSeconds();
      chirpTorqueNm = calculateLogChirpTorque(
          chirpTimeS,
          instantaneousFrequencyHz
      );

      float unconstrainedCommand = positionTorqueNm + chirpTorqueNm;

      totalTorqueCommandNm = constrain(
          unconstrainedCommand,
          -MAX_TOTAL_COMMAND_TORQUE_NM,
           MAX_TOTAL_COMMAND_TORQUE_NM
      );

      commandSaturated = fabsf(
          totalTorqueCommandNm - unconstrainedCommand
      ) > 1.0e-5f;

      sendMitCommand(
          0.0f, 0.0f, 0.0f, 0.0f, totalTorqueCommandNm
      );

      if (chirpTimeS >= CHIRP_DURATION_S) {
        setSyncActive(false);
        chirpTorqueNm = 0.0f;
        instantaneousFrequencyHz = 0.0f;
        enterState(STATE_POST_ZERO);
        Serial.println("EVENT,CHIRP_FINISHED");
      }
      break;
    }

    case STATE_POST_ZERO:
      updateHoldingTorque();
      chirpTorqueNm = 0.0f;
      instantaneousFrequencyHz = 0.0f;
      totalTorqueCommandNm = positionTorqueNm;

      sendMitCommand(
          0.0f, 0.0f, 0.0f, 0.0f, totalTorqueCommandNm
      );

      if (stateTimeS >= POST_ZERO_TIME_S) {
        setZeroTorqueCommand();
        enterState(STATE_DONE);
        Serial.println("EVENT,TEST_DONE");
      }
      break;
  }
}

// ============================================================
// 7. SERIAL COMMANDS AND MOTOR CSV
// ============================================================

void processSerialCommands() {
  while (Serial.available() > 0) {
    char received = (char)Serial.read();

    if (received == '\n' || received == '\r') {
      serialCommand.trim();
      serialCommand.toUpperCase();

      if (serialCommand == "START") {
        beginTest();
      } else if (serialCommand == "STOP"
          || serialCommand == "ABORT") {
        stopTest();
      } else if (serialCommand == "STATUS") {
        Serial.print("EVENT,STATUS,");
        Serial.print((uint8_t)testState);
        Serial.print(",");
        Serial.print(stateName(testState));
        Serial.print(",FEEDBACK,");
        Serial.print(feedbackReady ? 1 : 0);
        Serial.print(",ERROR,");
        Serial.println(latestErrorCode);
      }

      serialCommand = "";
    } else {
      serialCommand += received;
      if (serialCommand.length() > 32) {
        serialCommand = "";
      }
    }
  }
}

void printCsvHeader() {
  Serial.println(
      "Device_Time_us,Test_Time_s,Test_State,"
      "Instantaneous_Frequency_Hz,"
      "Position_Reference_deg,Position_Output_deg,Position_Error_deg,"
      "Speed_rad_s,Position_Control_Torque_Nm,"
      "Chirp_Torque_Command_Nm,Total_Torque_Command_Nm,"
      "Internal_Torque_Raw_Nm,Internal_Torque_Zeroed_Nm,"
      "Temperature_C,Error_Code,Command_Saturated,Sync_Output"
  );
}

void printDataRow() {
  if (!feedbackReady) {
    return;
  }

  float testTimeS = 0.0f;
  if (testState == STATE_CHIRP) {
    testTimeS = elapsedChirpSeconds();
  }

  float positionOutputDeg = 0.0f;
  if (testState != STATE_IDLE) {
    positionOutputDeg = (latestPosRad - holdPositionRad) * RAD2DEG;
  }

  Serial.print("DATA,");
  Serial.print(micros()); Serial.print(",");
  Serial.print(testTimeS, 7); Serial.print(",");
  Serial.print((uint8_t)testState); Serial.print(",");
  Serial.print(instantaneousFrequencyHz, 7); Serial.print(",");
  Serial.print(0.0f, 7); Serial.print(",");
  Serial.print(positionOutputDeg, 7); Serial.print(",");
  Serial.print(positionErrorRad * RAD2DEG, 7); Serial.print(",");
  Serial.print(latestSpeedRadS, 7); Serial.print(",");
  Serial.print(positionTorqueNm, 7); Serial.print(",");
  Serial.print(chirpTorqueNm, 7); Serial.print(",");
  Serial.print(totalTorqueCommandNm, 7); Serial.print(",");
  Serial.print(latestTorqueRawNm, 7); Serial.print(",");
  Serial.print(internalTorqueZeroedNm, 7); Serial.print(",");
  Serial.print(latestTempC, 2); Serial.print(",");
  Serial.print(latestErrorCode); Serial.print(",");
  Serial.print(commandSaturated ? 1 : 0); Serial.print(",");
  Serial.println(testState == STATE_CHIRP ? 1 : 0);
}

// ============================================================
// 8. SETUP AND LOOP
// ============================================================

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1000);

  pinMode(SYNC_OUTPUT_PIN, OUTPUT);
  setSyncActive(false);

  setupPositionController();

  Can1.begin();
  Can1.setBaudRate(CAN_BAUD);

  enterMitMode();
  delay(100);

  if (SET_MOTOR_ZERO_ON_BOOT) {
    zeroMotor();
    delay(100);
  }

  printCsvHeader();
  Serial.println("EVENT,READY");
  Serial.print("EVENT,AUTO_START,");
  Serial.println(AUTO_START ? 1 : 0);

  if (AUTO_START) {
    Serial.print("EVENT,INSTRUCTION,Start Leptrino logging before reset/power-up; test begins ");
    Serial.print(AUTO_START_DELAY_MS);
    Serial.println(" ms after valid MIT feedback");
  } else {
    Serial.println(
        "EVENT,INSTRUCTION,Start Leptrino logging and then send START"
    );
  }
}

void loop() {
  readCanMessages();
  processSerialCommands();

  // One-shot automatic start.
  // The countdown begins only after valid MIT feedback is available.
  if (AUTO_START
      && !autoStartConsumed
      && testState == STATE_IDLE
      && feedbackReady) {

    if (!autoStartCountdownActive) {
      autoStartCountdownActive = true;
      autoStartCountdownMs = millis();

      Serial.print("EVENT,AUTO_START_COUNTDOWN_MS,");
      Serial.println(AUTO_START_DELAY_MS);
    }

    if ((uint32_t)(millis() - autoStartCountdownMs)
        >= AUTO_START_DELAY_MS) {
      autoStartConsumed = true;
      autoStartCountdownActive = false;
      beginTest();
    }
  }

  if (controlTimerUs >= CONTROL_INTERVAL_US) {
    controlTimerUs -= CONTROL_INTERVAL_US;
    updateControl();
    runSafetyChecks();
  }

  if (logTimerUs >= LOG_INTERVAL_US) {
    logTimerUs -= LOG_INTERVAL_US;
    printDataRow();
  }

  if (heartbeatTimerMs >= 1000) {
    heartbeatTimerMs = 0;
    Serial.print("EVENT,HEARTBEAT,");
    Serial.print((uint8_t)testState);
    Serial.print(",");
    Serial.print(stateName(testState));
    Serial.print(",");
    Serial.print(txCount);
    Serial.print(",");
    Serial.println(rxCount);
  }
}
