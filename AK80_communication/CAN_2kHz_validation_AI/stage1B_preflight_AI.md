# Stage 1B offline preflight

> Superseded on 2026-11-12 by the latest Chat directive: Stage 1B is on hold, and the approved path is actual v5 two-motor CAN communication with the allowlisted enable, zero, and exit frames. The prior `UPLOAD_BLOCKED` physical-isolation gate below is historical and no longer governs the task. Current v5 experiment status is tracked in `../CAN_debug_AK80_Jia_TimerISR_v5_phase250_experiment_AI/README_AI.md`.

Complete Teensy 4.1 Arduino CLI builds with 500 us virtual-event timing metrics. No board upload or device access occurred.

| Variant | Effective compiler flags | HEX SHA-256 | ELF SHA-256 |
|---|---|---|---|
| 50 us | default build.flags.cpp; TIMER_BASE_PERIOD_US=50 from source default | `86F9D40D68761AE23AD18B39105F32EC439219D9B6494F9AE7F8DD71330E1FDC` | `50BE5A018BCF10F35BA8C5965E5511C1B7196F0E92029B1DFF73658D88ABD678` |
| 100 us | build.flags.cpp adds -DTIMER_BASE_PERIOD_US=100 | `1FE73EEC20365728D401519B3DD6C4070EFCFCE5458573F499A25DED8750C27A` | `BF957184A1A786325BD03B43A1E680CB8E399D4C0B23A5D1BD2C1FA54BB8658C` |

Both configurations linked successfully. Memory: FLASH code 21,348 bytes; RAM1 variables 25,440 bytes; RAM2 variables 12,416 bytes. Only unused FlexCAN_T4 ISR warnings appeared. Each compile_commands.json records its effective compiler arguments.
An initial CLI attempt using compiler.cpp.extra_flags was ignored by the Teensy platform; that invalid artifact was replaced. Correct 100 us compile commands contain -DTIMER_BASE_PERIOD_US=100 and the HEX differs from 50 us.

Firmware safety: MODE_TIMER_ONLY=1; CAN initialization and RX/TX code are guarded out by #if !MODE_TIMER_ONLY; timer auto-starts in setup(); no START/STOP/FINAL serial commands. New metrics are measured on virtual-event timestamps inside the timer ISR.
Static sketch-object undefined references: IntervalTimer::beginCycles and usb_serial_write only; no FlexCAN symbols or TX functions.
Board discovery: Arduino CLI reports Teensy 4.1 at Port_#0001.Hub_#0002; COM3/5/6/7/8 are unidentified serial ports. Discovery only; no connection or upload. The capture utility requires the confirmed Teensy serial port; do not guess among the unknown COM ports.
Chat formally approved revised Stage 1B EXECUTE conditionally. Physical CAN isolation confirmation remains the only execution gate: Chat explicitly states `UPLOAD_BLOCKED` until Miyamoto reports the isolation complete in Slack. No upload, serial connection, or hardware run occurred.

The 500 us virtual-event instrumentation records latest interval, maximum absolute jitter, cumulative >50 us violations, and estimated missed virtual events. `capture_timing_AI.py` is a read-only serial logger that adds host elapsed time; it was not run. `analyze_timing_AI.py` now checks cadence, capture duration, jitter, ISR duration, missed events/deadlines, overruns, and zero CAN TX. Python syntax compilation and a synthetic 60-second 2 kHz log passed. These are offline checks, not hardware results.
