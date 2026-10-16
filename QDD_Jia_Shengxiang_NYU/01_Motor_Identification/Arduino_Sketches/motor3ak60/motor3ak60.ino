/*
  AK60-6 V3.0 MIT MODE CHIRP + EXTERNAL PD SYSTEM IDENTIFICATION
  Teensy + CAN1 + 1 Mbps
  Motor ID = 3

  This program mirrors the structure of current_mit_pd(1).ino used for AK80,
  but adapts the CAN protocol and feedback decoding for AK60-6 V3.2.

  Test structure:
    1. Wait for valid periodic AK60 feedback.
    2. Capture the initial motor position as the PD hold position.
    3. Hold that position with an external PD torque controller.
    4. Add a logarithmic chirp torque disturbance.
    5. Log position, velocity, estimated torque, PD torque, chirp torque,
       final command torque, temperature, and error code.
    6. After the chirp and stop delay, command zero torque continuously.

  IMPORTANT BEFORE TEST:
    - AK60 CAN ID must be 3.
    - CAN bitrate must be 1 Mbps.
    - CAN mode must be "periodic feedback".
    - For a 1-100 Hz chirp, set AK60 feedback rate to at least 1000 Hz.
    - Motor must be rigidly fixed before running the identification test.
*/

#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <math.h>

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can0;

// ============================================================
// CAN / Motor Settings
// ============================================================

constexpr uint32_t CAN_BAUD = 1000000;
constexpr uint8_t MOTOR_ID = 3;

// AK60-6 V3.2 MIT ranges
constexpr float P_MIN  = -12.56f;
constexpr float P_MAX  =  12.56f;
constexpr float V_MIN  = -60.0f;
constexpr float V_MAX  =  60.0f;
constexpr float T_MIN  = -12.0f;
constexpr float T_MAX  =  12.0f;
constexpr float KP_MIN =   0.0f;
constexpr float KP_MAX = 500.0f;
constexpr float KD_MIN =   0.0f;
constexpr float KD_MAX =   5.0f;

// AK60-6 V3.0 hardware values.
// Used only to convert periodic-feedback ERPM to output-shaft rad/s.
constexpr float MOTOR_POLE_PAIRS = 14.0f;
constexpr float GEAR_RATIO = 6.0f;

// MIT-mode output torque coefficient from the V3.2 protocol table.
constexpr float TORQUE_CONSTANT_NM_PER_A = 0.5994f;

// Change to -1.0f only if the torque-command direction is opposite to
// the positive position direction observed in feedback.
constexpr float TORQUE_COMMAND_SIGN = 1.0f;

// ============================================================
// Chirp Settings
// ============================================================

constexpr double START_FREQ_HZ = 25.0;
constexpr double END_FREQ_HZ = 150.0;
constexpr double CHIRP_DURATION_SEC = 100.0;

constexpr double START_DELAY_SEC = 2.0;
constexpr double STOP_DELAY_SEC = 2.0;

// Start conservatively for AK60.
// Increase only after a low-amplitude test is confirmed safe.
constexpr float CHIRP_TORQUE_AMPLITUDE_NM = 8.0f;

// ============================================================
// External PD Safety Controller
// ============================================================

// Same concept as the AK80 identification program:
// external PD torque + chirp torque are summed and sent as MIT feedforward torque.
//
// Position error is in degrees.
// Velocity is output-shaft rad/s decoded from the driver's ERPM feedback.
constexpr float KP_PD_NM_PER_DEG = 0.04f;
constexpr float KD_PD_NM_PER_RAD_S = 0.20f;

// Conservative initial total torque limit.
// AK60 protocol range is +/-12 Nm, but the identification test does not need
// anything remotely that heroic on the first run.
constexpr float TORQUE_LIMIT_NM = 10.0f;

// Abort if the motor moves too far away from the captured hold position.
constexpr float MAX_POSITION_DEVIATION_DEG = 100.0f;

// Abort if periodic feedback becomes stale.
constexpr uint32_t FEEDBACK_TIMEOUT_US = 20000;

// Command period = 500 us = 2000 Hz, matching the old AK80 test structure.
constexpr uint32_t COMMAND_PERIOD_US = 333;

// ============================================================
// Runtime Variables
// ============================================================

elapsedMicros txTimer;
elapsedMillis statusTimer;

uint32_t txCount = 0;
uint32_t rxCount = 0;

