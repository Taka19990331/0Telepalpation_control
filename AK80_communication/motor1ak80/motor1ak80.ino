#include <Arduino.h>
#include <FlexCAN_T4.h>

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can0;

//=====================
// Settings
//=====================
const uint32_t CAN_BAUD = 1000000;

// MIT mode uses standard CAN ID.
// This must match the ID set in Upper Computer.
const uint32_t MOTOR_ID = 2;

// AK80-8 MIT mode range
const float P_MIN  = -12.5f;
const float P_MAX  =  12.5f;
const float V_MIN  = -37.5f;
const float V_MAX  =  37.5f;
const float T_MIN  = -32.0f;
const float T_MAX  =  32.0f;
const float KP_MIN = 0.0f;
const float KP_MAX = 500.0f;
const float KD_MIN = 0.0f;
const float KD_MAX = 5.0f;

// Chirp settings
const double startFreq = 1;
const double endFreq = 100.0;
const double duration = 100.0;
const double startDelaySec = 2.0;
const double stopDelaySec = 2.0;   // chirp结束后再等10秒自动停止

// MIT torque disturbance amplitude [N.m]
const float CHIRP_TORQUE_AMPLITUDE = 2.0f;

// External PD torque controller
// Unit: torque command from position error and speed feedback
const float KP_PD = 0.04f;       // [N.m/deg], initial safe value
const float KD_PD = 0.2f;    // [N.m/(rad/s)], initial safe value

// Safety torque limit for PD + chirp
const float TORQUE_LIMIT_NM = 20.0f;

elapsedMicros txTimer;
elapsedMillis statusTimer;

uint32_t txCount = 0;
uint32_t rxCount = 0;

unsigned long setupTime;
unsigned long loopStartTime;

bool feedbackReady = false;
bool isFinished = false;

float latestPosRad = 0.0f;
float latestSpeedRadS = 0.0f;
float latestTorqueNm = 0.0f;
float latestTempC = 0.0f;
uint8_t latestError = 0;

float latestPosDeg = 0.0f;
float targetPosDeg = 0.0f;

float pdTorque = 0.0f;
float outputCommand = 0.0f;   // chirp torque [N.m]
float torqueCommand = 0.0f;   // PD + chirp [N.m]

//=====================
// Utility
//=====================
int float_to_uint(float x, float x_min, float x_max, int bits) {
  float span = x_max - x_min;

  if (x > x_max) x = x_max;
  if (x < x_min) x = x_min;

  return (int)((x - x_min) * ((float)((1 << bits) - 1)) / span);
}

float uint_to_float(int x_int, float x_min, float x_max, int bits) {
  float span = x_max - x_min;
  return ((float)x_int) * span / ((float)((1 << bits) - 1)) + x_min;
}

float angleErrorDeg(float targetDeg, float currentDeg) {
  float error = targetDeg - currentDeg;

  while (error > 180.0f) {
    error -= 360.0f;
  }

  while (error < -180.0f) {
    error += 360.0f;
  }

  return error;
}

//=====================
// MIT Mode Commands
//=====================
void sendMitSpecial(uint8_t cmd) {
  CAN_message_t msg;
  msg.id = MOTOR_ID;
  msg.len = 8;
  msg.flags.extended = 0;

  for (int i = 0; i < 7; i++) {
    msg.buf[i] = 0xFF;
  }
  msg.buf[7] = cmd;

  Can0.write(msg);
  txCount++;
}

void enterMotorMode() {
  sendMitSpecial(0xFC);
}

void exitMotorMode() {
  sendMitSpecial(0xFD);
}

void setZeroPosition() {
  sendMitSpecial(0xFE);
}

void sendMitCommand(float p_des, float v_des, float kp, float kd, float t_ff) {
  p_des = constrain(p_des, P_MIN, P_MAX);
  v_des = constrain(v_des, V_MIN, V_MAX);
  kp    = constrain(kp, KP_MIN, KP_MAX);
  kd    = constrain(kd, KD_MIN, KD_MAX);
  t_ff  = constrain(t_ff, T_MIN, T_MAX);

  int p_int  = float_to_uint(p_des, P_MIN, P_MAX, 16);
  int v_int  = float_to_uint(v_des, V_MIN, V_MAX, 12);
  int kp_int = float_to_uint(kp, KP_MIN, KP_MAX, 12);
  int kd_int = float_to_uint(kd, KD_MIN, KD_MAX, 12);
  int t_int  = float_to_uint(t_ff, T_MIN, T_MAX, 12);

  CAN_message_t msg;
  msg.id = MOTOR_ID;
  msg.len = 8;
  msg.flags.extended = 0;

  msg.buf[0] = p_int >> 8;
  msg.buf[1] = p_int & 0xFF;
  msg.buf[2] = v_int >> 4;
  msg.buf[3] = ((v_int & 0xF) << 4) | (kp_int >> 8);
  msg.buf[4] = kp_int & 0xFF;
  msg.buf[5] = kd_int >> 4;
  msg.buf[6] = ((kd_int & 0xF) << 4) | (t_int >> 8);
  msg.buf[7] = t_int & 0xFF;

  Can0.write(msg);
  txCount++;
}

