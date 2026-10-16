#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <math.h>

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can1;

// ============================================================
//  QDD / AK80 friction identification test
//
//  Purpose:
//    Command trapezoidal position trajectories with different
//    constant velocity sections.
//
//    During the constant-velocity region, acceleration is nearly zero,
//    so the required torque can be used as friction torque data.
//
//  Test structure:
//    target position trajectory -> position controller -> motor torque
//
//  Disabled in this file:
//    GIMP
//    DOB
//    friction feedforward
//
//  Log only DATA lines for Python logger.
// ============================================================

// ===================== Basic settings =====================
const uint8_t MOTOR_ID = 2;
const uint32_t CAN_BAUD = 1000000;

elapsedMicros txTimer;
elapsedMillis logTimer;

const uint32_t CONTROL_INTERVAL_US = 333;   // about 3003 Hz
const uint32_t LOG_INTERVAL_MS = 5;         // 200 Hz log

const float RAD2DEG = 57.2957795131f;
const float DEG2RAD = 0.0174532925199f;

const float DT =
    static_cast<float>(CONTROL_INTERVAL_US) * 1.0e-6f;

// ===================== MIT mode range =====================
const float P_MIN = -12.5f;
const float P_MAX = 12.5f;
const float V_MIN = -45.0f;
const float V_MAX = 45.0f;
const float KP_MIN = 0.0f;
const float KP_MAX = 500.0f;
const float KD_MIN = 0.0f;
const float KD_MAX = 5.0f;
const float T_MIN = -18.0f;
const float T_MAX = 18.0f;

// ===================== Test timing =====================
const double START_DELAY_SEC = 1.0;
const double STOP_DELAY_SEC = 1.0;

// ===================== Safety =====================
// Start conservative. If tracking is too poor, increase gradually.
const float TORQUE_LIMIT_NM = 2.5f;

// ===================== Position controller Cpos(z) =====================
// From MATLAB, Ts = 0.0005 s
const float CPOS_B0 = 162.924659f;
const float CPOS_B1 = -324.081539523f;
const float CPOS_B2 = 161.164552756f;
const float CPOS_A1 = -1.92425290225f;
const float CPOS_A2 = 0.924252902251f;

// ============================================================
// Friction identification trajectory settings
// ============================================================
// Velocity list in deg/s. Positive and negative are alternated
// to avoid drifting too far from the initial position.
const float FRIC_TEST_VEL_DEG_S[] = {
   5.0f,   -5.0f,
  10.0f,  -10.0f,
  20.0f,  -20.0f,
  40.0f,  -40.0f,
  80.0f,  -80.0f,
 120.0f, -120.0f,
 160.0f, -160.0f,
 200.0f, -200.0f
};

const int NUM_FRIC_TESTS = sizeof(FRIC_TEST_VEL_DEG_S) / sizeof(FRIC_TEST_VEL_DEG_S[0]);

// Trapezoid velocity profile for each speed:
// ramp up -> constant velocity -> ramp down -> rest
const float FRIC_RAMP_TIME_S  = 0.50f;
const float FRIC_CONST_TIME_S = 1.50f;
const float FRIC_REST_TIME_S  = 0.80f;

// Use only the middle part of the constant velocity section for fitting.
// This avoids transient data near ramp/constant boundaries.
const float FRIC_CONST_MARGIN_S = 0.25f;

// If position error is too large, the logged data can be excluded in MATLAB.
// The code only logs this as a flag. It does not stop the test.
const float FRIC_MAX_ERROR_FOR_GOOD_DATA_DEG = 3.0f;

// ===================== State variables =====================
bool feedbackReady = false;
bool activeStarted = false;
bool testDone = false;

float latestPosRawRad = 0.0f;
float latestPosRad = 0.0f;
float lastPosRawRad = 0.0f;
float latestSpeedRadS = 0.0f;
float latestTorqueNm = 0.0f;
float latestTempC = 0.0f;
uint8_t latestErrorCode = 0;

float startPosRad = 0.0f;
float targetPosRad = 0.0f;
float targetPosRelRad = 0.0f;

float positionTorqueNm = 0.0f;
float commandTorqueNm = 0.0f;
bool torqueSaturated = false;

