# v5 phase250 CAN communication experiment

## Status

Chat authorization: `EXECUTE APPROVED / MOTOR CAN TX CONDITIONALLY ALLOWED / STAGE 1B ON HOLD`.
Use the existing v5 sketch unchanged for the baseline. Motors remain connected.

## Current hardware status (2026-11-12)

- The original v5 source compiled and was selected in Arduino IDE. After the user pressed Program, read-only STATUS confirmed the running dual-motor v5 configuration (`STATE,IDLE`, period 500 us, RX poll 100 us, M1 and M2 present).
- COM3 is the Teensy USB serial device by VID:PID `16C0:0483`, serial `18363520`, USB location `1-1:x.0`. The startup `READY` line is one-shot and was missed; STATUS fallback safely validated the sketch before each `S`.
- Two-motor v5 at 2 kHz stopped at 2.557 s on a CAN2/M2 STUFF error. M2-only at 2 kHz reproduced it at 2.606 s. M2-only at 1 kHz produced it at 1.119 s with TEC/REC 0/9.
- Each run sent X on the first CAN error; the approved zero/exit frames were accepted and post-stop STATUS confirmed IDLE. No 60- or 600-second run was started after an error.
- The original v5 remains unchanged. The latest loaded test sketch is the M2-only 1 kHz diagnostic derivative; its run also ended in IDLE.

Base source: `../CAN_debug_AK80_Jia_TimerISR_v5_phase250/CAN_debug_AK80_Jia_TimerISR_v5_phase250.ino`

Baseline source SHA-256: `7141017626BAD2E1554E8F779F90D5CDC184588CDF41CA46C30D30F7F3073333`

## Chat-approved CAN TX allowlist

- MIT_ENABLE: standard ID 1 on CAN1 and ID 2 on CAN2, DLC 8, `FF FF FF FF FF FF FF FC`.
- MIT zero command: standard ID 1 or 2, DLC 8; requested position, velocity, Kp, Kd, and torque all zero. Kp=Kd=0 and torque quantization error up to ±0.007814 Nm are accepted.
- MIT_EXIT: standard ID 1 or 2, DLC 8, `FF FF FF FF FF FF FF FD`.
- All other active commands are prohibited. No code edits to v5 before the baseline runs.

The v5 source has one `CAN.write()` wrapper. Its only callers are the periodic zero frame and the MIT enable/exit helpers. The zero frame packs `floatToUint(0, -32, 32, 12)`, which truncates to code 2047 and decodes to approximately -0.007814 Nm.

## Test sequence

1. V1 initial observation: 10 seconds.
2. If both buses have received frames and no safety stop occurred, V2 baseline: 60 seconds.
3. If V2 is stable, V3: 600 seconds.
4. On a communication/fault event, preserve the first event and log, analyze the M1/M2 order, and make one cause-isolating change before retest. Do not retry automatically after a safety fault.

Record TX/RX counts, RX gaps/IDs/DLC/intervals, TEC/REC, ESR1, TX queue, ISR duration/jitter/overruns, and the first fault/event. `X` invokes the v5 approved zero/exit stop path; `STATUS`, `EVENTS`, and post-stop `HISTORY` are read-only.

## Evidence note

The dated `CAN_debug_AK80*.txt` files are code snapshots. No raw serial captures for the recent v5 runs were found locally, so historical 2 kHz/ISR/error claims are not treated as verified baseline evidence.

Build artifacts are under `../../build/V5AI`. The uploaded firmware version is unconfirmed until Teensy Loader reports a completed transfer and v5 emits `READY,`.
