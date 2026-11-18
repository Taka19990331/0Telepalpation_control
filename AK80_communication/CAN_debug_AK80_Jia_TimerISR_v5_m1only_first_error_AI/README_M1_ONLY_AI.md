# M1-only first CAN error comparison

This derivative is based on the completed M2-only first-STUFF diagnostic. It changes only motor selection and broadens the first-error latch from STUFF-only to any nonzero CAN IRQ error snapshot (including bus-off).

## Fixed settings

- M1 on CAN1, motor ID 1; M2/CAN2 disabled.
- 2 kHz periodic zero-torque command (`COMMAND_PERIOD_US=500`); M1's existing phase 0 is unchanged.
- CAN initialization/bitrate, 50 us timer ISR, 100 us RX polling, payload packing, and other communication processing remain unchanged.
- First CAN error snapshot records the driver's dequeue time, snapshot and current ESR1/ECR, all active-motor TX/RX counters, RX age/gap, queue/failure/overrun, and ISR/jitter maxima, then latches `FAULT_CAN_ERROR` to stop periodic TX.
- The saved time is when `loop()` dequeues the driver's record. It is not the physical wire-error time.
- MIT_ENABLE/EXIT and zero-torque commands (`Kp=Kd=tau_requested=0`) only.

## Chat test plan

- M1-only at 2 kHz, up to five trials; each is capped at 20 seconds.
- Before each `S`, the harness requires STATUS `STATE=IDLE`, M1 present, M2 absent, 500 us period, and 100 us RX polling.
- A first CAN error automatically stops periodic TX. If no error occurs within 20 seconds, host sends `X`.
- Require post-stop `STATE=IDLE`, then collect EVENTS and HISTORY. Stop the batch if IDLE cannot be confirmed.
- Harness allowlist: `STATUS`, `S`, `X`, `EVENTS`, `HISTORY`.

## Results (2026-11-12)

The derivative compiled and uploaded with Teensy 4.1 selected. All five trials reached the firmware's first CAN1 error stop before the 20-second cap. The four later trials were STUFF; trial 1 was FORM|CRC. Each trial accepted best-effort zero/exit frames and post-stop STATUS confirmed IDLE.

| Trial | First error (s) | Type | TX / RX_RAW / RX_VALID | Snapshot ESR1 / ECR | RX age / max gap (us) | TX_FAIL / TXQ / RX overrun | Captured ISR max / jitter max (us) |
|---:|---:|---|---:|---|---:|---:|---:|
| 1 | 0.921 | FORM\|CRC | 1842 / 1842 / 1842 | 0x4180A / 0x100 | 458 / 1000 | 0 / 0 / 0 | 15 / 0 |
| 2 | 1.049 | STUFF | 2097 / 2097 / 2097 | 0x4040A / 0x100 | 354 / 1000 | 0 / 0 / 0 | 15 / 0 |
| 3 | 0.949 | STUFF | 1896 / 1896 / 1896 | 0x4040A / 0x100 | 353 / 1000 | 0 / 0 / 0 | 16 / 0 |
| 4 | 0.323 | STUFF | 645 / 645 / 645 | 0x4040A / 0x100 | 353 / 1000 | 0 / 0 / 0 | 16 / 0 |
| 5 | 8.396 | STUFF | 16790 / 16790 / 16790 | 0x4040A / 0x100 | 354 / 1000 | 0 / 0 / 0 | 15 / 0 |

The saved first-error ISR field was 15–16 us on every run. The generic fault summary also printed 154–260 us because the existing `latchFault()` report includes the elapsed time since the previous tick; the first-error snapshot field is the captured ISR maximum and is the appropriate value for comparison. Current ESR1 at dequeue was 0x40008 in every trial. TEC/REC snapshot was 0/1 throughout. Detection timestamps are loop dequeue times, not the physical wire-error times.

These results show the error is not confined to CAN2/M2 under this setup. They do not establish a particular physical cause.

Raw logs are `logs/m1_first_error_trial1_20261112_142607.log` through `logs/m1_first_error_trial5_20261112_142647.log`.

## Artifacts

- Sketch: `CAN_debug_AK80_Jia_TimerISR_v5_m1only_first_error_AI.ino`
- Batch harness: `run_m1_first_error_batch_AI.ps1`
- Trial logs: `logs/m1_first_error_trial*.log`
