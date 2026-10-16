#pragma once

namespace Mode1Motor3 {

// ============================================================
// AK60-6 DOB + GIMP + Friction FF + Acceleration FF
//
// PURPOSE
//   Port the SAME DOB/GIMP control logic used on the previous motors
//   to the current AK60-6, while keeping the proven AK60 CAN protocol.
//
// CONTROL CHAIN
//   measured motor torque
//      -> startup bias removal
//      -> LPF
//      -> soft deadzone
//      -> low-speed/small-torque gain scheduling
//      -> GIMP admittance model
//      -> Cpos(z) position controller
//      -> DOB compensation
//      -> friction + breakaway compensation
//      -> acceleration feedforward
//      -> final torque command
//
// FINAL COMMAND
//   tau_cmd = tau_pos + tau_DOB + tau_friction + tau_accFF
//
// NO RFOB is used in this program.
//
// IMPORTANT
//   DOB and acceleration-FF values below are BASELINE values copied
//   from the previous successful DOB/GIMP logic. They are not claimed
//   to be the final tuned AK60 values yet.
// ============================================================

// ============================================================
// CAN: proven AK60 hardware/protocol path
// ============================================================
FlexCAN_T4<CAN3, RX_SIZE_256, TX_SIZE_16> CanBus;

constexpr uint32_t CAN_BAUD = 1000000;
constexpr uint8_t MOTOR_ID = 3;

// AK60 V3.x extended MIT command and servo feedback IDs
constexpr uint32_t MIT_COMMAND_ID = (8UL << 8) | MOTOR_ID;       // 0x803
constexpr uint32_t FEEDBACK_ID    = (0x29UL << 8) | MOTOR_ID;    // 0x2903

elapsedMicros txTimer;
elapsedMillis logTimer;

// ============================================================
// Timing: same DOB/GIMP logic
// ============================================================
constexpr float CONTROL_FREQ_HZ = 3000.0f;
constexpr float DT = 1.0f / CONTROL_FREQ_HZ;
constexpr uint32_t CONTROL_INTERVAL_US = 333;   // ~3 kHz
constexpr uint32_t LOG_INTERVAL_MS = 5;         // 200 Hz
constexpr float RAD2DEG = 57.2957795131f;

// ============================================================
// AK60 MIT ranges
// ============================================================
constexpr float P_MIN  = -12.56f;
constexpr float P_MAX  =  12.56f;
constexpr float V_MIN  = -60.0f;
constexpr float V_MAX  =  60.0f;
constexpr float KP_MIN = 0.0f;
constexpr float KP_MAX = 500.0f;
constexpr float KD_MIN = 0.0f;
constexpr float KD_MAX = 5.0f;
constexpr float T_MIN  = -12.0f;
constexpr float T_MAX  =  12.0f;

// ============================================================
// Test timing
// ============================================================
constexpr double START_DELAY_SEC = 1.0;
constexpr double DURATION_SEC = 300.0;
constexpr double STOP_DELAY_SEC = 1.0;

// ============================================================
// Safety / limits
// ============================================================
constexpr float TORQUE_LIMIT_NM = 20.0f;
constexpr uint32_t FEEDBACK_TIMEOUT_US = 50000;

// ============================================================
// AK60 identified nominal plant
// Pn(s) = 1 / (J s^2 + D s)
// ============================================================
constexpr float J_NOM = 0.0042243000f;   // kg*m^2
constexpr float D_NOM = 0.0523667000f;   // Nm*s/rad
constexpr float KT_NOMINAL = 0.5994f;    // Nm/A

constexpr float MOTOR_POLE_PAIRS = 14.0f;
constexpr float GEAR_RATIO = 6.0f;

// ============================================================
// GIMP model
// Same structure as previous DOB/GIMP program.
// Start with M = identified J and C = identified D.
// K_IMP remains a tuning parameter.
// ============================================================
constexpr float M_IMP = J_NOM;
constexpr float C_IMP = D_NOM*5.0;
constexpr float K_IMP = 4.0f;

// ============================================================
// Direct external-torque estimate for GIMP input
// ============================================================
constexpr float EXTERNAL_TORQUE_SIGN = -1.0f;
constexpr float EXT_TORQUE_LPF_ALPHA = 0.02f;

// ============================================================
// GIMP input bias removal + deadzone + gain scheduling
// ============================================================
constexpr bool GIMP_GAIN_SCHED_ENABLE = true;
constexpr bool AUTO_TORQUE_BIAS_ENABLE = true;

constexpr float GIMP_TORQUE_DEADZONE_NM = 0.03f;

// Conservative initial AK60 baseline.
constexpr float GIMP_GAIN_MAX = 1.5f;
constexpr float GIMP_GAIN_TORQUE_SCALE_NM = 0.15f;
constexpr float GIMP_GAIN_SPEED_SCALE_RAD_S = 0.10f;
constexpr float GIMP_GAIN_POSITION_CENTER_RAD = 2.6f * PI / 180.0f;
constexpr float GIMP_GAIN_POSITION_WIDTH_RAD  = 0.5f * PI / 180.0f;
constexpr float GIMP_INPUT_LIMIT_NM = 1.0f;

// ============================================================
// AK60 Cpos(z)
// Previously identified AK60 coefficients.
// ============================================================
constexpr float CPOS_B0 =  97.8639137161f;
constexpr float CPOS_B1 = -194.378485371f;
constexpr float CPOS_B2 =  96.5215930017f;
constexpr float CPOS_A1 = -1.92348013188f;
constexpr float CPOS_A2 =  0.923480131878f;

// ============================================================
// DOB settings
// SAME logic/baseline as previous motor.
// These are starting values, not yet final AK60 tuning.
// ============================================================
constexpr bool DOB_ENABLE = true;
constexpr float DOB_GAIN = 1.0f;
constexpr float DOB_COMP_SIGN = -1.0f;
constexpr float DOB_Q_FREQ_HZ = 1.0f;
constexpr float DOB_Q_ZETA = 0.7f;

// ============================================================
// AK60 identified friction parameters
// ============================================================
constexpr bool FRICTION_FF_ENABLE = true;

// Dynamic Coulomb friction
constexpr float TAU_C_P_NM = 0.1388340675f;
constexpr float TAU_C_N_NM = 0.1358578172f;

// Identified viscous friction
constexpr float B_F_P_NMS_RAD = 0.0014878030f;
constexpr float B_F_N_NMS_RAD = 0.0014533901f;

// Static / breakaway friction
constexpr float TAU_S_P_NM = 0.203796f;
constexpr float TAU_S_N_NM = 0.209790f;

// Smooth dynamic friction transition.
// tanh(w/omega0), equivalent to k ~= 20 s/rad when omega0 = 0.05.
constexpr float OMEGA0_RAD_S = 0.05f;

// Breakaway conditions: same logic as previous controller.
constexpr float OMEGA_TH_RAD_S = 0.20f;
constexpr float ERROR_TH_RAD = 0.20f * PI / 180.0f;

constexpr float FRICTION_TOTAL_LIMIT_NM = 0.30f;
constexpr float FRICTION_SIGN = 1.0f;

// ============================================================
// Acceleration feedforward
// SAME logic as previous motor:
//   tau_acc_ff = J_ACC_FF * gimpAcc
// Uses REFERENCE acceleration from GIMP, not measured acceleration.
// Baseline only; tune later if needed.
// ============================================================
constexpr bool ACC_FF_ENABLE = true;
constexpr float J_ACC_FF = J_NOM;
constexpr float ACC_FF_LIMIT_NM = 0.15f;

// ============================================================
// State variables
// ============================================================
bool feedbackReady = false;
bool activeStarted = false;

float latestPosRad = 0.0f;
float latestSpeedRadS = 0.0f;
float latestIqA = 0.0f;
float latestTorqueNm = 0.0f;
float latestTempC = 0.0f;
uint8_t latestErrorCode = 0;
uint32_t latestFeedbackUs = 0;

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
float externalTorqueNm = 0.0f;
float gimpScheduledGain = 1.0f;

float gimpPos = 0.0f;
float gimpVel = 0.0f;
float gimpAcc = 0.0f;

float positionTorqueNm = 0.0f;
float dobEstNm = 0.0f;
float dobCompNm = 0.0f;

float frictionFFNm = 0.0f;
float coulombNm = 0.0f;
float viscousNm = 0.0f;
float breakawayNm = 0.0f;
float accFFNm = 0.0f;

float commandTorqueNm = 0.0f;
float commandTorquePrevNm = 0.0f;

float globalZeroRad = 0.0f;
bool globalZeroSet = false;

// ============================================================
// IIR second-order filter
// ============================================================
struct IIR2 {
  float b0, b1, b2;
  float a1, a2;
  float x1, x2;
  float y1, y2;
};

IIR2 dobQ{};
IIR2 dobQPinv{};
IIR2 cpos{};

void resetIIR(IIR2 &f) {
  f.x1 = 0.0f;
  f.x2 = 0.0f;
  f.y1 = 0.0f;
  f.y2 = 0.0f;
}

float runIIR(IIR2 &f, float x) {
  const float y =
      f.b0 * x
      + f.b1 * f.x1
      + f.b2 * f.x2
      - f.a1 * f.y1
      - f.a2 * f.y2;

  f.x2 = f.x1;
  f.x1 = x;
  f.y2 = f.y1;
  f.y1 = y;

  return y;
}

// ============================================================
// Helpers
// ============================================================
uint16_t floatToUint(float x, float xMin, float xMax, int bits) {
  const float xConstrained = constrain(x, xMin, xMax);
  const float span = xMax - xMin;
  return (uint16_t)((xConstrained - xMin)
      * (float)((1UL << bits) - 1UL) / span);
}

float degToRadF(float deg) {
  return deg * PI / 180.0f;
}

float erpmToOutputRadS(float erpm) {
  const float outputRpm = erpm / MOTOR_POLE_PAIRS / GEAR_RATIO;
  return outputRpm * (2.0f * PI / 60.0f);
}

float signFloat(float x) {
  if (x > 0.0f) return 1.0f;
  if (x < 0.0f) return -1.0f;
  return 0.0f;
}

// ============================================================
// AK60 MIT command
// EXACT packing style used by the proven AK60 code:
// Kp -> Kd -> Position -> Velocity -> Torque
// ============================================================
bool sendMIT(float p, float v, float kp, float kd, float torque) {
  const uint16_t pInt  = floatToUint(p,      P_MIN,  P_MAX,  16);
  const uint16_t vInt  = floatToUint(v,      V_MIN,  V_MAX,  12);
  const uint16_t kpInt = floatToUint(kp,     KP_MIN, KP_MAX, 12);
  const uint16_t kdInt = floatToUint(kd,     KD_MIN, KD_MAX, 12);
  const uint16_t tInt  = floatToUint(torque, T_MIN,  T_MAX,  12);

  CAN_message_t msg{};
  msg.id = MIT_COMMAND_ID;
  msg.flags.extended = 1;
  msg.flags.remote = 0;
  msg.len = 8;

  msg.buf[0] = kpInt >> 4;
  msg.buf[1] = ((kpInt & 0x0F) << 4) | ((kdInt >> 8) & 0x0F);
  msg.buf[2] = kdInt & 0xFF;
  msg.buf[3] = pInt >> 8;
  msg.buf[4] = pInt & 0xFF;
  msg.buf[5] = vInt >> 4;
  msg.buf[6] = ((vInt & 0x0F) << 4) | ((tInt >> 8) & 0x0F);
  msg.buf[7] = tInt & 0xFF;

  return CanBus.write(msg);
}

void sendZeroTorque() {
  sendMIT(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}

// ============================================================
// AK60 periodic servo feedback: EXT ID 0x2903
// Data0/1 position int16 * 0.1 deg
// Data2/3 speed    int16 * 10 ERPM
// Data4/5 current  int16 * 0.01 A
// Data6 temperature
// Data7 error
// ============================================================
void decodeFeedback(const CAN_message_t &msg) {
  if (!msg.flags.extended) return;
  if (msg.id != FEEDBACK_ID) return;
  if (msg.len != 8) return;

  const int16_t positionRaw = (int16_t)(
      ((uint16_t)msg.buf[0] << 8) | msg.buf[1]);

  const int16_t speedRaw = (int16_t)(
      ((uint16_t)msg.buf[2] << 8) | msg.buf[3]);

  const int16_t currentRaw = (int16_t)(
      ((uint16_t)msg.buf[4] << 8) | msg.buf[5]);

  const float positionDeg = (float)positionRaw * 0.1f;
  const float erpm = (float)speedRaw * 10.0f;

  latestPosRad = degToRadF(positionDeg);
  latestSpeedRadS = erpmToOutputRadS(erpm);
  latestIqA = (float)currentRaw * 0.01f;
  latestTorqueNm = KT_NOMINAL * latestIqA;
  latestTempC = (float)msg.buf[6];
  latestErrorCode = msg.buf[7];
  latestFeedbackUs = micros();
  feedbackReady = true;
}

void readCANMessages() {
  CAN_message_t msg{};
  while (CanBus.read(msg)) {
    decodeFeedback(msg);
  }
}

// ============================================================
// DOB coefficient setup
// SAME professor-style implementation as previous controller:
//   d_hat = Q * (Pn^-1*x - u)
// ============================================================
void computeDobCoefficients() {
  const float Ts = DT;
  const float KtBilinear = 2.0f / Ts;
  const float w = 2.0f * PI * DOB_Q_FREQ_HZ;
  const float z = DOB_Q_ZETA;

  const float den0 =
      KtBilinear * KtBilinear
      + 2.0f * z * w * KtBilinear
      + w * w;

  const float den1 =
      -2.0f * KtBilinear * KtBilinear
      + 2.0f * w * w;

  const float den2 =
      KtBilinear * KtBilinear
      - 2.0f * z * w * KtBilinear
      + w * w;

  // Q(s)
  dobQ.b0 = (w * w) / den0;
  dobQ.b1 = (2.0f * w * w) / den0;
  dobQ.b2 = (w * w) / den0;
  dobQ.a1 = den1 / den0;
  dobQ.a2 = den2 / den0;
  resetIIR(dobQ);

  // Q(s) * Pn^-1(s), Pn^-1 = J*s^2 + D*s
  dobQPinv.b0 =
      (J_NOM * w * w * KtBilinear * KtBilinear
       + D_NOM * w * w * KtBilinear) / den0;

  dobQPinv.b1 =
      (-2.0f * J_NOM * w * w * KtBilinear * KtBilinear) / den0;

  dobQPinv.b2 =
      (J_NOM * w * w * KtBilinear * KtBilinear
       - D_NOM * w * w * KtBilinear) / den0;

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
  coulombNm = 0.0f;
  viscousNm = 0.0f;
  breakawayNm = 0.0f;
  accFFNm = 0.0f;

  commandTorqueNm = 0.0f;
  commandTorquePrevNm = 0.0f;
}

// ============================================================
// GIMP admittance model
// Gimp(s) = 1/(M s^2 + C s + K)
// ============================================================
float runGimpFilter(float inputTorqueNm) {
  gimpAcc =
      (inputTorqueNm - C_IMP * gimpVel - K_IMP * gimpPos)
      / M_IMP;

  gimpVel += gimpAcc * DT;
  gimpPos += gimpVel * DT;

  return gimpPos;
}

// ============================================================
// Cpos(z)
// ============================================================
float runPositionController(float targetRad, float posRad) {
  const float errorRad = targetRad - posRad;
  return runIIR(cpos, errorRad);
}

// ============================================================
// Friction direction logic: same as previous controller
// ============================================================
float getFrictionDirectionFromControl(float positionTorque,
                                      float posErrorRad,
                                      float refVelRadS) {
  constexpr float POS_TORQUE_EPS_NM = 0.05f;
  constexpr float REF_VEL_EPS_RAD_S = 0.03f;

  if (fabsf(positionTorque) > POS_TORQUE_EPS_NM) {
    return signFloat(positionTorque);
  }

  if (fabsf(refVelRadS) > REF_VEL_EPS_RAD_S) {
    return signFloat(refVelRadS);
  }

  if (fabsf(posErrorRad) > ERROR_TH_RAD) {
    return signFloat(posErrorRad);
  }

  return 0.0f;
}

// ============================================================
// AK60 friction + breakaway compensation
// Same logic as previous controller, with AK60 identified parameters.
// ============================================================
float computeFrictionWithBreakaway(float frictionVelRadS,
                                   float measuredVelRadS,
                                   float posErrorRad,
                                   float positionTorque,
                                   float refVelRadS) {
  if (!FRICTION_FF_ENABLE) {
    coulombNm = 0.0f;
    viscousNm = 0.0f;
    breakawayNm = 0.0f;
    return 0.0f;
  }

  const float dir = getFrictionDirectionFromControl(
      positionTorque,
      posErrorRad,
      refVelRadS);

  // 1) Dynamic Coulomb friction, using GIMP reference velocity magnitude.
  coulombNm = 0.0f;

  if (dir > 0.0f) {
    coulombNm =
        TAU_C_P_NM
        * tanhf(fabsf(frictionVelRadS) / OMEGA0_RAD_S);
  } else if (dir < 0.0f) {
    coulombNm =
        -TAU_C_N_NM
        * tanhf(fabsf(frictionVelRadS) / OMEGA0_RAD_S);
  }

  // 2) Same minimum-help logic used previously around startup.
  constexpr float TORQUE_HELP_TH_NM = 0.08f;
  constexpr float MIN_FRICTION_P_NM = 0.08f;
  constexpr float MIN_FRICTION_N_NM = 0.08f;

  if (fabsf(positionTorque) > TORQUE_HELP_TH_NM) {
    if (dir > 0.0f && coulombNm < MIN_FRICTION_P_NM) {
      coulombNm = MIN_FRICTION_P_NM;
    } else if (dir < 0.0f && coulombNm > -MIN_FRICTION_N_NM) {
      coulombNm = -MIN_FRICTION_N_NM;
    }
  }

  // 3) AK60 viscous friction identified from the torque-ramp experiment.
  viscousNm = 0.0f;
  if (dir > 0.0f) {
    viscousNm = B_F_P_NMS_RAD * frictionVelRadS;
  } else if (dir < 0.0f) {
    viscousNm = B_F_N_NMS_RAD * frictionVelRadS;
  }

  // 4) Static breakaway compensation.
  // Use measured speed to decide whether the motor is physically stuck.
  breakawayNm = 0.0f;

  if (fabsf(measuredVelRadS) < OMEGA_TH_RAD_S
      && fabsf(positionTorque) > TORQUE_HELP_TH_NM
      && fabsf(posErrorRad) > ERROR_TH_RAD) {

    if (dir > 0.0f) {
      breakawayNm = TAU_S_P_NM;
    } else if (dir < 0.0f) {
      breakawayNm = -TAU_S_N_NM;
    }
  }

  float tau = coulombNm + viscousNm + breakawayNm;
  tau *= FRICTION_SIGN;

  return constrain(
      tau,
      -FRICTION_TOTAL_LIMIT_NM,
       FRICTION_TOTAL_LIMIT_NM);
}

// ============================================================
// GIMP input processing
// ============================================================
float applySoftDeadzone(float value, float deadzone) {
  const float magnitude = fabsf(value);
  if (magnitude <= deadzone) return 0.0f;
  return copysignf(magnitude - deadzone, value);
}

float computeGimpScheduledGain(float torqueNm,
                               float measuredVelocityRadS,
                               float previousGimpPositionRad) {
  if (!GIMP_GAIN_SCHED_ENABLE) return 1.0f;

  const float torqueScale = fmaxf(GIMP_GAIN_TORQUE_SCALE_NM, 1.0e-6f);
  const float speedScale = fmaxf(GIMP_GAIN_SPEED_SCALE_RAD_S, 1.0e-6f);
  const float positionWidth = fmaxf(GIMP_GAIN_POSITION_WIDTH_RAD, 1.0e-6f);

  const float torqueRatio = fabsf(torqueNm) / torqueScale;
  const float speedRatio = fabsf(measuredVelocityRadS) / speedScale;

  const float torqueWeight = expf(-torqueRatio * torqueRatio);
  const float speedWeight = expf(-speedRatio * speedRatio);

  const float positionArgument =
      (fabsf(previousGimpPositionRad) - GIMP_GAIN_POSITION_CENTER_RAD)
      / positionWidth;

  const float positionWeight = 0.5f * (1.0f - tanhf(positionArgument));

  const float scheduledGain =
      1.0f
      + (GIMP_GAIN_MAX - 1.0f)
      * torqueWeight
      * speedWeight
      * positionWeight;

  return constrain(scheduledGain, 1.0f, GIMP_GAIN_MAX);
}

// ============================================================
// Main controller
// ============================================================
void updateControl(double elapsedSec) {
  // Startup motor-torque bias average while untouched/stationary.
  if (feedbackReady
      && AUTO_TORQUE_BIAS_ENABLE
      && !activeStarted
      && elapsedSec < START_DELAY_SEC) {
    externalTorqueBiasSumNm += latestTorqueNm;
    externalTorqueBiasCount++;
  }

  const bool feedbackFresh =
      feedbackReady
      && ((uint32_t)(micros() - latestFeedbackUs) <= FEEDBACK_TIMEOUT_US)
      && (latestErrorCode == 0);

  const bool active =
      feedbackFresh
      && elapsedSec >= START_DELAY_SEC
      && elapsedSec <= (START_DELAY_SEC + DURATION_SEC);

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
            (float)(externalTorqueBiasSumNm
            / (double)externalTorqueBiasCount);
      } else {
        externalTorqueBiasNm = latestTorqueNm;
      }

      activeStarted = true;

      Serial.print("EVENT,TORQUE_BIAS_NM,");
      Serial.println(externalTorqueBiasNm, 6);
    }