uint32_t setupTimeUs = 0;

bool feedbackReady = false;
bool isFinished = false;
bool safetyFault = false;

float latestPosDeg = 0.0f;
float latestPosRad = 0.0f;

float latestSpeedErpm = 0.0f;
float latestSpeedRadS = 0.0f;

float latestIqA = 0.0f;
float latestTorqueNm = 0.0f;

float latestTempC = 0.0f;
uint8_t latestError = 0;

uint32_t latestFeedbackTimeUs = 0;

float targetPosDeg = 0.0f;

float pdTorqueNm = 0.0f;
float chirpTorqueNm = 0.0f;
float torqueCommandNm = 0.0f;

// ============================================================
// Utility
// ============================================================

uint16_t float_to_uint(
    float x,
    float x_min,
    float x_max,
    uint8_t bits) {

  x = constrain(x, x_min, x_max);

  const float span = x_max - x_min;
  const uint32_t maxInt = (1UL << bits) - 1UL;

  return static_cast<uint16_t>(
      (x - x_min)
      * static_cast<float>(maxInt)
      / span);
}

float angleErrorDeg(
    float targetDeg,
    float currentDeg) {

  float error = targetDeg - currentDeg;

  while (error > 180.0f) {
    error -= 360.0f;
  }

  while (error < -180.0f) {
    error += 360.0f;
  }

  return error;
}

// Periodic-feedback speed definition:
//   decoded electrical speed = raw * 10 ERPM
//   output rpm = ERPM / pole_pairs / reduction_ratio
//   output rad/s = rpm * 2*pi/60
float erpmToOutputRadS(float erpm) {
  const float outputRpm =
      erpm / MOTOR_POLE_PAIRS / GEAR_RATIO;

  return outputRpm * (2.0f * PI / 60.0f);
}

// ============================================================
// AK60 MIT Command
// ============================================================

bool sendMitCommand(
    float p_des,
    float v_des,
    float kp,
    float kd,
    float t_ff) {

  p_des = constrain(p_des, P_MIN, P_MAX);
  v_des = constrain(v_des, V_MIN, V_MAX);
  kp    = constrain(kp, KP_MIN, KP_MAX);
  kd    = constrain(kd, KD_MIN, KD_MAX);
  t_ff  = constrain(t_ff, T_MIN, T_MAX);

  const uint16_t p_int =
      float_to_uint(p_des, P_MIN, P_MAX, 16);

  const uint16_t v_int =
      float_to_uint(v_des, V_MIN, V_MAX, 12);

  const uint16_t kp_int =
      float_to_uint(kp, KP_MIN, KP_MAX, 12);

  const uint16_t kd_int =
      float_to_uint(kd, KD_MIN, KD_MAX, 12);

  const uint16_t t_int =
      float_to_uint(t_ff, T_MIN, T_MAX, 12);

  CAN_message_t msg{};

  // MIT control-mode ID = 8, drive ID = MOTOR_ID.
  // For MOTOR_ID = 3, extended CAN ID = 0x803.
  msg.id = (8UL << 8) | MOTOR_ID;
  msg.flags.extended = 1;
  msg.flags.remote = 0;
  msg.len = 8;

  // AK60 V3.2 MIT byte order:
  // Kp -> Kd -> Position -> Velocity -> Torque
  msg.buf[0] = static_cast<uint8_t>(kp_int >> 4);
  msg.buf[1] = static_cast<uint8_t>(
      ((kp_int & 0x0F) << 4)
      | ((kd_int >> 8) & 0x0F));
  msg.buf[2] = static_cast<uint8_t>(kd_int & 0xFF);

  msg.buf[3] = static_cast<uint8_t>(p_int >> 8);
  msg.buf[4] = static_cast<uint8_t>(p_int & 0xFF);

  msg.buf[5] = static_cast<uint8_t>(v_int >> 4);
  msg.buf[6] = static_cast<uint8_t>(
      ((v_int & 0x0F) << 4)
      | ((t_int >> 8) & 0x0F));
  msg.buf[7] = static_cast<uint8_t>(t_int & 0xFF);

  const bool ok = Can0.write(msg);

  if (ok) {
    txCount++;
  }

  return ok;
}

void sendZeroTorque() {
  sendMitCommand(
      0.0f,
      0.0f,
      0.0f,
      0.0f,
      0.0f);
}

