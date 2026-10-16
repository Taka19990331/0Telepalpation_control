#pragma once

namespace Mode1Motor2 {

FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> Can2;

// ===================== Basic settings =====================
const uint8_t MOTOR_ID = 2;
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

// ===================== Identified nominal plant =====================
// Pn(s) = 1 / (J s^2 + D s)
// Identified: P(s)=87.613/(s^2+22.8889s)
const float J_NOM = 0.0116809040f;
const float D_NOM = 0.0347639547f;

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
const float GIMP_GAIN_MAX = 1.0f;

// Gain smoothly returns toward 1 as torque and speed increase.
const float GIMP_GAIN_TORQUE_SCALE_NM = 0.15f;
const float GIMP_GAIN_SPEED_SCALE_RAD_S = 0.10f;

// Gain also returns toward 1 when the GIMP displacement reaches this region.
const float GIMP_GAIN_POSITION_CENTER_RAD = 2.6f * PI / 180.0f;
const float GIMP_GAIN_POSITION_WIDTH_RAD  = 0.5f * PI / 180.0f;

// Safety clamp on the torque signal supplied to the GIMP model.
const float GIMP_INPUT_LIMIT_NM = 10.0f;

// ===================== Position controller Cpos(z) =====================
// From MATLAB, Ts = 0.0003 s
constexpr float CPOS_B0 = 135.952074016f;
constexpr float CPOS_B1 = -270.604208551f;
constexpr float CPOS_B2 = 134.656651889f;
constexpr float CPOS_A1 = -1.9424385461f;
constexpr float CPOS_A2 = 0.942438546105f;

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
const float TAU_C_P_NM = 0.12f;
const float TAU_C_N_NM = 0.116f;

// Smooth tanh transition speed
const float OMEGA0_RAD_S = 0.01f;

// Static / breakaway compensation
const float TAU_S_P_NM = 0.23f;
const float TAU_S_N_NM = 0.22f;

// Breakaway trigger condition
const float OMEGA_TH_RAD_S = 0.20f;
const float ERROR_TH_RAD = 0.20f * PI / 180.0f;

// Safety limit for total friction compensation
const float FRICTION_TOTAL_LIMIT_NM = 0.30f;

// Flip to -1.0f only if compensation direction is experimentally wrong
const float FRICTION_SIGN = 1.0f;

// ===================== Acceleration feedforward =====================
// Uses GIMP reference acceleration, not measured acceleration.
// tau_acc_ff = J_ACC_FF * gimpAcc
// Start conservative. Identified J is about 0.0114 Nm*s^2/rad.
const bool ACC_FF_ENABLE = true;
const float J_ACC_FF = J_NOM;
const float ACC_FF_LIMIT_NM = 0.15f;

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

  Can2.write(msg);
}

