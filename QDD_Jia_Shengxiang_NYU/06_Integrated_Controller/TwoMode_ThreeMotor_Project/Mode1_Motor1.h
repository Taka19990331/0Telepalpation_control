#pragma once

namespace Mode1Motor1 {

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can1;

// ===================== Basic settings =====================
const uint8_t MOTOR_ID = 1;
const uint32_t CAN_BAUD = 1000000;

elapsedMicros txTimer;
elapsedMillis logTimer;

const float CONTROL_FREQ_HZ = 3000.0f;
const float DT = 1.0f / CONTROL_FREQ_HZ;

const uint32_t CONTROL_INTERVAL_US = 333;   // about 3000 Hz
const uint32_t LOG_INTERVAL_MS = 5;         // 200 Hz log

const float RAD2DEG = 57.2957795131f;

// ===================== MIT mode range =====================
const float P_MIN = -12.5f;
const float P_MAX = 12.5f;
const float V_MIN = -45.0f;
const float V_MAX = 45.0f;
const float KP_MIN = 0.0f;
const float KP_MAX = 500.0f;
const float KD_MIN = 0.0f;
const float KD_MAX = 5.0f;
const float T_MIN = -32.0f;
const float T_MAX = 32.0f;

// ===================== Test timing =====================
const double START_DELAY_SEC = 1.0;
const double DURATION_SEC = 300.0;
const double STOP_DELAY_SEC = 1.0;

// ===================== Safety =====================
const float TORQUE_LIMIT_NM = 5.0f;
const uint32_t FEEDBACK_TIMEOUT_US = 50000;
const uint32_t SAFE_EXIT_TIMEOUT_US = 250000;
const bool EXIT_MIT_ON_MOTOR_ERROR = true;
const bool EXIT_MIT_ON_LONG_TIMEOUT = true;

// ===================== Identified nominal plant =====================
// Pn(s) = 1 / (J s^2 + D s)
// Identified: P(s)=87.613/(s^2+22.8889s)
const float J_NOM = 0.0114138376f;
const float D_NOM = 0.2612504548f;

// ===================== Gimp model =====================
// Gimp(s)=1/(Mimp s^2 + Cimp s + Kimp)
// First keep M=J, C=D, tune only K.
const float M_IMP = J_NOM;
const float C_IMP = D_NOM*5.0;
const float K_IMP = 8.0f;   // tune this first: 0.2, 0.3, 0.5

// ===================== External torque estimate =====================
const float EXTERNAL_TORQUE_SIGN = -1.0f;       // flip if the hand-push direction is reversed
const float EXT_TORQUE_LPF_ALPHA = 0.02f;

// ===================== GIMP input zeroing + smooth gain scheduling =====================
// Test-only compensation before the RFOB is implemented.
//
// Processing:
// measured torque -> startup bias removal -> LPF -> soft deadzone
// -> smooth low-speed/small-torque gain -> GIMP
//
// IMPORTANT:
// Gain is applied only after bias removal and deadzone. Otherwise the
// estimator offset would also be amplified.
const bool GIMP_GAIN_SCHED_ENABLE = true;

// Startup bias is averaged while elapsed time is below START_DELAY_SEC.
const bool AUTO_TORQUE_BIAS_ENABLE = true;

// Residual torque around zero is removed continuously and symmetrically.
const float GIMP_TORQUE_DEADZONE_NM = 0.03f;

// Maximum small-signal gain. Begin conservatively.
const float GIMP_GAIN_MAX = 1.50f;

// Gain smoothly returns toward 1 as torque and speed increase.
const float GIMP_GAIN_TORQUE_SCALE_NM = 0.15f;
const float GIMP_GAIN_SPEED_SCALE_RAD_S = 0.10f;

// Gain also returns toward 1 when the GIMP displacement reaches this region.
const float GIMP_GAIN_POSITION_CENTER_RAD = 2.6f * PI / 180.0f;
const float GIMP_GAIN_POSITION_WIDTH_RAD  = 0.5f * PI / 180.0f;

// Safety clamp on the torque signal supplied to the GIMP model.
const float GIMP_INPUT_LIMIT_NM = 10.0f;

// ===================== RFOB observation-only module =====================
// The direct measured-torque path remains the active GIMP input because it
// produced the better experimental result. RFOB is preserved independently
// for observation, logging, and future sensorless experiments.
const bool RFOB_ENABLE = true;
const bool RFOB_DRIVES_GIMP = false;

const float RFOB_CUTOFF_HZ = 1.0f;
const float J_RFOB_KGM2 = 0.0090f;
const float RFOB_TAU_C_POS_NM = 0.085f;
const float RFOB_TAU_C_NEG_NM = 0.090f;
const float RFOB_OMEGA0_RAD_S = 0.50f;
const float RFOB_B_NM_PER_RAD_S = 0.0003f;
const float RFOB_MOTOR_TORQUE_SIGN = 1.0f;
const float RFOB_TO_GIMP_SIGN = 1.0f;

// ===================== Position controller Cpos(z) =====================
// From MATLAB, Ts = 0.0003 s
const float CPOS_B0 = 216.2607249f;
const float CPOS_B1 = -429.7119067f;
const float CPOS_B2 = 213.4633874f;
const float CPOS_A1 = -1.929761384f;
const float CPOS_A2 = 0.9297613839f;

// ===================== DOB settings =====================
// Professor-style DOB compensation:
// d_hat = Q(s) * (Pn^-1(s)*x - u)
// command_torque = position_torque + DOB_COMP_SIGN * DOB_GAIN * d_hat
//
// DOB_ENABLE = false: GIMP-only test, no DOB compensation
// DOB_ENABLE = true : GIMP + DOB compensation
const bool DOB_ENABLE = true;

// Start small. Professor-style DOB compensation can make the motor harder/stiffer.
const float DOB_GAIN = 1.0f;

// With this sign, command = positionTorque - DOB_GAIN * d_hat.
// If DOB makes vibration worse, try +1.0f.
const float DOB_COMP_SIGN = -1.0f;

// Q filter setting. Current stable baseline.
const float DOB_Q_FREQ_HZ = 1.0f;
const float DOB_Q_ZETA = 0.7f;

// ===================== Friction feedforward + breakaway compensation =====================
// Unit: Nm
// Same friction FF structure as DOB_test_professor_DOB_with_friction_FF.
// commandTorque = positionTorque + dobComp + frictionFF

const bool FRICTION_FF_ENABLE = true;

// Sliding / Coulomb friction compensation
// P: positive direction, N: negative direction
const float TAU_C_P_NM = 0.1f;
const float TAU_C_N_NM = 0.1f;

// Smooth tanh transition speed
// tauC = tau_c * tanh(omega / omega0)
const float OMEGA0_RAD_S = 0.30f;

// Static / breakaway compensation
const float TAU_S_P_NM = 0.2f;
const float TAU_S_N_NM = 0.2f;

// Breakaway trigger condition
const float OMEGA_TH_RAD_S = 0.2f;
const float ERROR_TH_RAD = 0.2f * PI / 180.0f;

// Safety limit for total friction compensation
const float FRICTION_TOTAL_LIMIT_NM = 1.0f;

// Flip to -1.0f if friction compensation direction is wrong
const float FRICTION_SIGN = 1.0f;

// ===================== Acceleration feedforward =====================
// Uses GIMP reference acceleration, not measured acceleration.
// tau_acc_ff = J_ACC_FF * gimpAcc
// Start conservative. Identified J is about 0.0114 Nm*s^2/rad.
const bool ACC_FF_ENABLE = true;
const float J_ACC_FF = J_NOM;
const float ACC_FF_LIMIT_NM = 0.15f;

// ===================== Task 1 module data =====================
// Motor angle and angular velocity are both received directly from the motor
// driver through MIT CAN feedback. Velocity is not calculated by
// differentiating motor angle on the Teensy.
struct MotorState {
  float position_raw_rad;
  float position_rad;
  float velocity_rad_s;
  float torque_nm;
  float temperature_c;
  uint8_t error_code;

