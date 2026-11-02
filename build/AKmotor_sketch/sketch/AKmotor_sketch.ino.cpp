#line 1 "C:\\0Telepalpation_control\\plain_programs\\AKmotor_sketch\\AKmotor_sketch.ino"
#include <Arduino.h>
#include "can_protocol.h"
#include "save_data.h"

#if !defined(ARDUINO_TEENSY41)
#error "Select Teensy 4.1 with built-in SD socket"
#endif

// Edit this value to change the constant torque command.
volatile float tau_cmd = 0.8f;
volatile bool active=false, startRequest=false;
volatile uint32_t runStart=0;
volatile uint32_t statusCode=0;
IntervalTimer controlTimer;
bool timerReady=false;
bool connectionReported=false;
uint32_t lastEnableMs=0;

#line 19 "C:\\0Telepalpation_control\\plain_programs\\AKmotor_sketch\\AKmotor_sketch.ino"
void controlISR();
#line 42 "C:\\0Telepalpation_control\\plain_programs\\AKmotor_sketch\\AKmotor_sketch.ino"
void startRun();
#line 51 "C:\\0Telepalpation_control\\plain_programs\\AKmotor_sketch\\AKmotor_sketch.ino"
void setup();
#line 65 "C:\\0Telepalpation_control\\plain_programs\\AKmotor_sketch\\AKmotor_sketch.ino"
void loop();
#line 19 "C:\\0Telepalpation_control\\plain_programs\\AKmotor_sketch\\AKmotor_sketch.ino"
void controlISR() {
  const uint32_t now=micros();
  if(startRequest) { startRequest=false; runStart=now; active=true; }
  if(!active) {
    // Keep polling with zero position/velocity/gains/torque, even before power-on.
    CanProtocol::command(0,0,0,0,0);
    return;
  }
  if(!CanProtocol::connected()) {
    statusCode=6; active=false; SaveData::finish(statusCode);
    CanProtocol::command(0,0,0,0,0); return;
  }
  const float torque=tau_cmd;
  if(!CanProtocol::command(0,0,0,0,torque)) {
    statusCode=2; active=false; SaveData::finish(statusCode);
    CanProtocol::command(0,0,0,0,0); return;
  }
  if(!SaveData::record(now-runStart,torque)) {
    statusCode=3; active=false; SaveData::finish(statusCode);
    CanProtocol::command(0,0,0,0,0);
  }
}

void startRun() {
  if(!timerReady || !CanProtocol::connected()) {
    Serial.println("START_REJECTED,WAIT_FOR_CONNECTION"); return;
  }
  if(active || startRequest || SaveData::recording || SaveData::finalizeRequest) return;
  if(!SaveData::start(tau_cmd)) { Serial.println("START_REJECTED,SD_ERROR"); return; }
  noInterrupts(); startRequest=true; interrupts();
}

void setup() {
  Serial.begin(115200);
  CanProtocol::begin();
  const bool cardReady=SaveData::begin();
  controlTimer.priority(192);
  timerReady=controlTimer.begin(controlISR,CanProtocol::PERIOD_US);
  Serial.print("MOTOR,"); Serial.print(CanProtocol::MOTOR_SELECT);
  Serial.print(",ID,"); Serial.print(CanProtocol::MOTOR_ID);
  Serial.print(",SD,"); Serial.print(cardReady?"OK":"ERROR");
  Serial.print(",TIMER,"); Serial.println(timerReady?"OK":"ERROR");
  CanProtocol::enable();
  lastEnableMs=millis();
}

void loop() {
  const bool connected=CanProtocol::connected();
  if(connected != connectionReported) {
    connectionReported=connected;
    Serial.print(CanProtocol::MOTOR_SELECT);
    Serial.print(",ID,"); Serial.print(CanProtocol::MOTOR_ID);
    Serial.println(connected?" CONNECTED":" DISCONNECTED");
  }
  // Retry MIT enable so a motor powered after the Teensy can answer.
  if(!connected && !active && !startRequest && uint32_t(millis()-lastEnableMs)>=500) {
    CanProtocol::enable(); lastEnableMs=millis();
  }
  while(Serial.available()) {
    const char c=Serial.read();
    if(c=='s'||c=='S') startRun();
    else if(c=='x'||c=='X') {
      noInterrupts();
      const bool wasRunning=active || startRequest;
      active=false; startRequest=false;
      interrupts();
      CanProtocol::command(0,0,0,0,0);
      if(wasRunning) SaveData::finish(1);
    } else if((c=='d'||c=='D') && !SaveData::finalizeRequest) SaveData::dumpCsv();
  }
  if(SaveData::service()) CanProtocol::command(0,0,0,0,0);
}