// ============================================================
// AK60 Periodic Feedback Decode
// ============================================================

void decodeAk60Feedback(const CAN_message_t &msg) {
  if (!msg.flags.extended || msg.len != 8) {
    return;
  }

  const uint32_t functionId = msg.id >> 8;
  const uint8_t driveId =
      static_cast<uint8_t>(msg.id & 0xFF);

  // Periodic motor-status feedback.
  if (functionId != 0x29 || driveId != MOTOR_ID) {
    return;
  }

  const int16_t positionRaw =
      static_cast<int16_t>(
          (static_cast<uint16_t>(msg.buf[0]) << 8)
          | static_cast<uint16_t>(msg.buf[1]));

  const int16_t speedRaw =
      static_cast<int16_t>(
          (static_cast<uint16_t>(msg.buf[2]) << 8)
          | static_cast<uint16_t>(msg.buf[3]));

  const int16_t currentRaw =
      static_cast<int16_t>(
          (static_cast<uint16_t>(msg.buf[4]) << 8)
          | static_cast<uint16_t>(msg.buf[5]));

  latestPosDeg =
      static_cast<float>(positionRaw) * 0.1f;

  latestPosRad =
      latestPosDeg * PI / 180.0f;

  latestSpeedErpm =
      static_cast<float>(speedRaw) * 10.0f;

  latestSpeedRadS =
      erpmToOutputRadS(latestSpeedErpm);

  latestIqA =
      static_cast<float>(currentRaw) * 0.01f;

  latestTorqueNm =
      TORQUE_CONSTANT_NM_PER_A * latestIqA;

  latestTempC =
      static_cast<float>(
          static_cast<int8_t>(msg.buf[6]));

  latestError = msg.buf[7];

  latestFeedbackTimeUs = micros();
  rxCount++;

  if (!feedbackReady) {
    targetPosDeg = latestPosDeg;
    setupTimeUs = micros();
    feedbackReady = true;
  }

  Serial.print("DATA,");
  Serial.print(millis());
  Serial.print(",");

  Serial.print(latestPosDeg, 4);
  Serial.print(",");

  Serial.print(latestSpeedRadS, 5);
  Serial.print(",");

  Serial.print(latestTorqueNm, 5);
  Serial.print(",");

  Serial.print(latestIqA, 5);
  Serial.print(",");

  Serial.print(pdTorqueNm, 5);
  Serial.print(",");

  Serial.print(chirpTorqueNm, 5);
  Serial.print(",");

  Serial.print(torqueCommandNm, 5);
  Serial.print(",");

  Serial.print(latestTempC, 1);
  Serial.print(",");

  Serial.println(latestError);
}

void readCANMessages() {
  CAN_message_t rxmsg{};

  while (Can0.read(rxmsg)) {
    decodeAk60Feedback(rxmsg);
  }
}

// ============================================================
// Safety
// ============================================================

bool feedbackIsFresh() {
  if (!feedbackReady) {
    return false;
  }

  return (micros() - latestFeedbackTimeUs)
      <= FEEDBACK_TIMEOUT_US;
}

void triggerSafetyFault(const char *reason) {
  if (!safetyFault) {
    Serial.print("FAULT,");
    Serial.println(reason);
  }

  safetyFault = true;
  chirpTorqueNm = 0.0f;
  pdTorqueNm = 0.0f;
  torqueCommandNm = 0.0f;
}

bool checkSafety() {
  if (!feedbackReady) {
    return false;
  }

  if (!feedbackIsFresh()) {
    triggerSafetyFault("FEEDBACK_TIMEOUT");
    return false;
  }

  if (latestError != 0) {
    triggerSafetyFault("MOTOR_ERROR");
    return false;
  }

  const float deviationDeg =
      fabsf(angleErrorDeg(
          targetPosDeg,
          latestPosDeg));

  if (deviationDeg > MAX_POSITION_DEVIATION_DEG) {
    triggerSafetyFault("POSITION_DEVIATION");
    return false;
  }

  return true;
}

// ============================================================
// Setup
// ============================================================

