# M1-only 100 us ISR first-error analysis

## Test setup

- Sketch: `CAN_debug_AK80_Jia_TimerISR_v5_m1only_100us_isr_first_error_AI.ino`
- Board/port: Teensy 4.1, COM3
- M1 enabled, M2 disabled
- M1 TX period 500 us; RX polling 100 us; ISR base 100 us
- CAN initialization and communication method unchanged from the M1-only 50 us baseline
- Zero-torque MIT frames only; automatic stop at first CAN error; otherwise X at 20 s
- Five trials, each followed by `STATUS=IDLE`

## Results

| Trial | Stop time (s) | First CAN error | TX/RX_RAW/RX_VALID | Snapshot ESR1/ECR | RX age / max gap (us) | TX_FAIL/TXQ/HW overrun | Captured ISR/jitter max (us) |
|---:|---:|---|---:|---|---:|---:|---:|
| 1 | 3.994 | FORM\|CRC | 7982/7982/7982 | 0x4180A / 0x100 | 476 / 1000 | 0/0/0 | 16/0 |
| 2 | 13.522 | STUFF | 27041/27041/27041 | 0x4040A / 0x100 | 353 / 1000 | 0/0/0 | 15/0 |
| 3 | 12.511 | STUFF | 25017/25017/25017 | 0x4040A / 0x100 | 353 / 1000 | 0/0/0 | 16/0 |
| 4 | 8.976 | STUFF | 17948/17948/17948 | 0x4040A / 0x100 | 369 / 1000 | 0/0/0 | 18/0 |
| 5 | 20.001 | No CAN error; host X stop | 40002/40003/40003 | no error snapshot | see raw log | 0/0/0 | 15/0 |

All five logs include `POST_STOP,STATE,IDLE` and `IDLE_CONFIRMED=1`. In the four error trials, dequeue-time current ESR1 was 0x40008 and TEC/REC was 0/1. The captured CAN error snapshot timestamp is the firmware's dequeue/detection time, not a measurement of the physical error instant.

## Interpretation

- A 100 us ISR did not eliminate the CAN error: four of five trials stopped on a CAN error (three STUFF, one FORM|CRC). Therefore, a 50 us ISR is not a necessary condition for the observed errors.
- One 20 s trial completed without a CAN error, so the error is intermittent; this five-trial sample does not establish a 100% failure rate.
- In error trials, TX, RX_RAW, and RX_VALID counts matched. TX failure/queue, RX hardware overrun, and recorded jitter were zero. These metrics do not prove that the physical bus had no transient errors.
- Captured ISR maximum was 15–18 us. Do not treat the generic `latchFault()` duration as ISR execution time: its interval includes time since the prior tick.
- This comparison does not isolate CAN bit timing versus physical wiring/termination/transceiver versus common software behavior. No physical wiring changes or oscilloscope measurements were performed.

## Raw logs

`logs/m1_100us_isr_trial1_20261112_143439.log` through `logs/m1_100us_isr_trial5_20261112_143555.log`.