    // --------------------------------------------------------
    // 1) Direct measured-torque estimator for GIMP input.
    // --------------------------------------------------------
    externalTorqueRawNm = EXTERNAL_TORQUE_SIGN * latestTorqueNm;

    externalTorqueZeroedNm =
        EXTERNAL_TORQUE_SIGN
        * (latestTorqueNm - externalTorqueBiasNm);

    externalTorqueFilt +=
        EXT_TORQUE_LPF_ALPHA
        * (externalTorqueZeroedNm - externalTorqueFilt);

    externalTorqueDeadzoneNm =
        applySoftDeadzone(
            externalTorqueFilt,
            GIMP_TORQUE_DEADZONE_NM);

    gimpScheduledGain =
        computeGimpScheduledGain(
            externalTorqueDeadzoneNm,
            latestSpeedRadS,
            gimpPos);

    externalTorqueNm =
        gimpScheduledGain * externalTorqueDeadzoneNm;

    externalTorqueNm =
        constrain(
            externalTorqueNm,
            -GIMP_INPUT_LIMIT_NM,
             GIMP_INPUT_LIMIT_NM);

    // --------------------------------------------------------
    // 2) GIMP generates reference position, velocity, acceleration.
    // --------------------------------------------------------
    gimpRefRad = runGimpFilter(externalTorqueNm);
    targetPosRad = startPosRad + gimpRefRad;

