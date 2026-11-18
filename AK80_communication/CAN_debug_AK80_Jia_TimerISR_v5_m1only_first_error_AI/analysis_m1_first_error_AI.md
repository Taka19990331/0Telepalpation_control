# M1-only first CAN error — analysis

## Procedure

- M1/CAN1 enabled alone; M2/CAN2 disabled.
- 2 kHz periodic zero command, M1 phase 0, unchanged CAN initialization, 50 us ISR, and 100 us RX polling.
- Five trials, each stopped at first CAN error or 20 s. The firmware latched its first nonzero CAN IRQ error snapshot, stopped periodic TX, and used the existing best-effort zero/exit stop path.
- Before every trial, the harness required `STATE=IDLE`, M1 present, M2 absent, and the expected period/poll values. After stopping, STATUS had to confirm IDLE before EVENTS/HISTORY were read.
- No nonzero motor command was sent.

## Results

| Trial | Detection (s) | Type | TX/RX_RAW/RX_VALID | ESR1/ECR snapshot | RX age / max gap (us) | TX fail / TXQ / HW overrun | Captured ISR / jitter max (us) |
|---:|---:|---|---:|---|---:|---:|---:|
| 1 | 0.921 | FORM\|CRC | 1842/1842/1842 | 0x4180A / 0x100 | 458/1000 | 0/0/0 | 15/0 |
| 2 | 1.049 | STUFF | 2097/2097/2097 | 0x4040A / 0x100 | 354/1000 | 0/0/0 | 15/0 |
| 3 | 0.949 | STUFF | 1896/1896/1896 | 0x4040A / 0x100 | 353/1000 | 0/0/0 | 16/0 |
| 4 | 0.323 | STUFF | 645/645/645 | 0x4040A / 0x100 | 353/1000 | 0/0/0 | 16/0 |
| 5 | 8.396 | STUFF | 16790/16790/16790 | 0x4040A / 0x100 | 354/1000 | 0/0/0 | 15/0 |

All current ESR1 values at dequeue were 0x40008; TEC/REC snapshots were 0/1. RX_RAW and RX_VALID equaled TX in every run. TX failures, TX queue, RX hardware overrun, and jitter were zero. Stop zero/exit frames were accepted, and all five post-stop STATUS checks returned IDLE.

## Interpretation and caveats

- A CAN error occurred in 5/5 M1-only trials, so this setup does not support a CAN2-only/M2-only fault hypothesis. Four first snapshots were STUFF and one was FORM|CRC.
- Detection time varied from 0.323 to 8.396 s (645 to 16,790 frames). No fixed time/frame threshold follows from this small sample.
- The driver's record gives loop dequeue/detection time, not physical wire-error time. TX/RX counter equality does not show that the bus had no transient errors.
- Use `FIRST_CAN_ERROR_ISR_MAX_US` (15–16 us) for the ISR maximum at first-error capture. The generic fault summary printed 154–260 us because `latchFault()` includes elapsed time since the previous timer tick; that number is not the ISR execution duration.
- These measurements do not identify a specific transceiver, cable, termination, driver, or bit-timing defect.

## Raw logs

- `logs/m1_first_error_trial1_20261112_142607.log`
- `logs/m1_first_error_trial2_20261112_142617.log`
- `logs/m1_first_error_trial3_20261112_142627.log`
- `logs/m1_first_error_trial4_20261112_142637.log`
- `logs/m1_first_error_trial5_20261112_142647.log`