int fricTestIndex = 0;
float fricTestTime = 0.0f;
float fricCmdVelDegS = 0.0f;
float fricTargetVelRadS = 0.0f;
int fricPhase = 0;              // 0=ramp up, 1=constant, 2=ramp down, 3=rest, 4=done
bool fricConstVelFlag = false;  // true only in the middle of constant velocity region
bool fricGoodDataFlag = false;  // constant velocity and small tracking error

// ===================== IIR second order filter =====================
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
  while (Can1.read(msg)) {
    parseFeedback(msg);
  }
}

// ===================== Controller setup =====================
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

  targetPosRelRad = 0.0f;
  targetPosRad = startPosRad;

  positionTorqueNm = 0.0f;
  commandTorqueNm = 0.0f;
  torqueSaturated = false;

  fricTestIndex = 0;
  fricTestTime = 0.0f;
  fricCmdVelDegS = 0.0f;
  fricTargetVelRadS = 0.0f;
  fricPhase = 0;
  fricConstVelFlag = false;
  fricGoodDataFlag = false;
  testDone = false;
}

float runPositionController(float targetRad, float posRad) {
  float e = targetRad - posRad;
  return runIIR(cpos, e);
}

float saturateTorque(float tau) {
  torqueSaturated = false;

  if (tau > TORQUE_LIMIT_NM) {
    torqueSaturated = true;
    return TORQUE_LIMIT_NM;
  }

  if (tau < -TORQUE_LIMIT_NM) {
    torqueSaturated = true;
    return -TORQUE_LIMIT_NM;
  }

  return tau;
}

// ===================== Friction trajectory =====================
void updateFrictionTrajectory() {
  if (fricTestIndex >= NUM_FRIC_TESTS) {
    fricTargetVelRadS = 0.0f;
    fricCmdVelDegS = 0.0f;
    fricPhase = 4;
    fricConstVelFlag = false;
    testDone = true;
    return;
  }

  fricCmdVelDegS = FRIC_TEST_VEL_DEG_S[fricTestIndex];
  float vCmdRadS = fricCmdVelDegS * DEG2RAD;

  fricTestTime += DT;
  fricConstVelFlag = false;

  const float t1 = FRIC_RAMP_TIME_S;
  const float t2 = FRIC_RAMP_TIME_S + FRIC_CONST_TIME_S;
  const float t3 = 2.0f * FRIC_RAMP_TIME_S + FRIC_CONST_TIME_S;
  const float t4 = 2.0f * FRIC_RAMP_TIME_S + FRIC_CONST_TIME_S + FRIC_REST_TIME_S;

  if (fricTestTime < t1) {
    // Ramp up velocity from 0 to vCmd
    float ratio = fricTestTime / FRIC_RAMP_TIME_S;
    fricTargetVelRadS = ratio * vCmdRadS;
    fricPhase = 0;

  } else if (fricTestTime < t2) {
    // Constant velocity
    fricTargetVelRadS = vCmdRadS;
    fricPhase = 1;

    float tConst = fricTestTime - FRIC_RAMP_TIME_S;
    if (tConst > FRIC_CONST_MARGIN_S &&
        tConst < (FRIC_CONST_TIME_S - FRIC_CONST_MARGIN_S)) {
      fricConstVelFlag = true;
    }

  } else if (fricTestTime < t3) {
    // Ramp down velocity from vCmd to 0
    float tDown = fricTestTime - FRIC_RAMP_TIME_S - FRIC_CONST_TIME_S;
    float ratio = 1.0f - tDown / FRIC_RAMP_TIME_S;
    fricTargetVelRadS = ratio * vCmdRadS;
    fricPhase = 2;

  } else if (fricTestTime < t4) {
    // Rest / hold position
    fricTargetVelRadS = 0.0f;
    fricPhase = 3;

  } else {
    // Next velocity case
    fricTestIndex++;
    fricTestTime = 0.0f;
    fricTargetVelRadS = 0.0f;
    fricPhase = 3;
  }

  // Integrate target velocity to target position.
  targetPosRelRad += fricTargetVelRadS * DT;
  targetPosRad = startPosRad + targetPosRelRad;
}

