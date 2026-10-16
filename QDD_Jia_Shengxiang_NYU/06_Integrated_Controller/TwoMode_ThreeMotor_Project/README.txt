TwoMode_ThreeMotor_Project
==========================

Serial baud: 115200

Algorithm selection:
  MODE1  = original ThreeMotor_Controller algorithm
  MODE2  = RFOB + friction compensation algorithm
  P1/P2  = short aliases for MODE1/MODE2

Motor commands (same in both algorithms):
  ON   = start all three motors
  M1   = only ID1 / CAN1
  M2   = only ID2 / CAN2
  M3   = only ID3 / CAN3
  OFF  = stop all three motors
  STATUS = print selected algorithm and motor run mode

Safety behavior:
  - Startup: MODE1 + OFF
  - Changing MODE1/MODE2 first stops all motors.
  - A newly selected algorithm stays OFF until ON/M1/M2/M3 is sent.
  - Only the selected algorithm services CAN/control in loop().

Example:
  MODE1
  ON
  OFF
  MODE2
  M3
  OFF
