#include <Arduino.h>
#include <FlexCAN_T4.h>

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can0;

//=====================
// Settings
//=====================
const uint32_t CAN_BAUD = 1000000;
const uint8_t MOTOR_ID = 104;

const float TARGET_ERPM = 6000.0f;
const float TARGET_CURRENT = 0.7f; 

elapsedMillis txTimer;
elapsedMillis statusTimer;

uint32_t txCount = 0;
uint32_t rxCount = 0;

const double startFreq = 0.5;   // 開始周波数 [Hz]
const double endFreq = 100.0;   // 終了周波数 [Hz]
const double duration = 60.0;   // スイープ時間 [sec]
#define CHIRP_PWM_AMPLITUDE 0.5 // 最大振幅0.5A-1000にしたら煙 150 システム同定ｙ0.212

const double startDelaySec = 5.0;
bool isFinished = false;
unsigned long setupTime;
unsigned long loopStartTime;    
float outputCommand=0;
// Safe initial PD settings
float latestPosDeg = 0.0f;
float latestSpeedErpm = 0.0f;
bool feedbackReady = false;

float targetPosDeg;// = 0.0f;
float pdCurrent = 0.0f;
float currentCommand = 0.0f;

const float KP_PD = 0.1f;
const float KD_PD = 0.0001f;
//=====================
// Utility
//=====================
void appendInt32BigEndian(uint8_t *buf, int32_t value) {
    buf[0] = (value >> 24) & 0xFF;
    buf[1] = (value >> 16) & 0xFF;
    buf[2] = (value >> 8) & 0xFF;
    buf[3] = value & 0xFF;
}

int16_t readInt16BigEndian(uint8_t high, uint8_t low) {
    uint16_t u = ((uint16_t)high << 8) | low;
    return (int16_t)u;
}

uint32_t makeServoExtendedID(uint8_t packet_id, uint8_t motor_id) {
    return ((uint32_t)packet_id << 8) | motor_id;
}

enum ServoCanPacketID {
    CAN_PACKET_SET_DUTY = 0,
    CAN_PACKET_SET_CURRENT = 1,
    CAN_PACKET_SET_CURRENT_BRAKE = 2,
    CAN_PACKET_SET_RPM = 3,
    CAN_PACKET_SET_POS = 4,
    CAN_PACKET_SET_ORIGIN_HERE = 5,
    CAN_PACKET_SET_POS_SPD = 6,
};

//=====================
// Speed Command
//=====================
void sendServoSpeedCommand(float erpm) {
    CAN_message_t msg;
    int32_t rpm_cmd = (int32_t)erpm;

    msg.id = makeServoExtendedID(CAN_PACKET_SET_RPM, MOTOR_ID);
    msg.len = 4;
    msg.flags.extended = 1;

    appendInt32BigEndian(msg.buf, rpm_cmd);
    Can0.write(msg);
    txCount++;
}

//=====================
// Torque (Current) Command
//=====================
void sendServoCurrentCommand(float current_A) {
    CAN_message_t msg;
    int32_t current_cmd = (int32_t)(current_A * 1000.0f);

    msg.id = makeServoExtendedID(CAN_PACKET_SET_CURRENT, MOTOR_ID);
    msg.len = 4;
    msg.flags.extended = 1;

    appendInt32BigEndian(msg.buf, current_cmd);
    Can0.write(msg);
    txCount++;
}