  uint32_t feedback_time_us;
  uint32_t feedback_count;
  bool feedback_valid;
  uint32_t feedback_age_us;
  bool communication_timeout;
};

enum class SafetyEvent : uint8_t {
  NONE = 0,
  INVALID_FEEDBACK,
  FEEDBACK_TIMEOUT,
  MOTOR_ERROR,
  EMERGENCY_STOP
};

struct MotorSafetyState {
  bool emergency_stop;
  bool motor_safe;
  bool mit_exit_sent;
  SafetyEvent last_event;
  uint32_t event_count;
  uint32_t last_event_time_us;
};

struct ExternalTorqueEstimate {
  float raw_nm;
  float bias_nm;
  float zeroed_nm;
  float filtered_nm;
  float deadzone_nm;
  float scheduled_gain;
  float for_gimp_nm;
};

struct GimpReference {
  float position_rad;
  float velocity_rad_s;
  float acceleration_rad_s2;
};

struct PositionControllerOutput {
  float error_rad;
  float torque_nm;
};

struct DobEstimate {
  float inverse_model_term_nm;
  float command_filter_term_nm;
  float disturbance_estimate_nm;
  float compensation_nm;
};

struct RfobEstimate {
  float motor_torque_zeroed_nm;
  float friction_model_nm;
  float viscous_model_nm;
  float internal_resistance_nm;
  float filter_input_nm;
  float filter_state_nm;
  float external_torque_nm;
  float reaction_torque_nm;
};

struct MotorCompensation {
  float coulomb_friction_nm;
  float breakaway_friction_nm;
  float viscous_friction_nm;
  float torque_bias_nm;
  float rotor_inertia_nm;
  float motor_viscous_dynamics_nm;
  float acceleration_feedforward_nm;
  float total_nm;
};

struct TorqueCommandTerms {
  float robot_task_torque_nm;
  float gravity_compensation_nm;
  float position_control_nm;
  float dob_compensation_nm;
  float motor_compensation_nm;
  float unsaturated_total_nm;
  float saturated_total_nm;
};

MotorState motorState = {};
MotorSafetyState motorSafety = {
  false, false, false, SafetyEvent::INVALID_FEEDBACK, 0, 0
};

ExternalTorqueEstimate externalTorqueEstimate = {};
GimpReference gimpReference = {};
PositionControllerOutput positionControllerOutput = {};
DobEstimate dobEstimate = {};
RfobEstimate rfobEstimate = {};
MotorCompensation motorCompensation = {};
TorqueCommandTerms torqueCommandTerms = {};

// ===================== State variables =====================
bool feedbackReady = false;
bool activeStarted = false;

float latestPosRawRad = 0.0f;
float latestPosRad = 0.0f;
float lastPosRawRad = 0.0f;
float latestSpeedRadS = 0.0f;
float latestTorqueNm = 0.0f;
float latestTempC = 0.0f;
uint8_t latestErrorCode = 0;

float startPosRad = 0.0f;
float targetPosRad = 0.0f;
float gimpRefRad = 0.0f;

float externalTorqueBiasNm = 0.0f;
double externalTorqueBiasSumNm = 0.0;
uint32_t externalTorqueBiasCount = 0;

float externalTorqueRawNm = 0.0f;
float externalTorqueZeroedNm = 0.0f;
float externalTorqueFilt = 0.0f;
float externalTorqueDeadzoneNm = 0.0f;
float externalTorqueNm = 0.0f;   // final torque input supplied to GIMP
float gimpScheduledGain = 1.0f;

float gimpPos = 0.0f;
float gimpVel = 0.0f;
float gimpAcc = 0.0f;

float positionTorqueNm = 0.0f;
float dobEstNm = 0.0f;
float dobCompNm = 0.0f;

float frictionFFNm = 0.0f;
float breakawayNm = 0.0f;
float accFFNm = 0.0f;

float commandTorqueNm = 0.0f;
float commandTorquePrevNm = 0.0f;

float rfobFilterStateNm = 0.0f;
float rfobMotorTorqueBiasNm = 0.0f;

bool globalZeroSet = false;
float globalZeroRad = 0.0f;

// ===================== IIR second order filter =====================
struct IIR2 {
  float b0, b1, b2;
  float a1, a2;
  float x1, x2;
  float y1, y2;
};

IIR2 dobQ;
IIR2 dobQPinv;
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

// ===================== MIT helper =====================
uint16_t floatToUint(float x, float xMin, float xMax, int bits) {
  float span = xMax - xMin;
  float offset = xMin;
  float xConstrained = constrain(x, xMin, xMax);
  return (uint16_t)((xConstrained - offset) * ((float)((1UL << bits) - 1)) / span);
}

float uintToFloat(uint16_t xInt, float xMin, float xMax, int bits) {
  float span = xMax - xMin;
  float offset = xMin;
  return ((float)xInt) * span / ((float)((1UL << bits) - 1)) + offset;
}

void sendMitCommandPacket(float p, float v, float kp, float kd, float tau) {
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
}

void sendSpecialCommand(uint8_t lastByte) {
  CAN_message_t msg;
  msg.id = MOTOR_ID;
  msg.len = 8;
  for (int i = 0; i < 7; i++) msg.buf[i] = 0xFF;
  msg.buf[7] = lastByte;
  Can1.write(msg);
}

void enterMitMode() {
  sendSpecialCommand(0xFC);
}

void exitMitMode() {
  sendSpecialCommand(0xFD);
}

void zeroMotor() {
  sendSpecialCommand(0xFE);
}

// ===================== read_motor_state() module =====================
void updateUnwrappedPosition(float rawRad) {
  const float wrap = P_MAX - P_MIN;

  if (!motorState.feedback_valid) {
    motorState.position_rad = rawRad;
    latestPosRad = rawRad;
    lastPosRawRad = rawRad;
    return;
  }

  float diff = rawRad - lastPosRawRad;

  if (diff > wrap * 0.5f) diff -= wrap;
  if (diff < -wrap * 0.5f) diff += wrap;

  motorState.position_rad += diff;
  lastPosRawRad = rawRad;
}

bool parseMotorFeedback(const CAN_message_t &msg) {
  if (msg.len < 6) return false;
  if (msg.id != MOTOR_ID && msg.buf[0] != MOTOR_ID) return false;

  const uint16_t pInt =
      ((uint16_t)msg.buf[1] << 8) | msg.buf[2];

  const uint16_t vInt =
      ((uint16_t)msg.buf[3] << 4) | (msg.buf[4] >> 4);

  const uint16_t tInt =
      (((uint16_t)msg.buf[4] & 0x0F) << 8) | msg.buf[5];

  const float rawPositionRad =
      uintToFloat(pInt, P_MIN, P_MAX, 16);

  updateUnwrappedPosition(rawPositionRad);

  motorState.position_raw_rad = rawPositionRad;
  motorState.velocity_rad_s =
      uintToFloat(vInt, V_MIN, V_MAX, 12);
  motorState.torque_nm =
      uintToFloat(tInt, T_MIN, T_MAX, 12);
  motorState.temperature_c =
      msg.len >= 7 ? (float)msg.buf[6] : 0.0f;
  motorState.error_code =
      msg.len >= 8 ? msg.buf[7] : 0;

  motorState.feedback_time_us = micros();
  motorState.feedback_count++;
  motorState.feedback_valid = true;
  motorState.feedback_age_us = 0;
  motorState.communication_timeout = false;

  return true;
}

void updateFeedbackAgeAndSafety() {
  if (!motorState.feedback_valid) {
    motorState.feedback_age_us = UINT32_MAX;
    motorState.communication_timeout = true;
    motorSafety.motor_safe = false;
    return;
  }

  motorState.feedback_age_us =
      (uint32_t)(micros() - motorState.feedback_time_us);

  motorState.communication_timeout =
      motorState.feedback_age_us > FEEDBACK_TIMEOUT_US;

  motorSafety.motor_safe =
      !motorSafety.emergency_stop
      && !motorState.communication_timeout
      && motorState.error_code == 0;
}

void copyMotorStateToLegacyControllerVariables() {
  feedbackReady = motorState.feedback_valid;
  latestPosRawRad = motorState.position_raw_rad;
  latestPosRad = motorState.position_rad;
  latestSpeedRadS = motorState.velocity_rad_s;
  latestTorqueNm = motorState.torque_nm;
  latestTempC = motorState.temperature_c;
  latestErrorCode = motorState.error_code;
}

void read_motor_state() {
  CAN_message_t msg;

  while (Can1.read(msg)) {
    parseMotorFeedback(msg);
  }

  updateFeedbackAgeAndSafety();
  copyMotorStateToLegacyControllerVariables();
}

const char *safetyEventName(SafetyEvent event) {
  switch (event) {
    case SafetyEvent::INVALID_FEEDBACK: return "INVALID_FEEDBACK";
    case SafetyEvent::FEEDBACK_TIMEOUT: return "FEEDBACK_TIMEOUT";
    case SafetyEvent::MOTOR_ERROR: return "MOTOR_ERROR";
    case SafetyEvent::EMERGENCY_STOP: return "EMERGENCY_STOP";
    default: return "NONE";
  }
}

void recordSafetyEvent(SafetyEvent event) {
  if (motorSafety.last_event == event) return;

  motorSafety.last_event = event;
  motorSafety.event_count++;
  motorSafety.last_event_time_us = micros();

  Serial.print("SAFETY,");
  Serial.print(safetyEventName(event));
  Serial.print(",");
  Serial.print(motorState.feedback_age_us);
  Serial.print(",");
  Serial.println(motorState.error_code);
}

SafetyEvent evaluateMotorSafety() {
  if (motorSafety.emergency_stop) {
    return SafetyEvent::EMERGENCY_STOP;
  }

  if (!motorState.feedback_valid) {
    return SafetyEvent::INVALID_FEEDBACK;
  }

  if (motorState.communication_timeout) {
    return SafetyEvent::FEEDBACK_TIMEOUT;
  }

  if (motorState.error_code != 0) {
    return SafetyEvent::MOTOR_ERROR;
  }

  return SafetyEvent::NONE;
}

bool send_motor_torque(float requestedTorqueNm) {
  updateFeedbackAgeAndSafety();

  const SafetyEvent event = evaluateMotorSafety();

  if (event != SafetyEvent::NONE) {
    motorSafety.motor_safe = false;
    recordSafetyEvent(event);

    sendMitCommandPacket(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    const bool longTimeout =
        motorState.feedback_valid
        && motorState.feedback_age_us > SAFE_EXIT_TIMEOUT_US;

    const bool exitRequired =
        event == SafetyEvent::EMERGENCY_STOP
        || (event == SafetyEvent::MOTOR_ERROR
            && EXIT_MIT_ON_MOTOR_ERROR)
        || (event == SafetyEvent::FEEDBACK_TIMEOUT
            && longTimeout
            && EXIT_MIT_ON_LONG_TIMEOUT);

    if (exitRequired && !motorSafety.mit_exit_sent) {
      exitMitMode();
      motorSafety.mit_exit_sent = true;
      Serial.println("SAFETY,MIT_MODE_EXIT_SENT");
    }

    return false;
  }

  motorSafety.motor_safe = true;
  motorSafety.last_event = SafetyEvent::NONE;
  motorSafety.mit_exit_sent = false;

  const float safeLimitNm =
      min(TORQUE_LIMIT_NM, min(fabsf(T_MIN), fabsf(T_MAX)));

  const float safeTorqueNm =
      constrain(requestedTorqueNm, -safeLimitNm, safeLimitNm);

  sendMitCommandPacket(
      0.0f, 0.0f, 0.0f, 0.0f, safeTorqueNm
  );

  return true;
}

void send_zero_torque() {
  (void)send_motor_torque(0.0f);
}

void set_emergency_stop(bool enabled) {
  motorSafety.emergency_stop = enabled;

  if (enabled) {
    recordSafetyEvent(SafetyEvent::EMERGENCY_STOP);
    sendMitCommandPacket(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    if (!motorSafety.mit_exit_sent) {
      exitMitMode();
      motorSafety.mit_exit_sent = true;
    }
  }
}

// ===================== Controller setup =====================
void computeDobCoefficients() {
  const float Ts = DT;
  const float Kt = 2.0f / Ts;
  const float w = 2.0f * PI * DOB_Q_FREQ_HZ;
  const float z = DOB_Q_ZETA;

  float den0 = Kt * Kt + 2.0f * z * w * Kt + w * w;
  float den1 = -2.0f * Kt * Kt + 2.0f * w * w;
  float den2 = Kt * Kt - 2.0f * z * w * Kt + w * w;

  dobQ.b0 = (w * w) / den0;
  dobQ.b1 = (2.0f * w * w) / den0;
  dobQ.b2 = (w * w) / den0;
  dobQ.a1 = den1 / den0;
  dobQ.a2 = den2 / den0;
  resetIIR(dobQ);

  // Q(s) * Pn^-1(s) = Q(s) * (J*s^2 + D*s)
  dobQPinv.b0 = (J_NOM * w * w * Kt * Kt + D_NOM * w * w * Kt) / den0;
  dobQPinv.b1 = (-2.0f * J_NOM * w * w * Kt * Kt) / den0;
  dobQPinv.b2 = (J_NOM * w * w * Kt * Kt - D_NOM * w * w * Kt) / den0;
  dobQPinv.a1 = dobQ.a1;
  dobQPinv.a2 = dobQ.a2;
  resetIIR(dobQPinv);
}

void setupPositionController() {
  cpos.b0 = CPOS_B0;
  cpos.b1 = CPOS_B1;
  cpos.b2 = CPOS_B2;
  cpos.a1 = CPOS_A1;
  cpos.a2 = CPOS_A2;
  resetIIR(cpos);
}

void resetControllerStates() {
  resetIIR(cpos);
  resetIIR(dobQ);
  resetIIR(dobQPinv);

  gimpPos = 0.0f;
  gimpVel = 0.0f;
  gimpAcc = 0.0f;
  gimpRefRad = 0.0f;

  externalTorqueRawNm = 0.0f;
  externalTorqueZeroedNm = 0.0f;
  externalTorqueFilt = 0.0f;
  externalTorqueDeadzoneNm = 0.0f;
  externalTorqueNm = 0.0f;
  gimpScheduledGain = 1.0f;

  positionTorqueNm = 0.0f;
  dobEstNm = 0.0f;
  dobCompNm = 0.0f;

  frictionFFNm = 0.0f;
  breakawayNm = 0.0f;
  accFFNm = 0.0f;

  commandTorqueNm = 0.0f;
  commandTorquePrevNm = 0.0f;

  rfobFilterStateNm = 0.0f;
  externalTorqueEstimate = {};
  gimpReference = {};
  positionControllerOutput = {};
  dobEstimate = {};
  rfobEstimate = {};
  motorCompensation = {};
  torqueCommandTerms = {};
}

GimpReference gimp_admittance_model(float inputTorqueNm) {
  const float Ts = DT;

  // Save GIMP reference acceleration for acceleration feedforward.
  // This is desired/reference acceleration, not noisy measured acceleration.
  gimpAcc = (inputTorqueNm - C_IMP * gimpVel - K_IMP * gimpPos) / M_IMP;

  gimpVel += gimpAcc * Ts;
  gimpPos += gimpVel * Ts;

  gimpReference.position_rad = gimpPos;
  gimpReference.velocity_rad_s = gimpVel;
  gimpReference.acceleration_rad_s2 = gimpAcc;
  return gimpReference;
}

PositionControllerOutput position_controller(
    float referencePositionRad,
    float measuredPositionRad) {
  positionControllerOutput.error_rad =
      referencePositionRad - measuredPositionRad;
  positionControllerOutput.torque_nm =
      runIIR(cpos, positionControllerOutput.error_rad);
  return positionControllerOutput;
}

float signFloat(float x) {
  if (x > 0.0f) return 1.0f;
  if (x < 0.0f) return -1.0f;
  return 0.0f;
}

float getFrictionDirectionFromControl(float positionTorqueNm,
                                      float posErrorRad,
                                      float refVelRadS) {
  const float POS_TORQUE_EPS_NM = 0.05f;
  const float REF_VEL_EPS_RAD_S = 0.03f;

  // Main rule:
  // if the position controller is already pushing,
  // friction FF should help that direction.
  if (fabs(positionTorqueNm) > POS_TORQUE_EPS_NM) {
    return signFloat(positionTorqueNm);
  }

  // If controller torque is tiny but the GIMP reference is moving,
  // use reference velocity direction.
  if (fabs(refVelRadS) > REF_VEL_EPS_RAD_S) {
    return signFloat(refVelRadS);
  }

  // Last fallback near zero speed.
  if (fabs(posErrorRad) > ERROR_TH_RAD) {
    return signFloat(posErrorRad);
  }

  return 0.0f;
}

float computeFrictionWithBreakaway(float frictionVelRadS,
                                   float measuredVelRadS,
                                   float posErrorRad,
                                   float positionTorqueNm,
                                   float refVelRadS) {
  if (!FRICTION_FF_ENABLE) {
    breakawayNm = 0.0f;
    return 0.0f;
  }

  float dir = getFrictionDirectionFromControl(positionTorqueNm,
                                             posErrorRad,
                                             refVelRadS);

  // 1) Smooth Coulomb / dynamic friction compensation.
  // Magnitude uses reference velocity so FF does not wait for the motor to move.
  float tauC = 0.0f;

  if (dir > 0.0f) {
    tauC = TAU_C_P_NM * tanhf(fabs(frictionVelRadS) / OMEGA0_RAD_S);
  } else if (dir < 0.0f) {
    tauC = -TAU_C_N_NM * tanhf(fabs(frictionVelRadS) / OMEGA0_RAD_S);
  }

  // 2) Minimum friction help when the position controller is clearly pushing.
  // This prevents tauC from becoming almost zero near startup.
  const float TORQUE_HELP_TH_NM = 0.08f;
  const float MIN_FRICTION_P_NM = 0.08f;
  const float MIN_FRICTION_N_NM = 0.08f;

  if (fabs(positionTorqueNm) > TORQUE_HELP_TH_NM) {
    if (dir > 0.0f && tauC < MIN_FRICTION_P_NM) {
      tauC = MIN_FRICTION_P_NM;
    } else if (dir < 0.0f && tauC > -MIN_FRICTION_N_NM) {
      tauC = -MIN_FRICTION_N_NM;
    }
  }

  // 3) Static breakaway compensation.
  // Use MEASURED motor speed for the low-speed condition.
  // If the motor is physically not moving but the reference velocity is nonzero,
  // measuredVelRadS is the one that tells the truth.
  breakawayNm = 0.0f;

if (fabsf(measuredVelRadS) < OMEGA_TH_RAD_S &&
    fabsf(positionTorqueNm) > TORQUE_HELP_TH_NM &&
    fabsf(posErrorRad) > ERROR_TH_RAD) {

    if (dir > 0.0f) {
        breakawayNm = TAU_S_P_NM;
    } else if (dir < 0.0f) {
        breakawayNm = -TAU_S_N_NM;
    }
}

  float tau = tauC + breakawayNm;

  tau *= FRICTION_SIGN;
  tau = constrain(tau, -FRICTION_TOTAL_LIMIT_NM, FRICTION_TOTAL_LIMIT_NM);

  return tau;
}


float applySoftDeadzone(float value, float deadzone) {
  float magnitude = fabsf(value);

  if (magnitude <= deadzone) {
    return 0.0f;
  }

  return copysignf(magnitude - deadzone, value);
}

float computeGimpScheduledGain(float torqueNm,
                               float measuredVelocityRadS,
                               float previousGimpPositionRad) {
  if (!GIMP_GAIN_SCHED_ENABLE) {
    return 1.0f;
  }

  const float torqueScale =
      fmaxf(GIMP_GAIN_TORQUE_SCALE_NM, 1.0e-6f);
  const float speedScale =
      fmaxf(GIMP_GAIN_SPEED_SCALE_RAD_S, 1.0e-6f);
  const float positionWidth =
      fmaxf(GIMP_GAIN_POSITION_WIDTH_RAD, 1.0e-6f);

  float torqueRatio = fabsf(torqueNm) / torqueScale;
  float speedRatio = fabsf(measuredVelocityRadS) / speedScale;

  // Gaussian weights: close to 1 near zero and smoothly decay toward zero.
  float torqueWeight = expf(-torqueRatio * torqueRatio);
  float speedWeight = expf(-speedRatio * speedRatio);

  // Smoothly reduce the gain to 1 around the selected displacement region.
  float positionArgument =
      (fabsf(previousGimpPositionRad) - GIMP_GAIN_POSITION_CENTER_RAD)
      / positionWidth;

  float positionWeight =
      0.5f * (1.0f - tanhf(positionArgument));

  float scheduledGain =
      1.0f
      + (GIMP_GAIN_MAX - 1.0f)
      * torqueWeight
      * speedWeight
      * positionWeight;

  return constrain(scheduledGain, 1.0f, GIMP_GAIN_MAX);
}

// ===================== Direct external-torque estimator =====================
ExternalTorqueEstimate direct_external_torque_estimator(
    float measuredTorqueNm,
    float measuredVelocityRadS,
    float previousGimpPositionRad) {
  externalTorqueEstimate.raw_nm =
      EXTERNAL_TORQUE_SIGN * measuredTorqueNm;

  externalTorqueEstimate.bias_nm =
      externalTorqueBiasNm;

  externalTorqueEstimate.zeroed_nm =
      EXTERNAL_TORQUE_SIGN
      * (measuredTorqueNm - externalTorqueBiasNm);

  externalTorqueFilt +=
      EXT_TORQUE_LPF_ALPHA
      * (externalTorqueEstimate.zeroed_nm - externalTorqueFilt);

  externalTorqueEstimate.filtered_nm =
      externalTorqueFilt;

  externalTorqueEstimate.deadzone_nm =
      applySoftDeadzone(
          externalTorqueEstimate.filtered_nm,
          GIMP_TORQUE_DEADZONE_NM
      );

  externalTorqueEstimate.scheduled_gain =
      computeGimpScheduledGain(
          externalTorqueEstimate.deadzone_nm,
          measuredVelocityRadS,
          previousGimpPositionRad
      );

  externalTorqueEstimate.for_gimp_nm =
      externalTorqueEstimate.scheduled_gain
      * externalTorqueEstimate.deadzone_nm;

  externalTorqueEstimate.for_gimp_nm =
      constrain(
          externalTorqueEstimate.for_gimp_nm,
          -GIMP_INPUT_LIMIT_NM,
           GIMP_INPUT_LIMIT_NM
      );

  return externalTorqueEstimate;
}

// ===================== DOB module =====================
// d_hat = (Q * Pn^-1){position} - Q{previous command torque}
// Actual motor acceleration is not explicitly calculated. The acceleration
// effect is contained in the combined Q*(J*s^2 + D*s) IIR filter.
DobEstimate dob_disturbance_estimator(
    float relativePositionRad,
    float previousCommandTorqueNm) {
  dobEstimate.inverse_model_term_nm =
      runIIR(dobQPinv, relativePositionRad);

  dobEstimate.command_filter_term_nm =
      runIIR(dobQ, previousCommandTorqueNm);

  dobEstimate.disturbance_estimate_nm =
      dobEstimate.inverse_model_term_nm
      - dobEstimate.command_filter_term_nm;

  dobEstimate.compensation_nm =
      DOB_ENABLE
      ? DOB_COMP_SIGN * DOB_GAIN
        * dobEstimate.disturbance_estimate_nm
      : 0.0f;

  return dobEstimate;
}

// ===================== RFOB module =====================
// RFOB remains observation-only in this baseline. Its result is logged but
// does not replace the direct measured-torque input to GIMP.
RfobEstimate rfob_external_torque_estimator(
    float measuredTorqueNm,
    float measuredVelocityRadS) {
  if (!RFOB_ENABLE) {
    rfobEstimate = {};
    return rfobEstimate;
  }

  const float signedMotorTorqueNm =
      RFOB_MOTOR_TORQUE_SIGN
      * (measuredTorqueNm - rfobMotorTorqueBiasNm);

  const float frictionNm =
      measuredVelocityRadS >= 0.0f
      ? RFOB_TAU_C_POS_NM
        * tanhf(measuredVelocityRadS / RFOB_OMEGA0_RAD_S)
      : RFOB_TAU_C_NEG_NM
        * tanhf(measuredVelocityRadS / RFOB_OMEGA0_RAD_S);

  const float viscousNm =
      RFOB_B_NM_PER_RAD_S * measuredVelocityRadS;

  const float internalResistanceNm =
      frictionNm + viscousNm;

  const float g =
      2.0f * PI * RFOB_CUTOFF_HZ;

  const float alpha =
      1.0f - expf(-g * DT);

  const float filterInputNm =
      internalResistanceNm
      - signedMotorTorqueNm
      - g * J_RFOB_KGM2 * measuredVelocityRadS;

  rfobFilterStateNm +=
      alpha * (filterInputNm - rfobFilterStateNm);

  const float externalTorqueNm =
      rfobFilterStateNm
      + g * J_RFOB_KGM2 * measuredVelocityRadS;

  rfobEstimate.motor_torque_zeroed_nm = signedMotorTorqueNm;
  rfobEstimate.friction_model_nm = frictionNm;
  rfobEstimate.viscous_model_nm = viscousNm;
  rfobEstimate.internal_resistance_nm = internalResistanceNm;
  rfobEstimate.filter_input_nm = filterInputNm;
  rfobEstimate.filter_state_nm = rfobFilterStateNm;
  rfobEstimate.external_torque_nm = externalTorqueNm;
  rfobEstimate.reaction_torque_nm = -externalTorqueNm;

  return rfobEstimate;
}

// ===================== Motor compensation module =====================
MotorCompensation motor_compensation(
    float referenceVelocityRadS,
    float measuredVelocityRadS,
    float positionErrorRad,
    float positionControlTorqueNm,
    float referenceAccelerationRadS2) {
  motorCompensation = {};

  const float combinedFrictionNm =
      computeFrictionWithBreakaway(
          referenceVelocityRadS,
          measuredVelocityRadS,
          positionErrorRad,
          positionControlTorqueNm,
          referenceVelocityRadS
      );

  motorCompensation.breakaway_friction_nm =
      breakawayNm;

  motorCompensation.coulomb_friction_nm =
      combinedFrictionNm - breakawayNm;

  motorCompensation.acceleration_feedforward_nm =
      ACC_FF_ENABLE
      ? constrain(
          J_ACC_FF * referenceAccelerationRadS2,
          -ACC_FF_LIMIT_NM,
           ACC_FF_LIMIT_NM
        )
      : 0.0f;

  motorCompensation.total_nm =
      motorCompensation.coulomb_friction_nm
      + motorCompensation.breakaway_friction_nm
      + motorCompensation.viscous_friction_nm
      + motorCompensation.torque_bias_nm
      + motorCompensation.rotor_inertia_nm
      + motorCompensation.motor_viscous_dynamics_nm
      + motorCompensation.acceleration_feedforward_nm;

  frictionFFNm =
      motorCompensation.coulomb_friction_nm
      + motorCompensation.breakaway_friction_nm;

  breakawayNm =
      motorCompensation.breakaway_friction_nm;

  accFFNm =
      motorCompensation.acceleration_feedforward_nm;

  return motorCompensation;
}

// ===================== Final torque assembly module =====================
TorqueCommandTerms assemble_torque_command(
    float robotTaskTorqueNm,
    float gravityCompensationNm,
    float positionControlTorqueNm,
    float dobCompensationNm,
    const MotorCompensation &compensation) {
  torqueCommandTerms.robot_task_torque_nm = robotTaskTorqueNm;
  torqueCommandTerms.gravity_compensation_nm = gravityCompensationNm;
  torqueCommandTerms.position_control_nm = positionControlTorqueNm;
  torqueCommandTerms.dob_compensation_nm = dobCompensationNm;
  torqueCommandTerms.motor_compensation_nm = compensation.total_nm;

  torqueCommandTerms.unsaturated_total_nm =
      robotTaskTorqueNm
      + gravityCompensationNm
      + positionControlTorqueNm
      + dobCompensationNm
      + compensation.total_nm;

  torqueCommandTerms.saturated_total_nm =
      constrain(
          torqueCommandTerms.unsaturated_total_nm,
          -TORQUE_LIMIT_NM,
           TORQUE_LIMIT_NM
      );

  return torqueCommandTerms;
}

// ===================== Main control =====================
void updateControl(double elapsedSec) {
  if (!motorSafety.motor_safe) {
    activeStarted = false;
    commandTorqueNm = 0.0f;
    positionTorqueNm = 0.0f;
    send_zero_torque();
    return;
  }

  if (feedbackReady
      && AUTO_TORQUE_BIAS_ENABLE
      && !activeStarted
      && elapsedSec < START_DELAY_SEC) {
    externalTorqueBiasSumNm += latestTorqueNm;
    externalTorqueBiasCount++;
  }

  const bool active =
      feedbackReady
      && elapsedSec >= START_DELAY_SEC
      && elapsedSec <= START_DELAY_SEC + DURATION_SEC;

  if (active) {
    if (!activeStarted) {
      resetControllerStates();

      if (!globalZeroSet) {
        globalZeroRad = latestPosRad;
        globalZeroSet = true;
      }

      startPosRad = globalZeroRad;
      targetPosRad = startPosRad;

      if (AUTO_TORQUE_BIAS_ENABLE
          && externalTorqueBiasCount > 0) {
        externalTorqueBiasNm =
            (float)(externalTorqueBiasSumNm
                    / (double)externalTorqueBiasCount);
      } else {
        externalTorqueBiasNm = latestTorqueNm;
      }

      // Use the same startup motor-torque bias for observation-only RFOB.
      rfobMotorTorqueBiasNm = externalTorqueBiasNm;

      activeStarted = true;

      Serial.print("EVENT,TORQUE_BIAS_NM,");
      Serial.println(externalTorqueBiasNm, 6);
    }

    // 1) Direct measured-torque estimator and gain scheduling.
    externalTorqueEstimate =
        direct_external_torque_estimator(
            latestTorqueNm,
            latestSpeedRadS,
            gimpPos
        );

    externalTorqueRawNm = externalTorqueEstimate.raw_nm;
    externalTorqueZeroedNm = externalTorqueEstimate.zeroed_nm;
    externalTorqueFilt = externalTorqueEstimate.filtered_nm;
    externalTorqueDeadzoneNm = externalTorqueEstimate.deadzone_nm;
    gimpScheduledGain = externalTorqueEstimate.scheduled_gain;

    // The better-performing direct-torque path remains the active GIMP input.
    externalTorqueNm = externalTorqueEstimate.for_gimp_nm;

    // 2) RFOB runs independently for observation and logging.
    rfobEstimate =
        rfob_external_torque_estimator(
            latestTorqueNm,
            latestSpeedRadS
        );

    if (RFOB_DRIVES_GIMP) {
      externalTorqueNm =
          constrain(
              RFOB_TO_GIMP_SIGN
              * rfobEstimate.external_torque_nm,
              -GIMP_INPUT_LIMIT_NM,
               GIMP_INPUT_LIMIT_NM
          );
    }

    // 3) GIMP admittance model.
    gimpReference =
        gimp_admittance_model(externalTorqueNm);

    gimpRefRad = gimpReference.position_rad;
    targetPosRad = startPosRad + gimpReference.position_rad;

    // 4) Position controller.
    positionControllerOutput =
        position_controller(
            targetPosRad,
            latestPosRad
        );

    positionTorqueNm =
        positionControllerOutput.torque_nm;

    // 5) DOB.
    dobEstimate =
        dob_disturbance_estimator(
            latestPosRad - startPosRad,
            commandTorquePrevNm
        );

    dobEstNm = dobEstimate.disturbance_estimate_nm;
    dobCompNm = dobEstimate.compensation_nm;

    // 6) Motor-level compensation.
    motorCompensation =
        motor_compensation(
            gimpReference.velocity_rad_s,
            latestSpeedRadS,
            positionControllerOutput.error_rad,
            positionControllerOutput.torque_nm,
            gimpReference.acceleration_rad_s2
        );

    // 7) Explicit final torque assembly.
    torqueCommandTerms =
        assemble_torque_command(
            0.0f,
            0.0f,
            positionControllerOutput.torque_nm,
            dobEstimate.compensation_nm,
            motorCompensation
        );

    commandTorqueNm =
        torqueCommandTerms.saturated_total_nm;

    // 8) Safe motor output.
    if (!send_motor_torque(commandTorqueNm)) {
      commandTorqueNm = 0.0f;
    }

    commandTorquePrevNm = commandTorqueNm;
    return;
  }

  activeStarted = false;
  commandTorqueNm = 0.0f;
  commandTorquePrevNm = 0.0f;
  send_zero_torque();

  if (elapsedSec > START_DELAY_SEC + DURATION_SEC + STOP_DELAY_SEC) {
    exitMitMode();
  }
}

// ===================== Logging =====================
void printLog() {
  if (!feedbackReady) return;

  float posDeg = (latestPosRad - startPosRad) * RAD2DEG;
  float targetDeg = (targetPosRad - startPosRad) * RAD2DEG;
  float gimpDeg = gimpRefRad * RAD2DEG;
  float errDeg = (targetPosRad - latestPosRad) * RAD2DEG;

  Serial.print("DATA,");   // <-- Python 需要这个前缀

  Serial.print(millis()); Serial.print(",");
  Serial.print(targetDeg, 6); Serial.print(",");
  Serial.print(posDeg, 6); Serial.print(",");
  Serial.print(latestSpeedRadS, 6); Serial.print(",");
  Serial.print(positionTorqueNm, 6); Serial.print(",");
  Serial.print(externalTorqueRawNm, 6); Serial.print(",");
  Serial.print(externalTorqueBiasNm, 6); Serial.print(",");
  Serial.print(externalTorqueZeroedNm, 6); Serial.print(",");
  Serial.print(externalTorqueFilt, 6); Serial.print(",");
  Serial.print(externalTorqueDeadzoneNm, 6); Serial.print(",");
  Serial.print(gimpScheduledGain, 6); Serial.print(",");
  Serial.print(externalTorqueNm, 6); Serial.print(",");
  Serial.print(gimpDeg, 6); Serial.print(",");
  Serial.print(dobEstNm, 6); Serial.print(",");
  Serial.print(dobCompNm, 6); Serial.print(",");
  Serial.print(frictionFFNm, 6); Serial.print(",");
  Serial.print(breakawayNm, 6); Serial.print(",");
  Serial.print(accFFNm, 6); Serial.print(",");
  Serial.print(commandTorqueNm, 6); Serial.print(",");
  Serial.print(latestTorqueNm, 6); Serial.print(",");
  Serial.print(errDeg, 6); Serial.print(",");
  Serial.print(latestTempC, 2); Serial.print(",");
  Serial.print(latestErrorCode); Serial.print(",");
  Serial.print(DOB_ENABLE ? 1 : 0); Serial.print(",");

  Serial.print(motorState.feedback_time_us); Serial.print(",");
  Serial.print(motorState.feedback_age_us); Serial.print(",");
  Serial.print(motorState.feedback_count); Serial.print(",");
  Serial.print(motorState.feedback_valid ? 1 : 0); Serial.print(",");
  Serial.print(motorState.communication_timeout ? 1 : 0); Serial.print(",");
  Serial.print(motorSafety.motor_safe ? 1 : 0); Serial.print(",");
  Serial.print(motorSafety.event_count); Serial.print(",");

  Serial.print(gimpReference.velocity_rad_s, 6); Serial.print(",");
  Serial.print(gimpReference.acceleration_rad_s2, 6); Serial.print(",");

  Serial.print(dobEstimate.inverse_model_term_nm, 6); Serial.print(",");
  Serial.print(dobEstimate.command_filter_term_nm, 6); Serial.print(",");

  Serial.print(motorCompensation.coulomb_friction_nm, 6); Serial.print(",");
  Serial.print(motorCompensation.total_nm, 6); Serial.print(",");
  Serial.print(torqueCommandTerms.unsaturated_total_nm, 6); Serial.print(",");

  Serial.print(rfobEstimate.motor_torque_zeroed_nm, 6); Serial.print(",");
  Serial.print(rfobEstimate.friction_model_nm, 6); Serial.print(",");
  Serial.print(rfobEstimate.viscous_model_nm, 6); Serial.print(",");
  Serial.print(rfobEstimate.internal_resistance_nm, 6); Serial.print(",");
  Serial.print(rfobEstimate.filter_input_nm, 6); Serial.print(",");
  Serial.print(rfobEstimate.filter_state_nm, 6); Serial.print(",");
  Serial.print(rfobEstimate.external_torque_nm, 6); Serial.print(",");
  Serial.print(rfobEstimate.reaction_torque_nm, 6); Serial.print(",");
  Serial.print(RFOB_ENABLE ? 1 : 0); Serial.print(",");
  Serial.println(RFOB_DRIVES_GIMP ? 1 : 0);
}


bool projectEnabled = false;
uint32_t projectSessionStartMs = 0;

void projectInit() {
  Can1.begin();
  Can1.setBaudRate(CAN_BAUD);
  computeDobCoefficients();
  setupPositionController();
}

void projectStart() {
  externalTorqueBiasSumNm = 0.0;
  externalTorqueBiasCount = 0;
  externalTorqueBiasNm = 0.0f;
  globalZeroSet = false;
  activeStarted = false;
  resetControllerStates();
  motorSafety.emergency_stop = false;
  motorSafety.mit_exit_sent = false;
  projectSessionStartMs = millis();
  txTimer = 0;
  logTimer = 0;
  enterMitMode();
  projectEnabled = true;
  Serial.println("STATE,M1,ON,ID1,CAN1");
}

void projectStop() {
  if (projectEnabled) {
    // Bypass feedback gating so OFF always emits an actual zero-torque packet.
    sendMitCommandPacket(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    exitMitMode();
  }
  projectEnabled = false;
  activeStarted = false;
  commandTorqueNm = 0.0f;
  commandTorquePrevNm = 0.0f;
  Serial.println("STATE,M1,OFF,ID1,CAN1");
}

void projectService() {
  read_motor_state();
  if (!projectEnabled) return;

  const double elapsedSec = (millis() - projectSessionStartMs) / 1000.0;

  if (txTimer >= CONTROL_INTERVAL_US) {
    txTimer = 0;
    updateControl(elapsedSec);
  }

  if (logTimer >= LOG_INTERVAL_MS) {
    logTimer = 0;
    printLog();
  }
}

} // namespace Motor1
