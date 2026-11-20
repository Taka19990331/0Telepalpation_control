#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_RFOB_Gravity_Fric\\README.md"
# trans_ctrl_RFOB_Gravity_Fric

Teensy 4.1 dual-AK80 Jia Mode1 controller with RFOB transparency, gravity compensation, and an independent motor-specific tanh friction feed-forward term.

- M1: CAN1 / ID 1; M2: CAN2 / ID 2; CAN 1 Mbps.
- RX polling: 100 us; control/TX: 500 us (2 kHz); timer priority 96, CAN IRQ priority 128.
- Per-motor command: RFOB torque + gravity torque + motor-specific tanh friction FF.
- New FF model: `tc * tanhf(omega / OMEGA_0) + b * omega`, with `OMEGA_0=0.3 rad/s` and separate positive/negative parameters for each motor.
- Tanh FF gains are independently configured by `INITIAL_K_F1` and `INITIAL_K_F2`; change them live with `KF1 <value>` and `KF2 <value>`. `S` resets them to the configured defaults.
- Stribeck friction remains in the RFOB observer input compensation. No Stribeck friction feed-forward is added to the motor command.
- Gravity constants and calibration match `AK80_GravityCompensation.ino`; `K_G=0.75` and the 1-second startup ramp are preserved.
- RFOB output is limited by `TORQUE_LIMIT`; the final RFOB + gravity + tanh FF sum is stopped if it exceeds `MAX_ABS_COMMAND_TORQUE_NM`.
- Startup feedback timeout is 500 ms; after the first feedback, an age above 3 ms causes a fault stop. Startup sends periodic zero-torque MIT frames, and `S` alone leaves all control outputs disabled.

Commands: `S`, `ON`, `OFF`, `KR <value>`, `KF1 <value>`, `KF2 <value>`, `BIAS`, `STATUS`, `X`.

`STATUS` includes per-motor RX/TX diagnostics, fault number and target motor, RFOB-controller output, gravity torque, new tanh FF torque and gain, and final command torque.
