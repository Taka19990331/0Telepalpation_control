# v5 M2-only cause-isolation experiment

This is a derivative of the Chat-approved v5 baseline for the next cause-isolating run. The original source remains unchanged.

## Single source change

- `MOTOR1_ON`: `true` → `false`
- `MOTOR2_ON`: remains `true`
- CAN2 / Motor ID 2 remains at 1 Mbit/s with a 500 us (2 kHz) zero-command period.
- `TX_M2_PHASE_US` remains 250 us; zero command parameters and fault-stop logic are unchanged.
- Only MIT_ENABLE, Kp=Kd=τ=0 zero frames, and MIT_EXIT are permitted.

Purpose: the preceding two-motor run's first CAN anomaly was a CAN2/M2 STUFF error at 2.556 s, with no M1 CAN error, TX failure, TX queue growth, or ISR/RX-poll overrun. Running M2 alone checks whether that CAN2 error recurs without M1 activity. It does not prove a physical root cause.

## Result (2026-11-12)

The M2-only status check passed (`STATE,IDLE`, only an `M2 TX:` section), and the 10-second capture began. CAN2/M2 produced the same `STUFF` snapshot at 2.606 s (`ESR1=0x4040A`, TEC/REC `0/1`). `X` accepted zero and exit frames; post-stop STATUS reported IDLE. No 60- or 600-second run was started after the first communication anomaly.

At 1 s M2 TX/RX was `1999/1999`; at 2 s it was `2000/2000`. TX failures, TX queue, RX rejects/overruns, RX-poll overruns, and jitter were zero; max ISR duration was 24 us and max RX gap 1000 us. Final counters: TX/RX `5214/5215`, TX fail `0`, max TEC/REC `0/8`, three CAN IRQ snapshots.

The post-stop event order is M2 START → M2 STUFF snapshot at 2.606 s → M2 STOP at 2.608 s. This reproduces the CAN2 error with M1 absent, so M1 activity is not required for the fault. `analysis_m2only_AI.md` compares this run with the two-motor run.

The next M2-only 1 kHz comparison also reproduced STUFF, earlier at 1.119 s (`TEC/REC=0/9`). See `../CAN_debug_AK80_Jia_TimerISR_v5_phase250_m2only_1khz_AI/README_AI.md`. The firmware has remained in IDLE after each stop.

Before capture, the host must receive a read-only `STATUS` signature of `STATE,IDLE,PERIOD_US,500,RX_POLL_US,100` and confirm that only M2 is enabled. If either check fails, the capture utility must not send `S`.

The source filename intentionally matches this sketch folder. Build artifacts and run logs belong under `../../build/M2AI` and this experiment's `logs` folder.
