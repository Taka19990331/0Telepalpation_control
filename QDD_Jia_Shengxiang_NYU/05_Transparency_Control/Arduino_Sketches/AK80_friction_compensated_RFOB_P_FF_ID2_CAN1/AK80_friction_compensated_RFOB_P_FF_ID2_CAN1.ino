#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <math.h>

// ============================================================
// AK80 RFOB + ASYMMETRIC STRIBECK v5 - STABLE IDLE
// Motor ID = 2, CAN1, 1 Mbps, MIT mode
//
// Goal:
//   1) When nobody touches the motor: stay quiet and output ~0 torque.
//   2) When the shaft is moved: friction compensation comes in smoothly.
//   3) Keep the implementation simple: no Kalman filter, no dither,
//      no position-direction state machine.
//
// Main idea:
//   - 15 Hz LPF on 12-bit MIT velocity.
//   - Velocity deadband around zero to reject quantization noise.
//   - RFOB torque deadband to reject observer noise.
//   - Smooth friction engagement outside the deadband.
//   - If RFOB clearly detects an external torque before velocity becomes
//     reliable, it may also smoothly request static-friction assistance.
// ============================================================

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> CanBus;

// ===================== CAN / MIT =====================
constexpr uint8_t MOTOR_ID = 2;
constexpr uint32_t CAN_BAUD = 1000000;

constexpr float P_MIN  = -12.5f;
constexpr float P_MAX  =  12.5f;
constexpr float V_MIN  = -45.0f;
constexpr float V_MAX  =  45.0f;
constexpr float KP_MIN =   0.0f;
constexpr float KP_MAX = 500.0f;
constexpr float KD_MIN =   0.0f;
constexpr float KD_MAX =   5.0f;
constexpr float T_MIN  = -32.0f;
constexpr float T_MAX  =  32.0f;

// ===================== Identified plant =====================
constexpr float J_NOMINAL = 0.0116809040f;

// ===================== Identified asymmetric Stribeck model =====================
// Positive direction
constexpr float TS_P_NM     = 2.5934000000e-01f;
constexpr float FC_P_NM     = 8.6923385825e-02f;
constexpr float VS_P_RAD_S  = 3.2345688808e-01f;
constexpr float B_P_NMS_RAD = 4.7038532737e-03f;

// Negative direction
constexpr float TS_N_NM     = 4.2637400000e-01f;
constexpr float FC_N_NM     = 1.0601495644e-01f;
constexpr float VS_N_RAD_S  = 2.0988215029e-01f;
constexpr float B_N_NMS_RAD = 1.6877512420e-03f;

// ===================== SIMPLE STABILITY SETTINGS =====================
bool frictionFFEnabled = true;
bool rfobEnabled = true;

// 12-bit MIT velocity resolution is about 0.022 rad/s.
// A 15 Hz LPF removes most single-code jitter without becoming absurdly slow.
constexpr float VELOCITY_LPF_HZ = 15.0f;

// Treat the zero-speed quantization region as exactly zero.
constexpr float VELOCITY_DEADBAND_RAD_S = 0.001f;

// Friction compensation grows smoothly from 0 at the deadband edge to
// full Stribeck compensation at this speed. This avoids a torque step.
constexpr float FRICTION_FULL_SPEED_RAD_S = 0.075f;

// Friction command itself gets a fast LPF. This is only to remove abrupt
// sample-to-sample changes, not to deliberately delay compensation.
constexpr float FRICTION_COMMAND_LPF_HZ = 70.0f;
constexpr float FRICTION_GAIN = 1.00f;
constexpr float FRICTION_LIMIT_NM = 0.52f;

// RFOB noise below this level is treated as zero.
constexpr float RFOB_DEADBAND_NM = 0.0025f;

// If the RFOB detects a real external torque while velocity is still inside
// the deadband, use it as a smooth low-speed friction intent cue.
// This is optional assistance; idle noise remains zero because of the deadband.
constexpr float RFOB_FRICTION_FULL_NM = 0.12f;

// ===================== RFOB =====================
constexpr float ACCEL_CUTOFF_HZ = 12.0f;
constexpr float RFOB_CUTOFF_HZ  = 20.0f;
constexpr float ZERO_TORQUE_KP   = 1.0f;
constexpr float CONTROL_SIGN     = 1.0f;

// Conservative while tuning.
constexpr float TORQUE_LIMIT_NM = 1.0f;

