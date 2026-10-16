/*
  AK60-6 TORQUE-RAMP FRICTION IDENTIFICATION
  Teensy + CAN1 + 1 Mbps
  Motor ID = 3

  Reference idea:
      i_ref(t) = a_i * t

  Torque-domain equivalent used here:
      tau_ref(t) = a_tau * t
      a_tau = Kt * a_i

  For AK60 this version uses torque-ramp slopes chosen from the
  motor's measured friction scale:

      ±0.05, ±0.10, ±0.20, ±0.30, ±0.50 Nm/s

  with a torque-ramp limit of 0.30 Nm.

  Each slope is repeated 3 times.

  IMPORTANT:
    - Direct MIT feedforward torque ramp.
    - No software feedback-timeout/current/speed FAULT latches.
    - No position controller.
    - No Cpos.
    - No DOB/RFOB.
    - No GIMP.
    - No friction compensation.
    - Speed and torque are logged sample-by-sample at 250 Hz.
    - Measured torque = Kt * measured Iq from AK60 feedback.

  The raw friction residual is also logged:
      tau_f_raw = Kt*Iq - J*domega/dt

  For final analysis, filter speed and differentiate offline.
*/

#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <math.h>

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can0;

// ============================================================
// CAN / Motor
// ============================================================

constexpr uint32_t CAN_BAUD = 1000000;
constexpr uint8_t MOTOR_ID = 3;

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

constexpr float MOTOR_POLE_PAIRS = 14.0f;
constexpr float GEAR_RATIO = 6.0f;

constexpr float TORQUE_CONSTANT_NM_PER_A = 0.5994f;
constexpr float IDENTIFIED_INERTIA_KG_M2 = 0.0042243000f;

constexpr float TORQUE_COMMAND_SIGN = 1.0f;

// ============================================================
// Torque-ramp experiment
// ============================================================

// Exact torque-domain equivalents of:
// ±0.5, ±1.0, ±1.5, ±2.0, ±2.5 A/s
constexpr float TORQUE_RAMP_SLOPES_NM_PER_S[] = {
   0.05f, -0.05f,
   0.10f, -0.10f,
   0.20f, -0.20f,
   0.30f, -0.30f,
   0.50f, -0.50f
};

constexpr int NUM_RAMP_SLOPES =
    sizeof(TORQUE_RAMP_SLOPES_NM_PER_S)
    / sizeof(TORQUE_RAMP_SLOPES_NM_PER_S[0]);

constexpr int REPEATS_PER_SLOPE = 3;

// Same physical maximum as 1.0 A in the current-ramp version.
constexpr float TORQUE_RAMP_LIMIT_NM = 0.30f;

constexpr float START_DELAY_SEC = 2.0f;
constexpr float PRE_RAMP_REST_SEC = 1.0f;
constexpr float POST_RAMP_REST_SEC = 1.5f;

// ============================================================
// Timing
// ============================================================

constexpr uint32_t COMMAND_PERIOD_US = 333;
constexpr uint32_t LOG_PERIOD_US = 4000;       // 250 Hz
constexpr uint32_t FEEDBACK_TIMEOUT_US = 200000;  // 200 ms

// ============================================================
// Experimental stopping condition
// ============================================================
//
// No software FAULT logic is used in this version.
//
// The only automatic stop inside a ramp is the NORMAL trial-end
// speed threshold below. Reaching it is NOT a fault; the current
// trial simply ends and enters the rest phase.
//
// Motor Error_Code is still logged, but it does not latch the
// experiment into PHASE_FAULT.
//

constexpr float TRIAL_END_SPEED_RAD_S = 15.0f;

// ============================================================
// State machine
// ============================================================

enum TestPhase : uint8_t {
  PHASE_WAIT_START = 0,
  PHASE_PRE_REST   = 1,
  PHASE_RAMP       = 2,
  PHASE_POST_REST  = 3,
  PHASE_FINISHED   = 4
};

TestPhase phase = PHASE_WAIT_START;

int slopeIndex = 0;
int repeatIndex = 0;

bool feedbackReady = false;
bool testFinished = false;

uint32_t firstFeedbackUs = 0;
uint32_t phaseStartUs = 0;