    // --------------------------------------------------------
    // 3) Cpos tracks the GIMP position reference.
    // --------------------------------------------------------
    positionTorqueNm =
        runPositionController(targetPosRad, latestPosRad);

    // --------------------------------------------------------
    // 4) Professor-style DOB.
    //    d_hat = Q * (Pn^-1*x - u_prev)
    // --------------------------------------------------------
    const float xRel = latestPosRad - startPosRad;

    dobEstNm =
        runIIR(dobQPinv, xRel)
        - runIIR(dobQ, commandTorquePrevNm);

    dobCompNm =
        DOB_ENABLE
        ? DOB_COMP_SIGN * DOB_GAIN * dobEstNm
        : 0.0f;

    // --------------------------------------------------------
    // 5) Friction + breakaway compensation.
    //    Same signal logic as previous controller:
    //    reference velocity for dynamic magnitude,
    //    measured velocity for breakaway detection.
    // --------------------------------------------------------
    const float posErrorRad = targetPosRad - latestPosRad;

    frictionFFNm =
        computeFrictionWithBreakaway(
            gimpVel,
            latestSpeedRadS,
            posErrorRad,
            positionTorqueNm,
            gimpVel);

    // --------------------------------------------------------
    // 6) Acceleration feedforward.
    //    Same logic as before: reference acceleration only.
    // --------------------------------------------------------
    if (ACC_FF_ENABLE) {
      accFFNm = J_ACC_FF * gimpAcc;
      accFFNm = constrain(
          accFFNm,
          -ACC_FF_LIMIT_NM,
           ACC_FF_LIMIT_NM);
    } else {
      accFFNm = 0.0f;
    }