// ===================== Timing =====================
constexpr uint32_t COMMAND_PERIOD_US = 333;      // ~3 kHz
constexpr uint32_t FEEDBACK_TIMEOUT_US = 20000;
constexpr uint32_t LOG_PERIOD_US = 10000;        // 100 Hz

// ============================================================
// STATE
// ============================================================
struct MotorState {
  bool valid = false;
  bool positionInitialized = false;

  float positionRawRad = 0.0f;
  float previousPositionRawRad = 0.0f;
  float positionRad = 0.0f;

  float speedRadS = 0.0f;
  float measuredTorqueNm = 0.0f;
  float temperatureC = 0.0f;
  uint8_t errorCode = 0;
  uint32_t lastFeedbackUs = 0;
};

MotorState motor;

bool controlEnabled = false;
bool observerInitialized = false;

float motorTorqueBiasNm = 0.0f;
float motorTorqueZeroedNm = 0.0f;

float filteredVelocityRadS = 0.0f;
float velocityForControlRadS = 0.0f;
float previousFilteredVelocityRadS = 0.0f;
float rawAccelerationRadS2 = 0.0f;
float filteredAccelerationRadS2 = 0.0f;

float observerFrictionNm = 0.0f;
float rawReactionTorqueNm = 0.0f;
float estimatedReactionTorqueNm = 0.0f;
float reactionTorqueUsedNm = 0.0f;
float rfobControlTorqueNm = 0.0f;

float speedFrictionActivation = 0.0f;
float rfobFrictionActivation = 0.0f;
float frictionActivation = 0.0f;
float frictionDirection = 0.0f;
float frictionModelMagnitudeNm = 0.0f;
float frictionTargetTorqueNm = 0.0f;
float frictionCompTorqueNm = 0.0f;

float torqueCommandNm = 0.0f;

uint32_t previousObserverUs = 0;
elapsedMicros commandTimer;
uint32_t lastPrintUs = 0;

// ============================================================
// HELPERS
// ============================================================
float lowPassAlpha(float cutoffHz, float dt) {
  if (cutoffHz <= 0.0f) return 1.0f;
  const float tau = 1.0f / (2.0f * PI * cutoffHz);
  return dt / (tau + dt);
}

float signFloat(float x) {
  if (x > 0.0f) return 1.0f;
  if (x < 0.0f) return -1.0f;
  return 0.0f;
}

float smoothStep01(float x) {
  x = constrain(x, 0.0f, 1.0f);
  return x * x * (3.0f - 2.0f * x);
}

// Continuous deadband: no output inside deadband, no jump at the edge.
float applyContinuousDeadband(float x, float deadband) {
  const float a = fabsf(x);
  if (a <= deadband) return 0.0f;
  return signFloat(x) * (a - deadband);
}

float smoothActivation(float value, float startValue, float fullValue) {
  if (value <= startValue) return 0.0f;
  if (value >= fullValue) return 1.0f;
  const float x = (value - startValue) / (fullValue - startValue);
  return smoothStep01(x);
}

uint16_t floatToUint(float x, float xMin, float xMax, uint8_t bits) {
  x = constrain(x, xMin, xMax);
  const uint32_t maxInt = (1UL << bits) - 1UL;
  return static_cast<uint16_t>(
      (x - xMin) * static_cast<float>(maxInt) / (xMax - xMin));
}

float uintToFloat(uint16_t xInt, float xMin, float xMax, uint8_t bits) {
  const uint32_t maxInt = (1UL << bits) - 1UL;
  return static_cast<float>(xInt) * (xMax - xMin)
       / static_cast<float>(maxInt) + xMin;
}

// ============================================================
// AK80 MIT COMMANDS
// ============================================================
bool sendMIT(float p, float v, float kp, float kd, float torque) {
  const uint16_t pInt  = floatToUint(p,      P_MIN,  P_MAX,  16);
  const uint16_t vInt  = floatToUint(v,      V_MIN,  V_MAX,  12);
  const uint16_t kpInt = floatToUint(kp,     KP_MIN, KP_MAX, 12);
  const uint16_t kdInt = floatToUint(kd,     KD_MIN, KD_MAX, 12);
  const uint16_t tInt  = floatToUint(torque, T_MIN,  T_MAX,  12);

  CAN_message_t msg{};
  msg.id = MOTOR_ID;
  msg.flags.extended = 0;
  msg.flags.remote = 0;
  msg.len = 8;

  msg.buf[0] = pInt >> 8;
  msg.buf[1] = pInt & 0xFF;
  msg.buf[2] = vInt >> 4;
  msg.buf[3] = ((vInt & 0x0F) << 4) | (kpInt >> 8);
  msg.buf[4] = kpInt & 0xFF;
  msg.buf[5] = kdInt >> 4;
  msg.buf[6] = ((kdInt & 0x0F) << 4) | (tInt >> 8);
  msg.buf[7] = tInt & 0xFF;

  return CanBus.write(msg);
}