// ============================================================
// Runtime signals
// ============================================================

elapsedMicros txTimer;
elapsedMicros logTimer;
elapsedMillis statusTimer;

uint32_t txCount = 0;
uint32_t rxCount = 0;

float latestPosDeg = 0.0f;

float latestSpeedErpm = 0.0f;
float latestSpeedRadS = 0.0f;

float latestIqA = 0.0f;
float latestMeasuredTorqueNm = 0.0f;

float latestTempC = 0.0f;
uint8_t latestError = 0;

uint32_t latestFeedbackTimeUs = 0;

float torqueRampSlopeNmPerS = 0.0f;
float torqueReferenceNm = 0.0f;
float torqueCommandNm = 0.0f;

// Diagnostic acceleration only
bool accelReady = false;
uint32_t previousSpeedTimeUs = 0;
float previousSpeedRadS = 0.0f;

float rawAccelerationRadS2 = 0.0f;
float inertiaTorqueNm = 0.0f;
float rawFrictionResidualNm = 0.0f;

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

float erpmToOutputRadS(float erpm) {
  const float outputRpm =
      erpm / MOTOR_POLE_PAIRS / GEAR_RATIO;

  return outputRpm * (2.0f * PI / 60.0f);
}

// ============================================================
// AK60 MIT command
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

  msg.id = (8UL << 8) | MOTOR_ID;   // 0x803
  msg.flags.extended = 1;
  msg.flags.remote = 0;
  msg.len = 8;

  // AK60 V3.x MIT byte order:
  // Kp -> Kd -> Position -> Velocity -> Torque
  msg.buf[0] =
      static_cast<uint8_t>(kp_int >> 4);

  msg.buf[1] =
      static_cast<uint8_t>(
          ((kp_int & 0x0F) << 4)
          | ((kd_int >> 8) & 0x0F));

  msg.buf[2] =
      static_cast<uint8_t>(kd_int & 0xFF);

  msg.buf[3] =
      static_cast<uint8_t>(p_int >> 8);

  msg.buf[4] =
      static_cast<uint8_t>(p_int & 0xFF);

  msg.buf[5] =
      static_cast<uint8_t>(v_int >> 4);

  msg.buf[6] =
      static_cast<uint8_t>(
          ((v_int & 0x0F) << 4)
          | ((t_int >> 8) & 0x0F));

  msg.buf[7] =
      static_cast<uint8_t>(t_int & 0xFF);

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
// Feedback
// ============================================================

void updateAccelerationDiagnostic() {
  const uint32_t nowUs = micros();

  if (!accelReady) {
    accelReady = true;
    previousSpeedTimeUs = nowUs;
    previousSpeedRadS = latestSpeedRadS;

    rawAccelerationRadS2 = 0.0f;
    inertiaTorqueNm = 0.0f;
    rawFrictionResidualNm = latestMeasuredTorqueNm;
    return;
  }

  const float dt =
      static_cast<float>(
          nowUs - previousSpeedTimeUs)
      * 1.0e-6f;

  previousSpeedTimeUs = nowUs;

  if (dt <= 0.0f || dt > 0.05f) {
    previousSpeedRadS = latestSpeedRadS;
    rawAccelerationRadS2 = 0.0f;
    inertiaTorqueNm = 0.0f;
    rawFrictionResidualNm = latestMeasuredTorqueNm;
    return;
  }

  rawAccelerationRadS2 =
      (latestSpeedRadS - previousSpeedRadS)
      / dt;

  previousSpeedRadS = latestSpeedRadS;

  inertiaTorqueNm =
      IDENTIFIED_INERTIA_KG_M2
      * rawAccelerationRadS2;

  rawFrictionResidualNm =
      latestMeasuredTorqueNm
      - inertiaTorqueNm;
}

