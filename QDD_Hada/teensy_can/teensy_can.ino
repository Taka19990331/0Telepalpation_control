// Teensy 4.1の CAN1 ピンを通常のデジタルピンとして使用します
const int txPin = 23; // CTX1
const int rxPin = 22; // CRX1

void setup() {
  Serial.begin(115200);
  
  // ピンの入出力設定
  pinMode(txPin, OUTPUT);
  pinMode(rxPin, INPUT);
  
  // 初期状態はHIGH (CANバスのリセッシブ状態) にしておく
  digitalWrite(txPin, HIGH);
  delay(1000);
  Serial.println("--- Wiring & Voltage Divider Test Start ---");
}

void loop() {
  // 1. TXピンから LOW (0V) を出力
  digitalWrite(txPin, LOW);
  delay(10); // トランシーバの反応待ち
  int valLow = digitalRead(rxPin); // RXピンで読み取る
  
  // 2. TXピンから HIGH (3.3V) を出力
  digitalWrite(txPin, HIGH);
  delay(10);
  int valHigh = digitalRead(rxPin);

  // 3. 結果の表示
  Serial.print("Sent LOW  -> Read: ");
  Serial.print(valLow);
  Serial.print("  |  Sent HIGH -> Read: ");
  Serial.println(valHigh);
  
  // もし配線が完璧なら、「Sent LOW -> Read: 0 | Sent HIGH -> Read: 1」と表示されます
  if (valLow == 0 && valHigh == 1) {
    Serial.println("   [OK] Wiring and Voltage Divider are PERFECT!");
  } else {
    Serial.println("   [ERROR] Something is wrong with the wiring or resistors.");
  }
  
  Serial.println("-------------------------------------------");
  delay(1000);
}