void sendSpecialCommand(uint8_t lastByte) {
  CAN_message_t msg{};
  msg.id = MOTOR_ID;
  msg.flags.extended = 0;
  msg.flags.remote = 0;
  msg.len = 8;
  for (int i = 0; i < 7; ++i) msg.buf[i] = 0xFF;
  msg.buf[7] = lastByte;
  CanBus.write(msg);
}

void enterMITMode() { sendSpecialCommand(0xFC); }
void exitMITMode()  { sendSpecialCommand(0xFD); }
void sendZeroTorque() { sendMIT(0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }

// ============================================================
// POSITION UNWRAP
// ============================================================
void updateUnwrappedPosition(float rawRad) {
  const float wrap = P_MAX - P_MIN;
  motor.positionRawRad = rawRad;

  if (!motor.positionInitialized) {
    motor.previousPositionRawRad = rawRad;
    motor.positionRad = rawRad;
    motor.positionInitialized = true;
    return;
  }

  float diff = rawRad - motor.previousPositionRawRad;
  if (diff > wrap * 0.5f) diff -= wrap;
  if (diff < -wrap * 0.5f) diff += wrap;

  motor.positionRad += diff;
  motor.previousPositionRawRad = rawRad;
}

// ============================================================
// ASYMMETRIC STRIBECK MODEL
// ============================================================
float positiveFrictionMagnitude(float speedAbsRadS) {
  const float w = fabsf(speedAbsRadS);
  const float r = w / VS_P_RAD_S;
  return FC_P_NM
       + (TS_P_NM - FC_P_NM) * expf(-(r * r))
       + B_P_NMS_RAD * w;
}

float negativeFrictionMagnitude(float speedAbsRadS) {
  const float w = fabsf(speedAbsRadS);
  const float r = w / VS_N_RAD_S;
  return FC_N_NM
       + (TS_N_NM - FC_N_NM) * expf(-(r * r))
       + B_N_NMS_RAD * w;
}

float frictionMagnitudeForDirection(float speedAbsRadS, float direction) {
  if (direction > 0.0f) return positiveFrictionMagnitude(speedAbsRadS);
  if (direction < 0.0f) return negativeFrictionMagnitude(speedAbsRadS);
  return 0.0f;
}

// ============================================================
// CONTROL / OBSERVER
// ============================================================
void resetControlState() {
  observerInitialized = false;

  filteredVelocityRadS = 0.0f;
  velocityForControlRadS = 0.0f;
  previousFilteredVelocityRadS = 0.0f;
  rawAccelerationRadS2 = 0.0f;
  filteredAccelerationRadS2 = 0.0f;

  observerFrictionNm = 0.0f;
  rawReactionTorqueNm = 0.0f;
  estimatedReactionTorqueNm = 0.0f;
  reactionTorqueUsedNm = 0.0f;
  rfobControlTorqueNm = 0.0f;

  speedFrictionActivation = 0.0f;
  rfobFrictionActivation = 0.0f;
  frictionActivation = 0.0f;
  frictionDirection = 0.0f;
  frictionModelMagnitudeNm = 0.0f;
  frictionTargetTorqueNm = 0.0f;
  frictionCompTorqueNm = 0.0f;

  torqueCommandNm = 0.0f;
}

void updateObserverAndControl() {
  const uint32_t nowUs = micros();

  if (!observerInitialized) {
    previousObserverUs = nowUs;
    filteredVelocityRadS = motor.speedRadS;
    previousFilteredVelocityRadS = filteredVelocityRadS;
    observerInitialized = true;
    return;
  }

  const float dt = static_cast<float>(nowUs - previousObserverUs) * 1.0e-6f;
  if (dt <= 0.0f || dt > 0.05f) {
    previousObserverUs = nowUs;
    previousFilteredVelocityRadS = filteredVelocityRadS;
    return;
  }
  previousObserverUs = nowUs;

  // ----------------------------------------------------------
  // 1) Simple velocity LPF.
  // ----------------------------------------------------------
  const float velAlpha = lowPassAlpha(VELOCITY_LPF_HZ, dt);
  filteredVelocityRadS +=
      velAlpha * (motor.speedRadS - filteredVelocityRadS);

  // Continuous deadband for control/observer use.
  velocityForControlRadS =
      applyContinuousDeadband(filteredVelocityRadS,
                              VELOCITY_DEADBAND_RAD_S);

  // ----------------------------------------------------------
  // 2) Acceleration from the filtered velocity.
  // ----------------------------------------------------------
  rawAccelerationRadS2 =
      (filteredVelocityRadS - previousFilteredVelocityRadS) / dt;
  previousFilteredVelocityRadS = filteredVelocityRadS;

  const float accAlpha = lowPassAlpha(ACCEL_CUTOFF_HZ, dt);
  filteredAccelerationRadS2 +=
      accAlpha * (rawAccelerationRadS2 - filteredAccelerationRadS2);

  // ----------------------------------------------------------
  // 3) Learn motor torque zero bias only while control is OFF.
  // ----------------------------------------------------------
  if (!controlEnabled && fabsf(filteredVelocityRadS) < 0.08f) {
    constexpr float BIAS_ALPHA = 0.01f;
    motorTorqueBiasNm +=
        BIAS_ALPHA * (motor.measuredTorqueNm - motorTorqueBiasNm);
  }
  motorTorqueZeroedNm = motor.measuredTorqueNm - motorTorqueBiasNm;

  // ----------------------------------------------------------
  // 4) RFOB.
  // Observer friction is also faded to zero near standstill so
  // quantized velocity does not create a fake reaction torque.
  // ----------------------------------------------------------
  const float absVelFilt = fabsf(filteredVelocityRadS);
  const float velDirection = signFloat(velocityForControlRadS);

  speedFrictionActivation = smoothActivation(
      absVelFilt,
      VELOCITY_DEADBAND_RAD_S,
      FRICTION_FULL_SPEED_RAD_S);

  const float observerFrictionMag =
      frictionMagnitudeForDirection(absVelFilt, velDirection);
  observerFrictionNm =
      speedFrictionActivation * velDirection * observerFrictionMag;

  rawReactionTorqueNm =
      motorTorqueZeroedNm
      - J_NOMINAL * filteredAccelerationRadS2
      - observerFrictionNm;

  const float rfobAlpha = lowPassAlpha(RFOB_CUTOFF_HZ, dt);
  estimatedReactionTorqueNm +=
      rfobAlpha * (rawReactionTorqueNm - estimatedReactionTorqueNm);

  // Continuous RFOB deadband: stable at idle, no torque step at the edge.
  reactionTorqueUsedNm =
      applyContinuousDeadband(estimatedReactionTorqueNm,
                              RFOB_DEADBAND_NM);

  if (controlEnabled && rfobEnabled) {
    rfobControlTorqueNm =
        CONTROL_SIGN * ZERO_TORQUE_KP * (-reactionTorqueUsedNm);
  } else {
    rfobControlTorqueNm = 0.0f;
  }

  // ----------------------------------------------------------
  // 5) Friction compensation.
  //
  // Main cue: filtered velocity outside the deadband.
  // Optional low-speed cue: a real RFOB control effort can smoothly
  // activate static-friction assistance before velocity is reliable.
  // ----------------------------------------------------------
  rfobFrictionActivation = 0.0f;
  if (fabsf(rfobControlTorqueNm) > 0.0f) {
    rfobFrictionActivation = smoothActivation(
        fabsf(reactionTorqueUsedNm),
        0.0f,
        RFOB_FRICTION_FULL_NM);
  }

  frictionActivation = max(speedFrictionActivation,
                           rfobFrictionActivation);

  // Prefer measured motion direction. Only use RFOB direction if velocity
  // is still inside the deadband.
  if (velDirection != 0.0f) {
    frictionDirection = velDirection;
  } else if (rfobFrictionActivation > 0.0f) {
    frictionDirection = signFloat(rfobControlTorqueNm);
  } else {
    frictionDirection = 0.0f;
  }

  frictionModelMagnitudeNm =
      frictionMagnitudeForDirection(absVelFilt, frictionDirection);

  if (controlEnabled && frictionFFEnabled && frictionDirection != 0.0f) {
    frictionTargetTorqueNm =
        FRICTION_GAIN
        * frictionActivation
        * frictionDirection
        * frictionModelMagnitudeNm;

    frictionTargetTorqueNm = constrain(
        frictionTargetTorqueNm,
        -FRICTION_LIMIT_NM,
         FRICTION_LIMIT_NM);
  } else {
    frictionTargetTorqueNm = 0.0f;
  }

  const float ffAlpha = lowPassAlpha(FRICTION_COMMAND_LPF_HZ, dt);
  frictionCompTorqueNm +=
      ffAlpha * (frictionTargetTorqueNm - frictionCompTorqueNm);

  // ----------------------------------------------------------
  // 6) Final torque command.
  // No dither. If both velocity and RFOB are in their deadbands,
  // both terms naturally go to zero.
  // ----------------------------------------------------------
  if (controlEnabled) {
    torqueCommandNm = rfobControlTorqueNm + frictionCompTorqueNm;
    torqueCommandNm = constrain(torqueCommandNm,
                                -TORQUE_LIMIT_NM,
                                 TORQUE_LIMIT_NM);
  } else {
    torqueCommandNm = 0.0f;
  }
}

// ============================================================
// FEEDBACK
// ============================================================
void decodeFeedback(const CAN_message_t &msg) {
  if (msg.flags.extended) return;
  if (msg.len < 6) return;
  if (msg.id != MOTOR_ID && msg.buf[0] != MOTOR_ID) return;

  const uint16_t pInt =
      (static_cast<uint16_t>(msg.buf[1]) << 8) | msg.buf[2];
  const uint16_t vInt =
      (static_cast<uint16_t>(msg.buf[3]) << 4) | (msg.buf[4] >> 4);
  const uint16_t tInt =
      ((static_cast<uint16_t>(msg.buf[4]) & 0x0F) << 8) | msg.buf[5];

  const float rawPositionRad = uintToFloat(pInt, P_MIN, P_MAX, 16);
  updateUnwrappedPosition(rawPositionRad);

  motor.speedRadS = uintToFloat(vInt, V_MIN, V_MAX, 12);
  motor.measuredTorqueNm = uintToFloat(tInt, T_MIN, T_MAX, 12);
  if (msg.len >= 7) motor.temperatureC = static_cast<float>(msg.buf[6]);
  if (msg.len >= 8) motor.errorCode = msg.buf[7];

  motor.lastFeedbackUs = micros();
  motor.valid = true;

  updateObserverAndControl();
}

void readCAN() {
  CAN_message_t msg{};
  while (CanBus.read(msg)) decodeFeedback(msg);
}

// ============================================================
// SERIAL CONTROL
// ============================================================
void startControl() {
  if (!motor.valid) {
    Serial.println("Cannot start: no AK80 feedback.");
    return;
  }

  if (motor.errorCode != 0) {
    Serial.print("Cannot start: motor error = ");
    Serial.println(motor.errorCode);
    return;
  }

  resetControlState();
  controlEnabled = true;
  Serial.println("AK80 STABLE-IDLE RFOB + STRIBECK ON");
}

void stopControl() {
  controlEnabled = false;
  resetControlState();
  sendZeroTorque();
  Serial.println("AK80 CONTROL OFF / ZERO TORQUE");
}

void toggleFriction() {
  frictionFFEnabled = !frictionFFEnabled;
  Serial.print("Friction FF = ");
  Serial.println(frictionFFEnabled ? "ON" : "OFF");
}

void toggleRFOB() {
  rfobEnabled = !rfobEnabled;
  Serial.print("RFOB = ");
  Serial.println(rfobEnabled ? "ON" : "OFF");
}

// ============================================================
// LOGGING
// ============================================================
void printHeader() {
  Serial.println(
      "Time_ms,Control_Enabled,FrictionFF_Enabled,RFOB_Enabled,"
      "Position_rad,Velocity_Raw_rad_s,Velocity_Filt_rad_s,Velocity_Used_rad_s,"
      "Measured_Torque_Nm,Torque_Bias_Nm,Torque_Zeroed_Nm,Acceleration_rad_s2,"
      "Observer_Friction_Nm,RFOB_Raw_Nm,RFOB_Estimated_Nm,RFOB_Used_Nm,RFOB_Control_Nm,"
      "Speed_Friction_Activation,RFOB_Friction_Activation,Friction_Activation,Friction_Direction,"
      "Friction_Model_Mag_Nm,Friction_Target_Nm,Friction_Comp_Nm,Torque_Command_Nm,"
      "Temperature_C,Error_Code");
}

void printData() {
  Serial.print(millis()); Serial.print(',');
  Serial.print(controlEnabled ? 1 : 0); Serial.print(',');
  Serial.print(frictionFFEnabled ? 1 : 0); Serial.print(',');
  Serial.print(rfobEnabled ? 1 : 0); Serial.print(',');

  Serial.print(motor.positionRad, 6); Serial.print(',');
  Serial.print(motor.speedRadS, 6); Serial.print(',');
  Serial.print(filteredVelocityRadS, 6); Serial.print(',');
  Serial.print(velocityForControlRadS, 6); Serial.print(',');

  Serial.print(motor.measuredTorqueNm, 6); Serial.print(',');
  Serial.print(motorTorqueBiasNm, 6); Serial.print(',');
  Serial.print(motorTorqueZeroedNm, 6); Serial.print(',');
  Serial.print(filteredAccelerationRadS2, 6); Serial.print(',');

  Serial.print(observerFrictionNm, 6); Serial.print(',');
  Serial.print(rawReactionTorqueNm, 6); Serial.print(',');
  Serial.print(estimatedReactionTorqueNm, 6); Serial.print(',');
  Serial.print(reactionTorqueUsedNm, 6); Serial.print(',');
  Serial.print(rfobControlTorqueNm, 6); Serial.print(',');

  Serial.print(speedFrictionActivation, 6); Serial.print(',');
  Serial.print(rfobFrictionActivation, 6); Serial.print(',');
  Serial.print(frictionActivation, 6); Serial.print(',');
  Serial.print(frictionDirection, 3); Serial.print(',');
  Serial.print(frictionModelMagnitudeNm, 6); Serial.print(',');
  Serial.print(frictionTargetTorqueNm, 6); Serial.print(',');
  Serial.print(frictionCompTorqueNm, 6); Serial.print(',');
  Serial.print(torqueCommandNm, 6); Serial.print(',');

  Serial.print(motor.temperatureC, 1); Serial.print(',');
  Serial.println(motor.errorCode);
}

// ============================================================
// SETUP / LOOP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  CanBus.begin();
  CanBus.setBaudRate(CAN_BAUD);
  delay(200);

  enterMITMode();
  delay(100);
  sendZeroTorque();

  Serial.println();
  Serial.println("AK80 RFOB + STRIBECK v5 STABLE IDLE");
  Serial.println("Motor ID = 2, CAN1, 1 Mbps");
  Serial.println("No Kalman, no dither, no position-direction state machine.");
  Serial.print("Velocity LPF = ");
  Serial.print(VELOCITY_LPF_HZ, 1);
  Serial.println(" Hz");
  Serial.print("Velocity deadband = +/-");
  Serial.print(VELOCITY_DEADBAND_RAD_S, 3);
  Serial.println(" rad/s");
  Serial.print("RFOB deadband = +/-");
  Serial.print(RFOB_DEADBAND_NM, 3);
  Serial.println(" Nm");
  Serial.println();
  Serial.println("s = enable control");
  Serial.println("x = disable / zero torque");
  Serial.println("f = toggle friction FF");
  Serial.println("r = toggle RFOB");
  Serial.println();

  printHeader();
}

void loop() {
  CanBus.events();
  readCAN();

  if (Serial.available()) {
    const char c = Serial.read();
    if (c == 's' || c == 'S') startControl();
    else if (c == 'x' || c == 'X') stopControl();
    else if (c == 'f' || c == 'F') toggleFriction();
    else if (c == 'r' || c == 'R') toggleRFOB();
  }

  while (commandTimer >= COMMAND_PERIOD_US) {
    commandTimer -= COMMAND_PERIOD_US;

    const bool feedbackLost =
        !motor.valid ||
        (micros() - motor.lastFeedbackUs > FEEDBACK_TIMEOUT_US);

    if (feedbackLost || motor.errorCode != 0) {
      torqueCommandNm = 0.0f;
      sendZeroTorque();
      continue;
    }

    if (controlEnabled) {
      sendMIT(0.0f, 0.0f, 0.0f, 0.0f, torqueCommandNm);
    } else {
      sendZeroTorque();
    }
  }

  if (micros() - lastPrintUs >= LOG_PERIOD_US) {
    lastPrintUs = micros();
    printData();
  }
}