void decodeAk60Feedback(const CAN_message_t &msg) {
  if (!msg.flags.extended || msg.len != 8) {
    return;
  }

  const uint32_t functionId = msg.id >> 8;
  const uint8_t driveId =
      static_cast<uint8_t>(msg.id & 0xFF);

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

  latestSpeedErpm =
      static_cast<float>(speedRaw) * 10.0f;

  latestSpeedRadS =
      erpmToOutputRadS(latestSpeedErpm);

  latestIqA =
      static_cast<float>(currentRaw) * 0.01f;

  latestMeasuredTorqueNm =
      TORQUE_CONSTANT_NM_PER_A
      * latestIqA;

  latestTempC =
      static_cast<float>(
          static_cast<int8_t>(msg.buf[6]));

  latestError = msg.buf[7];

  latestFeedbackTimeUs = micros();
  rxCount++;

  updateAccelerationDiagnostic();

  if (!feedbackReady) {
    feedbackReady = true;
    firstFeedbackUs = micros();
    phaseStartUs = firstFeedbackUs;
  }
}

void readCANMessages() {
  CAN_message_t msg{};

  while (Can0.read(msg)) {
    decodeAk60Feedback(msg);
  }
}

// ============================================================
// Test state machine
// ============================================================

void beginPhase(TestPhase newPhase) {
  phase = newPhase;
  phaseStartUs = micros();
}

float phaseElapsedSec() {
  return static_cast<float>(
      micros() - phaseStartUs)
      * 1.0e-6f;
}

void advanceTrial() {
  repeatIndex++;

  if (repeatIndex >= REPEATS_PER_SLOPE) {
    repeatIndex = 0;
    slopeIndex++;
  }

  if (slopeIndex >= NUM_RAMP_SLOPES) {
    testFinished = true;
    beginPhase(PHASE_FINISHED);
  } else {
    beginPhase(PHASE_PRE_REST);
  }
}

void endCurrentRampNormally() {
  torqueReferenceNm = 0.0f;
  torqueCommandNm = 0.0f;

  sendZeroTorque();
  beginPhase(PHASE_POST_REST);
}

void updateExperiment() {
  if (!feedbackReady) {
    sendZeroTorque();
    return;
  }

  const float sinceFirstFeedbackSec =
      static_cast<float>(
          micros() - firstFeedbackUs)
      * 1.0e-6f;

  switch (phase) {

    case PHASE_WAIT_START:
      torqueReferenceNm = 0.0f;
      torqueCommandNm = 0.0f;
      sendZeroTorque();

      if (sinceFirstFeedbackSec >= START_DELAY_SEC) {
        beginPhase(PHASE_PRE_REST);
      }
      break;

    case PHASE_PRE_REST:
      torqueReferenceNm = 0.0f;
      torqueCommandNm = 0.0f;
      sendZeroTorque();

      if (phaseElapsedSec() >= PRE_RAMP_REST_SEC) {
        torqueRampSlopeNmPerS =
            TORQUE_RAMP_SLOPES_NM_PER_S[slopeIndex];

        beginPhase(PHASE_RAMP);
      }
      break;

    case PHASE_RAMP: {
      const float t = phaseElapsedSec();

      torqueRampSlopeNmPerS =
          TORQUE_RAMP_SLOPES_NM_PER_S[slopeIndex];

      torqueReferenceNm =
          torqueRampSlopeNmPerS * t;

      if (torqueRampSlopeNmPerS > 0.0f) {
        torqueReferenceNm =
            min(torqueReferenceNm, TORQUE_RAMP_LIMIT_NM);
      } else {
        torqueReferenceNm =
            max(torqueReferenceNm, -TORQUE_RAMP_LIMIT_NM);
      }

      torqueCommandNm =
          TORQUE_COMMAND_SIGN
          * torqueReferenceNm;

      // Direct feedforward torque only.
      sendMitCommand(
          0.0f,
          0.0f,
          0.0f,
          0.0f,
          torqueCommandNm);

      // Normal experimental stopping condition:
      // enough speed range collected, so finish this trial.
      if (fabsf(latestSpeedRadS)
          >= TRIAL_END_SPEED_RAD_S) {

        endCurrentRampNormally();
        break;
      }

      // Also end normally when the programmed ramp reaches its limit.
      const float rampDurationSec =
          TORQUE_RAMP_LIMIT_NM
          / fabsf(torqueRampSlopeNmPerS);

      if (t >= rampDurationSec) {
        endCurrentRampNormally();
      }

      break;
    }

    case PHASE_POST_REST:
      torqueReferenceNm = 0.0f;
      torqueCommandNm = 0.0f;
      sendZeroTorque();

      if (phaseElapsedSec() >= POST_RAMP_REST_SEC) {
        advanceTrial();
      }
      break;

    case PHASE_FINISHED:
      torqueReferenceNm = 0.0f;
      torqueCommandNm = 0.0f;
      sendZeroTorque();
      break;

    default:
      torqueReferenceNm = 0.0f;
      torqueCommandNm = 0.0f;
      sendZeroTorque();
      break;
  }
}