//=====================
// Decode MIT Feedback
//=====================
void decodeMitUpload(const CAN_message_t &msg) {
  if (msg.len != 8) return;

  int id = msg.buf[0];
  int p_int = (msg.buf[1] << 8) | msg.buf[2];
  int v_int = (msg.buf[3] << 4) | (msg.buf[4] >> 4);
  int t_int = ((msg.buf[4] & 0xF) << 8) | msg.buf[5];
  int temp_int = msg.buf[6];
  int err_int = msg.buf[7];

  latestPosRad = uint_to_float(p_int, P_MIN, P_MAX, 16);
  latestSpeedRadS = uint_to_float(v_int, V_MIN, V_MAX, 12);
  latestTorqueNm = uint_to_float(t_int, T_MIN, T_MAX, 12);
  latestTempC = temp_int - 40;
  latestError = err_int;

  latestPosDeg = latestPosRad * 180.0f / PI;

  if (!feedbackReady) {
    targetPosDeg = latestPosDeg;
    setupTime = micros();
    feedbackReady = true;
  }

  Serial.print("DATA,");
  Serial.print(millis()); Serial.print(",");
  Serial.print(latestPosDeg, 4); Serial.print(",");
  Serial.print(latestSpeedRadS, 5); Serial.print(",");
  Serial.print(latestTorqueNm, 5); Serial.print(",");
  Serial.print(pdTorque, 5); Serial.print(",");
  Serial.print(outputCommand, 5); Serial.print(",");
  Serial.print(torqueCommand, 5); Serial.print(",");
  Serial.print(latestTempC, 1); Serial.print(",");
  Serial.println(latestError);

  (void)id;
}

void readCANMessages() {
  CAN_message_t rxmsg;

  while (Can0.read(rxmsg)) {
    rxCount++;

    if (rxmsg.len == 8) {
      decodeMitUpload(rxmsg);
    }
  }
}

//=====================
// Setup
//=====================
void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println("==============================");
  Serial.println("AK80-8 MIT MODE CHIRP PD DATA LOGGER");
  Serial.println("==============================");
  Serial.println("CSV columns:");
  Serial.println("Time_ms,Position_deg,Speed_rad_s,Measured_Torque_Nm,PD_Torque_Nm,Chirp_Torque_Nm,Command_Torque_Nm,Temperature_C,Error_Code");

  Can0.begin();
  Can0.setBaudRate(CAN_BAUD);

  delay(500);

  enterMotorMode();
  delay(500);

  setupTime = micros();
}

//=====================
// Loop
//=====================
void loop() {
  readCANMessages();

  if (txTimer >= 500) {
    txTimer = 0;

    loopStartTime = micros();

    float t_total = (loopStartTime - setupTime) / 1000000.0f;
    float t_chirp = t_total - startDelaySec;
    if (t_chirp > duration + stopDelaySec) {
    for (int i = 0; i < 500; i++) {
        sendMitCommand(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        delay(1);
    }

    exitMotorMode();

    while (1) {
        delay(100);
      }
    }
    if (t_chirp >= 0.0f && t_chirp <= duration) {
      float k = pow(endFreq / startFreq, 1.0 / duration);
      float phase = 2.0f * PI * startFreq * (pow(k, t_chirp) - 1.0f) / log(k);
      outputCommand = CHIRP_TORQUE_AMPLITUDE * sin(phase);
    } else {
      outputCommand = 0.0f;

      if (t_chirp > duration) {
        isFinished = true;
      }
    }

    if (feedbackReady) {
      float posError = angleErrorDeg(targetPosDeg, latestPosDeg);

      pdTorque = KP_PD * posError - KD_PD * latestSpeedRadS;
      torqueCommand = pdTorque + outputCommand;
      torqueCommand = constrain(torqueCommand, -TORQUE_LIMIT_NM, TORQUE_LIMIT_NM);

      // External PD + chirp is sent as MIT torque feedforward.
      // MIT internal kp/kd are kept zero to match the old external controller structure.
      sendMitCommand(0.0f, 0.0f, 0.0f, 0.0f, torqueCommand);
    } else {
      sendMitCommand(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    }
  }

  if (statusTimer >= 1000) {
    statusTimer = 0;
    Serial.print("LOG: txCount=");
    Serial.print(txCount);
    Serial.print(" rxCount=");
    Serial.println(rxCount);
  }
}