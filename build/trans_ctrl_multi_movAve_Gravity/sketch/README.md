#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_multi_movAve_Gravity\\README.md"
# trans_ctrl_multi_movAve_Gravity

Teensy 4.1 dual-AK80 Jia Mode1 controller with only RFOB transparency and gravity compensation.

- M1: CAN1 / ID 1; M2: CAN2 / ID 2; 1 Mbps.
- RX polling: 100 us; control and TX: 500 us (2 kHz); timer priority 96, CAN IRQ priority 128.
- Control output per motor: saturated `(K_R * RFOB + K_F * friction) + gravity torque`.
- RFOB velocity uses the raw feedback velocity. Its first-order Q observer retains the original equation, asymmetric Stribeck parameters, J, and sign convention. The exact ZOH coefficient is precomputed for the 500 us control period.
- Gravity model and calibration match `AK80_GravityCompensation.ino`; edit `K_G` near the top of the sketch. Gravity ramps over 1 second after `ON`.
- RFOB plus friction feed-forward is limited by `TORQUE_LIMIT` (1.0 Nm). The combined command is fault-stopped above `MAX_ABS_COMMAND_TORQUE_NM` (5.0 Nm).
- Startup feedback timeout: 500 ms. After first feedback, a frame age above 3 ms faults the run.
- Startup sends periodic zero-torque MIT commands until both feedback streams arrive. `S` alone never enables either control output.

Commands: `S`, `ON`, `OFF`, `KR <value>`, `KF <value>`, `BIAS`, `STATUS`, `X`.

`STATUS` includes per-motor accepted, seen, and rejected RX counts; feedback age; TX failures and queue backlogs; RX budget hits; ISR maximum duration and overrun count; fault name/target; and current RFOB, gravity, and combined torques.