void sendSpecialCommand(uint8_t lastByte) {
  CAN_message_t msg;
  msg.id = MOTOR_ID;
  msg.len = 8;
  for (int i = 0; i < 7; i++) msg.buf[i] = 0xFF;
  msg.buf[7] = lastByte;
  Can2.write(msg);
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

// ===================== Feedback =====================
void updateUnwrappedPosition(float rawRad) {
  const float wrap = P_MAX - P_MIN;

  if (!feedbackReady) {
    latestPosRad = rawRad;
    lastPosRawRad = rawRad;
    feedbackReady = true;
    return;
  }

  float diff = rawRad - lastPosRawRad;

  if (diff > wrap / 2.0f) diff -= wrap;
  if (diff < -wrap / 2.0f) diff += wrap;

  latestPosRad += diff;
  lastPosRawRad = rawRad;
}

void parseFeedback(const CAN_message_t &msg) {
  if (msg.len < 6) return;
  if (msg.id != MOTOR_ID && msg.buf[0] != MOTOR_ID) return;

  uint16_t pInt = ((uint16_t)msg.buf[1] << 8) | msg.buf[2];
  uint16_t vInt = ((uint16_t)msg.buf[3] << 4) | (msg.buf[4] >> 4);
  uint16_t tInt = (((uint16_t)msg.buf[4] & 0x0F) << 8) | msg.buf[5];

  latestPosRawRad = uintToFloat(pInt, P_MIN, P_MAX, 16);
  latestSpeedRadS = uintToFloat(vInt, V_MIN, V_MAX, 12);
  latestTorqueNm = uintToFloat(tInt, T_MIN, T_MAX, 12);

  if (msg.len >= 7) latestTempC = (float)msg.buf[6];
  if (msg.len >= 8) latestErrorCode = msg.buf[7];

  updateUnwrappedPosition(latestPosRawRad);
}

void readCANMessages() {
  CAN_message_t msg;
  while (Can2.read(msg)) {
    parseFeedback(msg);
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
}

float runGimpFilter(float inputTorqueNm) {
  const float Ts = DT;

  // Save GIMP reference acceleration for acceleration feedforward.
  // This is desired/reference acceleration, not noisy measured acceleration.
  gimpAcc = (inputTorqueNm - C_IMP * gimpVel - K_IMP * gimpPos) / M_IMP;

  gimpVel += gimpAcc * Ts;
  gimpPos += gimpVel * Ts;

  return gimpPos;
}

float runPositionController(float targetRad, float posRad) {
  float e = targetRad - posRad;
  return runIIR(cpos, e);
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

// ===================== Main control =====================
void updateControl(double elapsedSec) {
  // Collect a startup torque-feedback average while the mechanism is
  // untouched and stationary. This is safer than capturing one noisy sample.
  if (feedbackReady &&
      AUTO_TORQUE_BIAS_ENABLE &&
      !activeStarted &&
      elapsedSec < START_DELAY_SEC) {
    externalTorqueBiasSumNm += latestTorqueNm;
    externalTorqueBiasCount++;
  }

  bool active = feedbackReady &&
                elapsedSec >= START_DELAY_SEC &&
                elapsedSec <= (START_DELAY_SEC + DURATION_SEC);

  if (active) {
    if (!activeStarted) {
      resetControllerStates();

      if (!globalZeroSet) {
        globalZeroRad = latestPosRad;
        globalZeroSet = true;
      }

      startPosRad = globalZeroRad;
      targetPosRad = startPosRad;

      if (AUTO_TORQUE_BIAS_ENABLE && externalTorqueBiasCount > 0) {
        externalTorqueBiasNm =
            (float)(externalTorqueBiasSumNm /
                    (double)externalTorqueBiasCount);
      } else {
        externalTorqueBiasNm = latestTorqueNm;
      }

      activeStarted = true;

      Serial.print("EVENT,TORQUE_BIAS_NM,");
      Serial.println(externalTorqueBiasNm, 6);
    }

    // -----------------------------------------------------
    // 1) Temporary external-torque input conditioning.
    // -----------------------------------------------------
    externalTorqueRawNm =
        EXTERNAL_TORQUE_SIGN * latestTorqueNm;

    externalTorqueZeroedNm =
        EXTERNAL_TORQUE_SIGN
        * (latestTorqueNm - externalTorqueBiasNm);

    externalTorqueFilt +=
        EXT_TORQUE_LPF_ALPHA
        * (externalTorqueZeroedNm - externalTorqueFilt);

    externalTorqueDeadzoneNm =
        applySoftDeadzone(
            externalTorqueFilt,
            GIMP_TORQUE_DEADZONE_NM
        );

    gimpScheduledGain =
        computeGimpScheduledGain(
            externalTorqueDeadzoneNm,
            latestSpeedRadS,
            gimpPos
        );

    externalTorqueNm =
        gimpScheduledGain * externalTorqueDeadzoneNm;

    externalTorqueNm =
        constrain(
            externalTorqueNm,
            -GIMP_INPUT_LIMIT_NM,
            GIMP_INPUT_LIMIT_NM
        );

    // -----------------------------------------------------
    // 2) GIMP generates reference position.
    // -----------------------------------------------------
    gimpRefRad = runGimpFilter(externalTorqueNm);
  
    targetPosRad = startPosRad + gimpRefRad;

    // -----------------------------------------------------
    // 3) Position controller tracks GIMP reference.
    // -----------------------------------------------------
    positionTorqueNm = runPositionController(targetPosRad, latestPosRad);

    // -----------------------------------------------------
    // 4) Professor-style DOB estimate.
    //    d_hat = Q * (Pn^-1*x - u)
    //    Here u is the previous motor command torque.
    // -----------------------------------------------------
    float xRel = latestPosRad - startPosRad;
    dobEstNm = runIIR(dobQPinv, xRel) - runIIR(dobQ, commandTorquePrevNm);

    if (DOB_ENABLE) {
      dobCompNm = DOB_COMP_SIGN * DOB_GAIN * dobEstNm;
    } else {
      dobCompNm = 0.0f;
    }

    // -----------------------------------------------------
    // 5) Friction feedforward + breakaway compensation.
    //    This is added as a feedforward torque term.
    //
    //    Use GIMP reference velocity for friction FF so the compensation
    //    follows the desired motion direction instead of waiting for the
    //    measured motor velocity to move. This helps around reversal/deadband.
    // -----------------------------------------------------
    float posErrorRad = targetPosRad - latestPosRad;

    // Use GIMP reference velocity for dynamic friction magnitude,
    // but use measured motor velocity for breakaway low-speed detection.
    float frictionVelRadS = gimpVel;
    float measuredVelForBreakawayRadS = latestSpeedRadS;

    frictionFFNm = computeFrictionWithBreakaway(
      frictionVelRadS,
      measuredVelForBreakawayRadS,
      posErrorRad,
      positionTorqueNm,
      gimpVel
    );

    // -----------------------------------------------------
    // 6) Acceleration feedforward.
    //    tau_acc_ff = J_ACC_FF * gimpAcc
    //    Uses GIMP reference acceleration, not measured acceleration.
    // -----------------------------------------------------
    if (ACC_FF_ENABLE) {
      accFFNm = J_ACC_FF * gimpAcc;
      accFFNm = constrain(accFFNm, -ACC_FF_LIMIT_NM, ACC_FF_LIMIT_NM);
    } else {
      accFFNm = 0.0f;
    }

    // -----------------------------------------------------
    // 7) Final command torque.
    //    command = position torque + DOB compensation + friction FF + acceleration FF.
    // -----------------------------------------------------
    commandTorqueNm = positionTorqueNm + dobCompNm + frictionFFNm + accFFNm;
    commandTorqueNm = constrain(commandTorqueNm, -TORQUE_LIMIT_NM, TORQUE_LIMIT_NM);

    sendMitCommand(0.0f, 0.0f, 0.0f, 0.0f, commandTorqueNm);
    commandTorquePrevNm = commandTorqueNm;

  } else {
    sendMitCommand(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    if (elapsedSec > (START_DELAY_SEC + DURATION_SEC + STOP_DELAY_SEC)) {
      activeStarted = false;
    }
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
  Serial.println(DOB_ENABLE ? 1 : 0);
}


bool projectEnabled = false;
uint32_t projectSessionStartMs = 0;

void projectInit() {
  Can2.begin();
  Can2.setBaudRate(CAN_BAUD);
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
  projectSessionStartMs = millis();
  txTimer = 0;
  logTimer = 0;
  enterMitMode();
  projectEnabled = true;
  Serial.println("STATE,M2,ON,ID2,CAN2");
}

void projectStop() {
  if (projectEnabled) {
    sendMitCommand(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    exitMitMode();
  }
  projectEnabled = false;
  activeStarted = false;
  commandTorqueNm = 0.0f;
  commandTorquePrevNm = 0.0f;
  Serial.println("STATE,M2,OFF,ID2,CAN2");
}

void projectService() {
  readCANMessages();
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

} // namespace Motor2