// ============================================================
// 250 Hz logging
// ============================================================

void printData() {
  if (!feedbackReady) {
    return;
  }

  Serial.print("DATA,");

  Serial.print(millis());
  Serial.print(",");

  Serial.print(static_cast<int>(phase));
  Serial.print(",");

  Serial.print(slopeIndex);
  Serial.print(",");

  Serial.print(repeatIndex);
  Serial.print(",");

  Serial.print(torqueRampSlopeNmPerS, 6);
  Serial.print(",");

  Serial.print(torqueReferenceNm, 6);
  Serial.print(",");

  Serial.print(torqueCommandNm, 6);
  Serial.print(",");

  Serial.print(latestIqA, 6);
  Serial.print(",");

  Serial.print(latestMeasuredTorqueNm, 6);
  Serial.print(",");

  Serial.print(latestSpeedRadS, 6);
  Serial.print(",");

  Serial.print(rawAccelerationRadS2, 6);
  Serial.print(",");

  Serial.print(inertiaTorqueNm, 6);
  Serial.print(",");

  Serial.print(rawFrictionResidualNm, 6);
  Serial.print(",");

  Serial.print(latestPosDeg, 4);
  Serial.print(",");

  Serial.print(latestTempC, 1);
  Serial.print(",");

  Serial.print(latestError);
  Serial.print(",");

  Serial.println(testFinished ? 1 : 0);
}

// ============================================================
// Setup / loop
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println("==============================================");
  Serial.println("AK60 TORQUE-RAMP FRICTION IDENTIFICATION");
  Serial.println("==============================================");
  Serial.println("tau_ref(t) = torque_slope * t");
  Serial.println("Each torque slope repeated 3 times.");
  Serial.println("Log rate = 250 Hz.");
  Serial.println();

  Serial.println(
      "Time_ms,"
      "Phase,"
      "Slope_Index,"
      "Repeat_Index,"
      "Torque_Ramp_Slope_Nm_per_s,"
      "Torque_Reference_Nm,"
      "Command_Torque_Nm,"
      "Measured_Iq_A,"
      "Measured_Motor_Torque_Nm,"
      "Measured_Speed_rad_s,"
      "Raw_Acceleration_rad_s2,"
      "Inertia_Torque_Nm,"
      "Raw_Friction_Residual_Nm,"
      "Position_deg,"
      "Temperature_C,"
      "Error_Code,"
      "Test_Finished");

  Can0.begin();
  Can0.setBaudRate(CAN_BAUD);

  delay(500);
  sendZeroTorque();
}

void loop() {
  Can0.events();
  readCANMessages();

  if (txTimer >= COMMAND_PERIOD_US) {
    txTimer -= COMMAND_PERIOD_US;
    updateExperiment();
  }

  if (logTimer >= LOG_PERIOD_US) {
    logTimer -= LOG_PERIOD_US;
    printData();
  }

  if (statusTimer >= 1000) {
    statusTimer = 0;

    Serial.print("LOG: phase=");
    Serial.print(static_cast<int>(phase));

    Serial.print(" slopeIndex=");
    Serial.print(slopeIndex);

    Serial.print(" repeat=");
    Serial.print(repeatIndex);

    Serial.print(" slope=");
    Serial.print(torqueRampSlopeNmPerS, 3);

    Serial.print(" tauRef=");
    Serial.print(torqueReferenceNm, 3);

    Serial.print(" tauMeas=");
    Serial.print(latestMeasuredTorqueNm, 3);

    Serial.print(" speed=");
    Serial.println(latestSpeedRadS, 3);
  }
}