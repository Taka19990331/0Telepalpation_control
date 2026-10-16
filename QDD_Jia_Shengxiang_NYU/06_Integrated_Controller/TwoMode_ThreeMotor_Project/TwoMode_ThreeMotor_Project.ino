#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <math.h>

#include "Mode1_Motor1.h"
#include "Mode1_Motor2.h"
#include "Mode1_Motor3.h"
#include "Mode2_Motor1.h"
#include "Mode2_Motor2.h"
#include "Mode2_Motor3.h"

enum class ControlMode : uint8_t {
  MODE1 = 1,
  MODE2 = 2
};

enum class RunMode : uint8_t {
  OFF = 0,
  ALL,
  M1_ONLY,
  M2_ONLY,
  M3_ONLY
};

ControlMode controlMode = ControlMode::MODE1;
RunMode runMode = RunMode::OFF;
String serialCommand;

const char* controlModeName() {
  return (controlMode == ControlMode::MODE1) ? "MODE1" : "MODE2";
}

void mode1StopAll() {
  Mode1Motor1::projectStop();
  Mode1Motor2::projectStop();
  Mode1Motor3::projectStop();
}

void mode2StopAll() {
  Mode2Motor1::stopControl();
  Mode2Motor2::stopControl();
  Mode2Motor3::stopControl();
}

void activeStopAll() {
  if (controlMode == ControlMode::MODE1) mode1StopAll();
  else mode2StopAll();
  runMode = RunMode::OFF;
}

void initMode1() {
  Mode1Motor1::projectInit();
  Mode1Motor2::projectInit();
  Mode1Motor3::projectInit();
  mode1StopAll();
}

void initMode2() {
  Mode2Motor1::init();
  Mode2Motor2::init();
  Mode2Motor3::init();
  mode2StopAll();
}

void switchControlMode(ControlMode nextMode) {
  // Always make the currently active algorithm command zero torque first.
  activeStopAll();
  delay(20);

  controlMode = nextMode;

  // Reinitialize the three physical CAN controllers for the newly selected
  // algorithm.  The new mode always starts in OFF for safety.
  if (controlMode == ControlMode::MODE1) initMode1();
  else initMode2();

  runMode = RunMode::OFF;

  Serial.print("CONTROL_MODE,");
  Serial.print(controlModeName());
  Serial.println(",RUN=OFF");
}

void startAll() {
  activeStopAll();
  delay(5);

  if (controlMode == ControlMode::MODE1) {
    Mode1Motor1::projectStart();
    Mode1Motor2::projectStart();
    Mode1Motor3::projectStart();
  } else {
    Mode2Motor1::startControl();
    Mode2Motor2::startControl();
    Mode2Motor3::startControl();
  }

  runMode = RunMode::ALL;
  Serial.print(controlModeName());
  Serial.println(",ON,ALL_MOTORS");
}

void startOnlyM1() {
  activeStopAll();
  delay(5);

  if (controlMode == ControlMode::MODE1) Mode1Motor1::projectStart();
  else Mode2Motor1::startControl();

  runMode = RunMode::M1_ONLY;
  Serial.print(controlModeName());
  Serial.println(",M1,ID1_CAN1_ONLY");
}

void startOnlyM2() {
  activeStopAll();
  delay(5);

  if (controlMode == ControlMode::MODE1) Mode1Motor2::projectStart();
  else Mode2Motor2::startControl();

  runMode = RunMode::M2_ONLY;
  Serial.print(controlModeName());
  Serial.println(",M2,ID2_CAN2_ONLY");
}

void startOnlyM3() {
  activeStopAll();
  delay(5);

  if (controlMode == ControlMode::MODE1) Mode1Motor3::projectStart();
  else Mode2Motor3::startControl();

  runMode = RunMode::M3_ONLY;
  Serial.print(controlModeName());
  Serial.println(",M3,ID3_CAN3_ONLY");
}

void printStatus() {
  Serial.print("STATUS,CONTROL=");
  Serial.print(controlModeName());
  Serial.print(",RUN=");
  switch (runMode) {
    case RunMode::ALL:     Serial.println("ON"); break;
    case RunMode::M1_ONLY: Serial.println("M1"); break;
    case RunMode::M2_ONLY: Serial.println("M2"); break;
    case RunMode::M3_ONLY: Serial.println("M3"); break;
    case RunMode::OFF:
    default:               Serial.println("OFF"); break;
  }
}

void handleCommand(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  if (cmd.length() == 0) return;

  if (cmd == "MODE1" || cmd == "P1") {
    switchControlMode(ControlMode::MODE1);
  } else if (cmd == "MODE2" || cmd == "P2") {
    switchControlMode(ControlMode::MODE2);
  } else if (cmd == "ON") {
    startAll();
  } else if (cmd == "M1") {
    startOnlyM1();
  } else if (cmd == "M2") {
    startOnlyM2();
  } else if (cmd == "M3") {
    startOnlyM3();
  } else if (cmd == "OFF") {
    activeStopAll();
    Serial.print(controlModeName());
    Serial.println(",OFF,ALL_MOTORS_STOPPED");
  } else if (cmd == "STATUS") {
    printStatus();
  } else {
    Serial.print("ERROR,UNKNOWN_COMMAND,");
    Serial.println(cmd);
    Serial.println("HELP,MODE1 | MODE2 | ON | M1 | M2 | M3 | OFF | STATUS");
  }
}

void readSerialCommands() {
  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());

    if (c == '\n' || c == '\r') {
      if (serialCommand.length() > 0) {
        handleCommand(serialCommand);
        serialCommand = "";
      }
    } else if (serialCommand.length() < 32) {
      serialCommand += c;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Default algorithm after power-up is MODE1, but motor output is OFF.
  controlMode = ControlMode::MODE1;
  initMode1();
  runMode = RunMode::OFF;

  Serial.println();
  Serial.println("READY,Two-mode three-motor controller");
  Serial.println("MODE1=original ThreeMotor_Controller algorithm");
  Serial.println("MODE2=RFOB + friction compensation algorithm");
  Serial.println("MAP,M1=ID1/CAN1,M2=ID2/CAN2,M3=ID3/CAN3");
  Serial.println("CMD,MODE1 | MODE2 | ON | M1 | M2 | M3 | OFF | STATUS");
  Serial.println("STARTUP,MODE1,OFF");
}

void loop() {
  readSerialCommands();

  // Only the selected control algorithm is allowed to consume CAN feedback
  // and generate control commands.
  if (controlMode == ControlMode::MODE1) {
    Mode1Motor1::projectService();
    Mode1Motor2::projectService();
    Mode1Motor3::projectService();
  } else {
    Mode2Motor1::update();
    Mode2Motor2::update();
    Mode2Motor3::update();
  }
}
