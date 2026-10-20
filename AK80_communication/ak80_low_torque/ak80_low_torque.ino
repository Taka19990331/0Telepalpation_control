#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <math.h>
// Select the wired motor here, then rebuild/upload.
constexpr char MOTOR_SELECT[] = "80B"; // "80A", "80B", "60"
constexpr bool sameText(const char *a, const char *b) {
  return *a == *b && (*a == '\0' || sameText(a+1,b+1));
}
constexpr bool IS_80A=sameText(MOTOR_SELECT,"80A");
constexpr bool IS_80B=sameText(MOTOR_SELECT,"80B");
constexpr bool IS_60=sameText(MOTOR_SELECT,"60");
static_assert(IS_80A || IS_80B || IS_60,"Select 80A, 80B or 60");
FlexCAN_T4<IS_60 ? CAN3 : (IS_80A ? CAN1 : CAN2), RX_SIZE_256, TX_SIZE_16> Can0;
constexpr uint32_t MOTOR_ID=IS_60 ? 3 : (IS_80A ? 1 : 2);
// Match 06_Integrated_Controller Mode1 ranges.
const float P_MIN=IS_60 ? -12.56f : -12.5f, P_MAX=-P_MIN;
const float V_MIN=IS_60 ? -60.0f : -45.0f, V_MAX=-V_MIN;
const float T_MIN=IS_60 ? -12.0f : -32.0f, T_MAX=-T_MIN;
const float KP_MIN=0, KP_MAX=500, KD_MIN=0, KD_MAX=5;
const float TEST_TORQUE_NM = 1.0f;
const float SPEED_STOP_RAD_S = 40.0f;
const uint32_t RUN_MS = 5000, FEEDBACK_TIMEOUT_MS = 100;
uint32_t txCount=0, startedMs=0, lastRxMs=0, runMs=0;
uint32_t txFailed=0, rxFrames=0, rxAccepted=0;
CAN_message_t lastRxFrame = {};
elapsedMicros txTimer;
elapsedMillis logTimer;
bool active=false, running=false, received=false;
float speedRadS=0, positionRad=0;
float feedbackTorqueNm=NAN, feedbackCurrentA=NAN, feedbackTempC=NAN;
int feedbackRaw=0, feedbackError=-1;
float queuedCommandNm=0, queuedEncodedNm=0;
// queued = accepted by FlexCAN, not an acknowledgement from the motor.
void printLogHeader() {
  Serial.println("DATA,Time_ms,Position_rad,Speed_rad_s,QueuedCommand_Nm,EncodedCommand_Nm,FeedbackRaw,FeedbackTorque_Nm,FeedbackCurrent_A,Temperature_C,ErrorCode,FeedbackAge_ms");
}
void printFeedbackLog() {
  char line[256];
  int n=snprintf(line,sizeof(line),"DATA,%lu,%.5f,%.5f,%.5f,%.5f,%d,%.5f,%.5f,%.1f,%d,%lu\n",
      (unsigned long)millis(),positionRad,speedRadS,queuedCommandNm,queuedEncodedNm,
      feedbackRaw,feedbackTorqueNm,feedbackCurrentA,feedbackTempC,feedbackError,
      (unsigned long)(millis()-lastRxMs));
  // Drop a log row rather than delay CAN servicing when USB is congested.
  if (n>0 && n<(int)sizeof(line) && Serial.availableForWrite()>=n) Serial.write(line,n);
}
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
  if (IS_60) return;
  CAN_message_t msg = {};
  msg.id = MOTOR_ID;
  msg.len = 8;
  msg.flags.extended = 0;

  for (int i = 0; i < 7; i++) {
    msg.buf[i] = 0xFF;
  }
  msg.buf[7] = cmd;

  if (!Can0.write(msg)) txFailed++;
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

  CAN_message_t msg = {};
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
  if (IS_60) {
    msg.id = (8UL << 8) | MOTOR_ID;
    msg.flags.extended = 1;
    msg.buf[0] = kp_int >> 4;
    msg.buf[1] = ((kp_int & 0xF) << 4) | (kd_int >> 8);
    msg.buf[2] = kd_int & 0xFF;
    msg.buf[3] = p_int >> 8;
    msg.buf[4] = p_int & 0xFF;
    msg.buf[5] = v_int >> 4;
    msg.buf[6] = ((v_int & 0xF) << 4) | (t_int >> 8);
    msg.buf[7] = t_int & 0xFF;
  }

  if (!Can0.write(msg)) txFailed++;
  else {
    queuedCommandNm=t_ff;
    queuedEncodedNm=uint_to_float(t_int,T_MIN,T_MAX,12);
  }
  txCount++;
}

//=====================