void setup() {
  Serial.begin(115200);

  delay(1500);

  Serial.println("==============================================");
  Serial.println("AK60-6 MIT MODE CHIRP + EXTERNAL PD LOGGER");
  Serial.println("==============================================");

  Serial.println("CSV columns:");
  Serial.println(
      "Time_ms,"
      "Position_deg,"
      "Speed_rad_s,"
      "Estimated_Torque_Nm,"
      "Iq_A,"
      "PD_Torque_Nm,"
      "Chirp_Torque_Nm,"
      "Command_Torque_Nm,"
      "Temperature_C,"
      "Error_Code");

  Serial.println();
  Serial.println("Waiting for AK60 periodic feedback...");

  Can0.begin();
  Can0.setBaudRate(CAN_BAUD);

  delay(500);

  // AK60 V3.2 MIT protocol does not use the old AK80
  // FF FF FF FF FF FF FF FC entry frame.
  //
  // Send zero torque while waiting for feedback.
  sendZeroTorque();

  setupTimeUs = micros();
}

// ============================================================
// Loop
// ============================================================

void loop() {
  Can0.events();
  readCANMessages();

  if (txTimer >= COMMAND_PERIOD_US) {
    txTimer -= COMMAND_PERIOD_US;

    if (!feedbackReady) {
      sendZeroTorque();
      return;
    }

    if (!checkSafety()) {
      sendZeroTorque();
      return;
    }

    const uint32_t nowUs = micros();

    const float totalTimeSec =
        static_cast<float>(
            nowUs - setupTimeUs)
        * 1.0e-6f;

    const float chirpTimeSec =
        totalTimeSec
        - static_cast<float>(START_DELAY_SEC);

    // --------------------------------------------------------
    // Chirp generation
    // --------------------------------------------------------

    if (chirpTimeSec >= 0.0f
        && chirpTimeSec <= CHIRP_DURATION_SEC) {

      const double k =
          pow(
              END_FREQ_HZ / START_FREQ_HZ,
              1.0 / CHIRP_DURATION_SEC);

      const double phase =
          2.0
          * PI
          * START_FREQ_HZ
          * (pow(k, chirpTimeSec) - 1.0)
          / log(k);

      chirpTorqueNm =
          CHIRP_TORQUE_AMPLITUDE_NM
          * sin(phase);

    } else {
      chirpTorqueNm = 0.0f;

      if (chirpTimeSec > CHIRP_DURATION_SEC) {
        isFinished = true;
      }
    }

    // --------------------------------------------------------
    // External PD position-hold torque
    // --------------------------------------------------------

    const float positionErrorDeg =
        angleErrorDeg(
            targetPosDeg,
            latestPosDeg);

    pdTorqueNm =
        KP_PD_NM_PER_DEG
        * positionErrorDeg
        - KD_PD_NM_PER_RAD_S
        * latestSpeedRadS;

    // --------------------------------------------------------
    // Final torque command
    // --------------------------------------------------------

    torqueCommandNm =
        pdTorqueNm
        + chirpTorqueNm;

    torqueCommandNm *= TORQUE_COMMAND_SIGN;

    torqueCommandNm =
        constrain(
            torqueCommandNm,
            -TORQUE_LIMIT_NM,
             TORQUE_LIMIT_NM);

    // External PD + chirp are sent entirely through torque feedforward.
    // AK60 internal MIT Kp/Kd remain zero, matching the AK80 identification
    // structure as closely as practical.
    sendMitCommand(
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        torqueCommandNm);

    // --------------------------------------------------------
    // Automatic end of test
    // --------------------------------------------------------

    if (chirpTimeSec
        > CHIRP_DURATION_SEC
        + STOP_DELAY_SEC) {

      chirpTorqueNm = 0.0f;
      pdTorqueNm = 0.0f;
      torqueCommandNm = 0.0f;

      for (int i = 0; i < 500; ++i) {
        sendZeroTorque();
        delay(1);
      }

      Serial.println("TEST_FINISHED");

      while (true) {
        Can0.events();
        readCANMessages();
        sendZeroTorque();
        delay(10);
      }
    }
  }

  if (statusTimer >= 1000) {
    statusTimer = 0;

    Serial.print("LOG: txCount=");
    Serial.print(txCount);

    Serial.print(" rxCount=");
    Serial.print(rxCount);

    Serial.print(" feedbackAge_us=");

    if (feedbackReady) {
      Serial.print(
          micros() - latestFeedbackTimeUs);
    } else {
      Serial.print("NA");
    }

    Serial.print(" finished=");
    Serial.print(isFinished ? 1 : 0);

    Serial.print(" safetyFault=");
    Serial.println(safetyFault ? 1 : 0);
  }
}
