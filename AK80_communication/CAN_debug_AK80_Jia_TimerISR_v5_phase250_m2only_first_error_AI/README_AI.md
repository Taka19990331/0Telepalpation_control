# M2-only first-STUFF capture

This new `_AI` sketch is a one-change diagnostic derivative of the previously tested M2-only v5 source. The original baseline and prior derivatives remain unchanged.

## Experimental change

- Keep CAN initialization, M2-only selection, 500 us periodic zero command (2 kHz), 250 us M2 phase, 50 us timer ISR, 100 us RX polling, and all MIT payloads unchanged.
- When `drainCanErrorSnapshots()` dequeues the first CAN2 error snapshot with `STF_ERR`, latch `FAULT_CAN_STUFF` and stop periodic traffic immediately through the existing fault path.
- Before latching the stop, preserve the driver's ESR1/ECR snapshot, current ESR1/ECR, detection `micros()`, M1/M2 TX/RX_RAW/RX_VALID/RX age/RX gap/RX overrun/TX failure/TX queue, and ISR/jitter maxima.
- The timestamp is the host-loop dequeue/detection time; the FlexCAN_T4 error record does not expose the original wire-error time.
- No Serial output is added to an ISR. Existing loop-side reporting and best-effort zero/exit stop frames remain in use.

## Run plan from Chat

- Up to five 10-second M2-only runs, 500 us period, same physical setup.
- Each run ends at first STUFF auto-stop or another existing stop condition. No test continues periodic TX after a communication error.
- Capture first anomaly, then normalize FAULT_STOPPED to IDLE with the already-approved `X` command; run read-only STATUS/HISTORY/EVENTS collection.
- Only MIT_ENABLE, Kp=Kd=0 and requested torque=0 frames, and MIT_EXIT are allowed.

## Results

## Result (2026-11-12; five trials)

Arduino IDE compiled and uploaded the derivative successfully. Read-only STATUS then confirmed `STATE,IDLE`, M2 only, and `PERIOD_US=500` before every `S`.

All five trials ended on the firmware's first CAN2 STUFF snapshot, with best-effort zero/exit frames accepted and read-only post-stop STATUS confirming IDLE. The capture also saved EVENTS and HISTORY after each run.

| Trial | First STUFF detection (s) | TX / RX_RAW / RX_VALID | TEC/REC snapshot | RX age / max gap (us) | TX_FAIL / TXQ / RX overrun | ISR max / jitter max (us) |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 1.586 | 3172 / 3172 / 3172 | 0/1 | 389 / 1001 | 0 / 0 / 0 | 23 / 0 |
| 2 | 0.786 | 1572 / 1572 / 1572 | 0/9 | 313 / 1000 | 0 / 0 / 0 | 18 / 0 |
| 3 | 3.662 | 7324 / 7324 / 7324 | 0/1 | 388 / 1000 | 0 / 0 / 0 | 23 / 0 |
| 4 | 0.019 | 38 / 38 / 38 | 0/1 | 314 / 1009 | 0 / 0 / 0 | 23 / 0 |
| 5 | 8.881 | 17762 / 17762 / 17762 | 0/1 | 388 / 1000 | 0 / 0 / 0 | 23 / 0 |

Every driver's first snapshot reported `ESR1=0x4040A` with `ERR=STUFF`; by dequeue time, current `ESR1` was `0x40008`. The snapshot timestamp is the `loop()` dequeue time, not the unknown physical wire-error time. TX count equaled RX_RAW and RX_VALID in all five snapshots. First-error TX queue, failure count, and RX hardware overrun were zero. Stop zero/exit were accepted in all five runs, and every run returned to IDLE.

The first-STUFF time ranged from 0.019 to 8.881 seconds (38 to 17,762 frames), so this five-run sample does not support a fixed elapsed-time or frame-count trigger. This result does not identify the physical root cause.

Full raw logs are `logs/first_stuff_trial1_20261112_141456.log` through `logs/first_stuff_trial5_20261112_141539.log`. See `analysis_first_stuff_AI.md` for the concise interpretation.