void stopMotor(const char *reason) {
  active=false; running=false;
  sendMitCommand(0,0,0,0,0);
  exitMotorMode();
  Serial.print("STOP,"); Serial.println(reason);
  if (received) {
    Serial.print("LAST_FEEDBACK,RAW,"); Serial.print(feedbackRaw);
    Serial.print(",TORQUE_NM,"); Serial.print(feedbackTorqueNm,5);
    Serial.print(",CURRENT_A,"); Serial.print(feedbackCurrentA,5);
    Serial.print(",ERROR,"); Serial.println(feedbackError);
  }
  Serial.print("CAN_DIAG,TX_ATTEMPTS,"); Serial.print(txCount);
  Serial.print(",TX_QUEUE_FAILED,"); Serial.print(txFailed);
  Serial.print(",RX_ALL,"); Serial.print(rxFrames);
  Serial.print(",RX_ACCEPTED,"); Serial.println(rxAccepted);
  if (rxFrames) {
    Serial.print("LAST_RX,ID_HEX,"); Serial.print(lastRxFrame.id, HEX);
    Serial.print(",EXT,"); Serial.print(lastRxFrame.flags.extended);
    Serial.print(",LEN,"); Serial.print(lastRxFrame.len);
    Serial.print(",DATA_HEX");
    for (uint8_t i=0; i<lastRxFrame.len && i<8; ++i) {
      Serial.print(','); Serial.print(lastRxFrame.buf[i], HEX);
    }
    Serial.println();
  }
}
void setup() {
  Serial.begin(115200);
  Can0.begin(); Can0.setBaudRate(1000000);
  sendMitCommand(0,0,0,0,0);
  Serial.print("MOTOR,"); Serial.print(MOTOR_SELECT);
  Serial.print(",CAN,"); Serial.print(IS_60 ? 3 : (IS_80A ? 1 : 2));
  Serial.print(",ID,"); Serial.println(MOTOR_ID);
  exitMotorMode();
  const uint32_t serialWaitMs = millis();
  while (!Serial && millis()-serialWaitMs < 5000) {}
  Serial.print("LOW_PROTOCOL_V3,MOTOR,"); Serial.print(MOTOR_SELECT);
  Serial.print(",CAN,"); Serial.print(IS_60 ? 3 : (IS_80A ? 1 : 2));
  Serial.print(",ID,"); Serial.println(MOTOR_ID);
  Serial.print("READY: s=start ");
  Serial.print(TEST_TORQUE_NM, 4);
  Serial.print(" Nm for ");
  Serial.print(RUN_MS / 1000.0f, 3);
  Serial.println(" s, x=stop; no newline needed");
  printLogHeader();
}
void loop() {
  if (IS_60) Can0.events(); // Integrated Controller Motor3 service sequence.
  while (Serial.available()) {
    char c=Serial.read();
    if (c=='x' || c=='X') stopMotor("USER");
    if ((c=='s' || c=='S') && !active) {
      CAN_message_t stale;
      while (Can0.read(stale)) {} // Discard frames from a previous run.
      printLogHeader();
      feedbackTorqueNm=NAN; feedbackCurrentA=NAN; feedbackTempC=NAN; feedbackError=-1;
      received=false; running=false; active=true; startedMs=millis();
      txCount=0; txFailed=0; rxFrames=0; rxAccepted=0;
      txTimer=0; logTimer=0;
      enterMotorMode();
      if (IS_60) sendMitCommand(0,0,0,0,0);
    }
  }
  CAN_message_t m;
  while (Can0.read(m)) {
    if (!active) continue;
    rxFrames++; lastRxFrame=m;
    if (m.flags.remote) continue;
    uint8_t error=0;
    if (IS_60) {
      if (!m.flags.extended || m.id!=((0x29UL<<8)|MOTOR_ID) || m.len!=8) continue;
      const int16_t p=(int16_t)(((uint16_t)m.buf[0]<<8)|m.buf[1]);
      const int16_t v=(int16_t)(((uint16_t)m.buf[2]<<8)|m.buf[3]);
      positionRad=p*0.1f*PI/180.0f;
      // Integrated controller: 14 pole pairs, 6:1 gear ratio.
      speedRadS=v*10.0f/14.0f/6.0f*(2.0f*PI/60.0f);
      feedbackRaw=(int16_t)(((uint16_t)m.buf[4]<<8)|m.buf[5]);
      feedbackCurrentA=feedbackRaw*0.01f;
      feedbackTorqueNm=0.5994f*feedbackCurrentA; // Mode1_Motor3.h KT_NOMINAL
      feedbackTempC=m.buf[6];
      error=m.buf[7];
    } else {
      // Same length/ID acceptance as Integrated Controller Mode1 Motor1/2.
      if (m.len < 6 || (m.id != MOTOR_ID && m.buf[0] != MOTOR_ID)) continue;
      positionRad=uint_to_float((m.buf[1]<<8)|m.buf[2],P_MIN,P_MAX,16);
      speedRadS=uint_to_float((m.buf[3]<<4)|(m.buf[4]>>4),V_MIN,V_MAX,12);
      feedbackRaw=((m.buf[4]&0x0F)<<8)|m.buf[5];
      feedbackTorqueNm=uint_to_float(feedbackRaw,T_MIN,T_MAX,12);
      // Same torque interpretation as Mode1_Motor1/2; no verified AK80 Nm/A conversion.
      feedbackCurrentA=NAN;
      feedbackTempC=m.len>=7 ? (float)m.buf[6] : NAN;
      error=m.len>=8 ? m.buf[7] : 0;
    }
    feedbackError=m.len>=8 ? error : -1; // -1: error field absent, not confirmed healthy.
    rxAccepted++; received=true; lastRxMs=millis();
    if (error) stopMotor("MOTOR_ERROR");
    else if (fabsf(speedRadS)>=SPEED_STOP_RAD_S) stopMotor("SPEED_LIMIT");
  }
  if (active) {
    if ((!received && millis()-startedMs>=FEEDBACK_TIMEOUT_MS) ||
        (received && millis()-lastRxMs>=FEEDBACK_TIMEOUT_MS)) stopMotor("FEEDBACK_TIMEOUT");
    else if (!running && received) {
      running=true; runMs=millis();
      Serial.print("RUN,"); Serial.print(TEST_TORQUE_NM, 4); Serial.println(" Nm");
    } else if (running && millis()-runMs>=RUN_MS) stopMotor("TIME_LIMIT");
  }
  if ((active || IS_60) && txTimer>=1000) {
    txTimer=0;
    sendMitCommand(0,0,0,0,(active && running) ? TEST_TORQUE_NM : 0);
  }
  if (active && received && logTimer>=50) {
    logTimer=0;
    printFeedbackLog();
  }
}