// ===================== Main control =====================
void updateControl(double elapsedSec) {
  bool active = feedbackReady && elapsedSec >= START_DELAY_SEC && !testDone;

  if (active) {
    if (!activeStarted) {
      startPosRad = latestPosRad;
      targetPosRad = startPosRad;
      resetControllerStates();
      activeStarted = true;
    }

    updateFrictionTrajectory();

    // Position controller only. No GIMP, no DOB, no friction FF.
    positionTorqueNm = runPositionController(targetPosRad, latestPosRad);
    commandTorqueNm = saturateTorque(positionTorqueNm);

    float errDeg = (targetPosRad - latestPosRad) * RAD2DEG;
    fricGoodDataFlag = fricConstVelFlag &&
                       (!torqueSaturated) &&
                       (fabs(errDeg) <= FRIC_MAX_ERROR_FOR_GOOD_DATA_DEG);

    sendMitCommand(0.0f, 0.0f, 0.0f, 0.0f, commandTorqueNm);

  } else {
    commandTorqueNm = 0.0f;
    positionTorqueNm = 0.0f;
    fricTargetVelRadS = 0.0f;
    fricConstVelFlag = false;
    fricGoodDataFlag = false;

    if (testDone) {
      fricPhase = 4;
    }

    sendMitCommand(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    if (testDone && elapsedSec > START_DELAY_SEC + STOP_DELAY_SEC) {
      // Keep sending zero torque after test completion.
      activeStarted = false;
    }
  }
}

// ===================== Logging =====================
void printLog() {
  if (!feedbackReady) return;

  float posDeg = (latestPosRad - startPosRad) * RAD2DEG;
  float targetDeg = (targetPosRad - startPosRad) * RAD2DEG;
  float errDeg = (targetPosRad - latestPosRad) * RAD2DEG;
  float measuredVelDegS = latestSpeedRadS * RAD2DEG;

  Serial.print("DATA,");

  Serial.print(millis()); Serial.print(",");
  Serial.print(fricTestIndex); Serial.print(",");
  Serial.print(fricPhase); Serial.print(",");
  Serial.print(fricConstVelFlag ? 1 : 0); Serial.print(",");
  Serial.print(fricGoodDataFlag ? 1 : 0); Serial.print(",");
  Serial.print(fricCmdVelDegS, 6); Serial.print(",");
  Serial.print(fricTargetVelRadS, 6); Serial.print(",");
  Serial.print(latestSpeedRadS, 6); Serial.print(",");
  Serial.print(measuredVelDegS, 6); Serial.print(",");
  Serial.print(targetDeg, 6); Serial.print(",");
  Serial.print(posDeg, 6); Serial.print(",");
  Serial.print(errDeg, 6); Serial.print(",");
  Serial.print(positionTorqueNm, 6); Serial.print(",");
  Serial.print(commandTorqueNm, 6); Serial.print(",");
  Serial.print(latestTorqueNm, 6); Serial.print(",");
  Serial.print(torqueSaturated ? 1 : 0); Serial.print(",");
  Serial.print(latestTempC, 2); Serial.print(",");
  Serial.print(latestErrorCode); Serial.print(",");
  Serial.println(testDone ? 1 : 0);
}

// ===================== Arduino =====================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Can1.begin();
  Can1.setBaudRate(CAN_BAUD);

  setupPositionController();

  enterMitMode();
  delay(100);

  Serial.println("Time_ms,Fric_Test_Index,Fric_Phase,Fric_ConstVel_Flag,Fric_GoodData_Flag,Fric_Cmd_Vel_deg_s,Fric_Target_Vel_rad_s,Measured_Speed_rad_s,Measured_Speed_deg_s,Target_Position_deg,Position_deg,Position_Error_deg,Position_Control_Torque_Nm,Command_Torque_Nm,Measured_Torque_Nm,Torque_Saturated,Temperature_C,Error_Code,Test_Done");
}

void loop() {
  readCANMessages();

  double elapsedSec = millis() / 1000.0;

  if (txTimer >= CONTROL_INTERVAL_US) {
    txTimer = 0;
    updateControl(elapsedSec);
  }

  if (logTimer >= LOG_INTERVAL_MS) {
    logTimer = 0;
    printLog();
  }
}