    // --------------------------------------------------------
    // 7) Final torque assembly.
    // --------------------------------------------------------
    commandTorqueNm =
        positionTorqueNm
        + dobCompNm
        + frictionFFNm
        + accFFNm;

    commandTorqueNm =
        constrain(
            commandTorqueNm,
            -TORQUE_LIMIT_NM,
             TORQUE_LIMIT_NM);

    // AK60 proven torque-only MIT command.
    sendMIT(
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        commandTorqueNm);

    commandTorquePrevNm = commandTorqueNm;

  } else {
    sendZeroTorque();
    commandTorquePrevNm = 0.0f;

    if (elapsedSec > (START_DELAY_SEC + DURATION_SEC + STOP_DELAY_SEC)) {
      activeStarted = false;
    }
  }
}

// ============================================================
// Logging
// ============================================================
void printLog() {
  if (!feedbackReady) return;

  const float posDeg = (latestPosRad - startPosRad) * RAD2DEG;
  const float targetDeg = (targetPosRad - startPosRad) * RAD2DEG;
  const float gimpDeg = gimpRefRad * RAD2DEG;
  const float errDeg = (targetPosRad - latestPosRad) * RAD2DEG;
  const uint32_t feedbackAgeUs = (uint32_t)(micros() - latestFeedbackUs);

  Serial.print("DATA,");
  Serial.print(millis()); Serial.print(",");
  Serial.print(targetDeg, 6); Serial.print(",");
  Serial.print(posDeg, 6); Serial.print(",");
  Serial.print(latestSpeedRadS, 6); Serial.print(",");
  Serial.print(latestIqA, 6); Serial.print(",");
  Serial.print(latestTorqueNm, 6); Serial.print(",");
  Serial.print(positionTorqueNm, 6); Serial.print(",");
  Serial.print(externalTorqueRawNm, 6); Serial.print(",");
  Serial.print(externalTorqueBiasNm, 6); Serial.print(",");
  Serial.print(externalTorqueZeroedNm, 6); Serial.print(",");
  Serial.print(externalTorqueFilt, 6); Serial.print(",");
  Serial.print(externalTorqueDeadzoneNm, 6); Serial.print(",");
  Serial.print(gimpScheduledGain, 6); Serial.print(",");
  Serial.print(externalTorqueNm, 6); Serial.print(",");
  Serial.print(gimpDeg, 6); Serial.print(",");
  Serial.print(gimpVel, 6); Serial.print(",");
  Serial.print(gimpAcc, 6); Serial.print(",");
  Serial.print(dobEstNm, 6); Serial.print(",");
  Serial.print(dobCompNm, 6); Serial.print(",");
  Serial.print(coulombNm, 6); Serial.print(",");
  Serial.print(viscousNm, 6); Serial.print(",");
  Serial.print(breakawayNm, 6); Serial.print(",");
  Serial.print(frictionFFNm, 6); Serial.print(",");
  Serial.print(accFFNm, 6); Serial.print(",");
  Serial.print(commandTorqueNm, 6); Serial.print(",");
  Serial.print(errDeg, 6); Serial.print(",");
  Serial.print(latestTempC, 2); Serial.print(",");
  Serial.print(latestErrorCode); Serial.print(",");
  Serial.print(DOB_ENABLE ? 1 : 0); Serial.print(",");
  Serial.print(ACC_FF_ENABLE ? 1 : 0); Serial.print(",");
  Serial.println(feedbackAgeUs);
}


bool projectEnabled = false;
uint32_t projectSessionStartMs = 0;

void projectInit() {
  CanBus.begin();
  CanBus.setBaudRate(CAN_BAUD);
  computeDobCoefficients();
  setupPositionController();
  sendZeroTorque();
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
  sendZeroTorque();
  projectEnabled = true;
  Serial.println("STATE,M3,ON,ID3,CAN3");
}

void projectStop() {
  sendZeroTorque();
  projectEnabled = false;
  activeStarted = false;
  commandTorqueNm = 0.0f;
  commandTorquePrevNm = 0.0f;
  Serial.println("STATE,M3,OFF,ID3,CAN3");
}

void projectService() {
  CanBus.events();
  readCANMessages();
  if (!projectEnabled) return;

  const double elapsedSec = (millis() - projectSessionStartMs) / 1000.0;

  while (txTimer >= CONTROL_INTERVAL_US) {
    txTimer -= CONTROL_INTERVAL_US;
    updateControl(elapsedSec);
  }

  if (logTimer >= LOG_INTERVAL_MS) {
    logTimer = 0;
    printLog();
  }
}

} // namespace Motor3
