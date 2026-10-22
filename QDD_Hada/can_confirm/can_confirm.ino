#include <FlexCAN_T4.h>

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> can1;
unsigned long previousMillis = 0;

void setup() {
  Serial.begin(115200);
  can1.begin();
  can1.setBaudRate(1000000); // 1Mbps
  
  delay(2000); // 起動待ち
  Serial.println("--- CAN Communication Ping Test ---");
}

void loop() {
  unsigned long currentMillis = millis();

  // 1秒に1回、「速度0」のコマンドを送信する
  if (currentMillis - previousMillis >= 1000) {
    previousMillis = currentMillis;

    CAN_message_t outMsg;
    uint8_t motor_id = 1;
    uint8_t command_id = 3; // 速度制御コマンド
    
    outMsg.id = (command_id << 8) | motor_id;
    outMsg.flags.extended = 1;
    outMsg.len = 4;
    outMsg.buf[0] = 0; outMsg.buf[1] = 0; outMsg.buf[2] = 0; outMsg.buf[3] = 0; // 速度 0 ERPM

    can1.write(outMsg);
    Serial.println("[TX] Sent Speed=0 command.");
  }

  // モータからの返事（フィードバック）を受信してダンプする
  CAN_message_t inMsg;
  if (can1.read(inMsg)) {
    Serial.print("[RX] Received Message - ID: 0x");
    Serial.print(inMsg.id, HEX);
    Serial.print("  Data: ");
    
    // 受信したデータを16進数で表示
    for (int i = 0; i < inMsg.len; i++) {
      if (inMsg.buf[i] < 0x10) Serial.print("0"); // 桁揃え
      Serial.print(inMsg.buf[i], HEX);
      Serial.print(" ");
    }
    Serial.println("  --> [OK] Communication Established!");
  }
}