// 原点（0度）セットコマンドを送信する関数
void setOriginHere() {
    CAN_message_t msg;
    msg.id = makeServoExtendedID(CAN_PACKET_SET_ORIGIN_HERE, MOTOR_ID);
    msg.len = 4;
    msg.flags.extended = 1;
    msg.buf[0] = 0; msg.buf[1] = 0; msg.buf[2] = 0; msg.buf[3] = 0;
    Can0.write(msg);
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
// Decode Servo Upload
//=====================
void decodeServoUpload(const CAN_message_t &msg) {
    if (msg.len != 8) return;

    int16_t pos_int = readInt16BigEndian(msg.buf[0], msg.buf[1]);
    int16_t spd_int = readInt16BigEndian(msg.buf[2], msg.buf[3]);
    int16_t cur_int = readInt16BigEndian(msg.buf[4], msg.buf[5]);

    float pos_deg = pos_int * 0.1f / 8.0f;
    float spd_erpm = spd_int * 10.0f;
    float cur_A = cur_int * 0.01f;
    int8_t temp_C = (int8_t)msg.buf[6];
    uint8_t error = msg.buf[7];

    // 4. ★1周(360度)で0度に戻るようにラップアラウンド処理★
    // 360.0で割った余りを計算
    float wrapped_pos_deg = fmod(pos_deg, 360.0f);
    
    // もしマイナス方向に回転して角度が負の値になった場合、正の角度（0〜359.9度）に補正する
    if (wrapped_pos_deg < 0.0f) {
        wrapped_pos_deg += 360.0f;
    }
    latestPosDeg = wrapped_pos_deg;
    latestSpeedErpm = spd_erpm;

    if (!feedbackReady) {
    targetPosDeg = latestPosDeg;
    feedbackReady = true;
    }

    // ★★★ Python側で抽出するためのCSV形式出力 (先頭に DATA, を付与) ★★★
    // フォーマット: DATA,時間(ms),位置(deg),速度(eRPM),電流(A),温度(C),エラー
    Serial.print("DATA,");
    Serial.print(millis()); Serial.print(",");
    Serial.print(wrapped_pos_deg, 2); Serial.print(",");
    Serial.print(spd_erpm, 1); Serial.print(",");
    Serial.print(cur_A, 2); Serial.print(",");
    Serial.print(pdCurrent, 5); Serial.print(",");
    Serial.print(outputCommand, 5); Serial.print(",");
    Serial.print(currentCommand, 5); Serial.print(",");;
    Serial.print(temp_C); Serial.print(",");
    Serial.println(error);
}

void readCANMessages() {
    CAN_message_t rxmsg;
    while (Can0.read(rxmsg)) {
        rxCount++;
        if (rxmsg.len == 8) {
            decodeServoUpload(rxmsg);
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
    Serial.println("AK80-8 SYSTEM IDENTIFICATION DATA LOGGER");
    Serial.println("==============================");

    Can0.begin();
    Can0.setBaudRate(CAN_BAUD);
    setOriginHere();
    delay(500);

    setupTime=micros();

}

//=====================
// Loop
//=====================
void loop() {
    readCANMessages();
    loopStartTime = micros();
// --- Chirp信号生成 ---
    float t_total = (loopStartTime - setupTime) / 1000000.0;
    float t_chirp = t_total - startDelaySec;

    if (t_chirp >= 0 && t_chirp <= duration) {
        float k = pow(endFreq/startFreq , 1.0/duration); //pow(底,指数)→指数関数
        float phase = 2.0 * M_PI * startFreq * (pow(k , t_chirp)-1) / log(k);
        outputCommand = CHIRP_PWM_AMPLITUDE * sin(phase);
    } else {
        outputCommand = 0;
        if (t_chirp > duration) isFinished = true;
    }

    //targetPosDeg = 50*sin(loopStartTime - setupTime)/1000000.0;
    targetPosDeg = 20.0f * sin(2.0f * PI * 0.2f * (loopStartTime - setupTime)/1000000.0);
    if (txTimer >= 10) { // 制御周期を100Hz (10ms) に高速化
        txTimer = 0;
        //sendServoSpeedCommand(TARGET_ERPM); // 速度制御の場合
        //sendServoCurrentCommand(TARGET_CURRENT); // トルク制御の場合
    if (feedbackReady) {
    float posError = angleErrorDeg(targetPosDeg, latestPosDeg);
    pdCurrent = KP_PD * posError - KD_PD * latestSpeedErpm;
    currentCommand = pdCurrent ;//+ outputCommand;

    sendServoCurrentCommand(currentCommand);
} else {
    sendServoCurrentCommand(0.0f);
}
    }

    if (statusTimer >= 1000) {
        statusTimer = 0;
        // 動作中の生存確認用ログ
        Serial.print("LOG: txCount="); Serial.print(txCount);
        Serial.print(" rxCount="); Serial.println(rxCount);
    }
}