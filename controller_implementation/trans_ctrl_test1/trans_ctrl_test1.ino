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

void controlISR() {
  const uint32_t now=micros();
  if(startRequest) { startRequest=false; runStart=now; active=true; }
  if(!active) return;
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
  if(active || SaveData::recording || !SaveData::start(tau_cmd)) return;
  CanProtocol::enable(); CanProtocol::command(0,0,0,0,0);
  noInterrupts(); startRequest=true; interrupts();
}

void setup() {
  Serial.begin(115200);
  CanProtocol::begin();
  const bool cardReady=SaveData::begin();
  controlTimer.priority(192);
  const bool timerReady=controlTimer.begin(controlISR,CanProtocol::PERIOD_US);
  Serial.print("READY,MOTOR,"); Serial.print(CanProtocol::MOTOR_SELECT);
  Serial.print(",ID,"); Serial.print(CanProtocol::MOTOR_ID);
  Serial.print(",SD,"); Serial.print(cardReady?"OK":"ERROR");
  Serial.print(",TIMER,"); Serial.println(timerReady?"OK":"ERROR");
}

void loop() {
  while(Serial.available()) {
    const char c=Serial.read();
    if(c=='s'||c=='S') startRun();
    else if((c=='x'||c=='X')&&active) {
      noInterrupts(); active=false; interrupts();
      CanProtocol::command(0,0,0,0,0); CanProtocol::disable();
      SaveData::finish(1);
    } else if(c=='d'||c=='D') SaveData::dumpCsv();
  }
  if(SaveData::service()) {
    CanProtocol::command(0,0,0,0,0); CanProtocol::disable();
  }
}
