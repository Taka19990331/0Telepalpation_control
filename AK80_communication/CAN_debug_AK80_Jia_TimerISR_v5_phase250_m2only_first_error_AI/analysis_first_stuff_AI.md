# M2-only first-STUFF diagnostic — analysis

## Procedure

- Baseline derivative: M1 disabled, M2 enabled, CAN2 at 1 Mbit/s.
- No communication configuration changed: 500 us zero-command period, 250 us phase, 50 us timer ISR, 100 us RX polling.
- Before every run, read-only STATUS required `STATE,IDLE`, only M2, and `PERIOD_US=500`.
- Up to five 10-second runs. Firmware latches `FAULT_CAN_STUFF` on the first CAN2 snapshot with `STF_ERR`, captures immediate counters, disables periodic TX, and uses the existing stop path. Host `X` then normalizes the latched state to IDLE. Each run includes read-only STATUS, EVENTS, and HISTORY capture.
- MIT_ENABLE/EXIT and Kp=Kd=τ=0 zero-torque frames only. All five stop zero/exit frames were accepted. No nonzero command was sent.

## Results

| Trial | Detection time | TX/RX_RAW/RX_VALID | Snapshot TEC/REC | Snapshot ESR1; current ESR1 | RX age / max gap (us) | TX failure / TXQ / RX hardware overrun | ISR max / jitter max (us) |
|---:|---:|---:|---:|---|---:|---:|---:|
| 1 | 1.586 s | 3172/3172/3172 | 0/1 | 0x4040A; 0x40008 | 389/1001 | 0/0/0 | 23/0 |
| 2 | 0.786 s | 1572/1572/1572 | 0/9 | 0x4040A; 0x40008 | 313/1000 | 0/0/0 | 18/0 |
| 3 | 3.662 s | 7324/7324/7324 | 0/1 | 0x4040A; 0x40008 | 388/1000 | 0/0/0 | 23/0 |
| 4 | 0.019 s | 38/38/38 | 0/1 | 0x4040A; 0x40008 | 314/1009 | 0/0/0 | 23/0 |
| 5 | 8.881 s | 17762/17762/17762 | 0/1 | 0x4040A; 0x40008 | 388/1000 | 0/0/0 | 23/0 |

The `ESR1=0x4040A` driver snapshots were labeled `STUFF`; current ESR1 at firmware dequeue time was already `0x40008`. The exact physical error time is unavailable in the driver record, so the logged timestamp marks dequeue/detection, not wire time.

## Interpretation

- CAN2 STUFF recurred in 5/5 runs with the same M2-only 2 kHz conditions.
- Detection time varied widely (0.019–8.881 s; 38–17,762 transmitted zero frames). This sample does not fit a simple fixed-time or fixed-frame threshold.
- At each snapshot, RX_RAW=RX_VALID=TX, RX age was 313–389 us, and maximum RX gap was 1000–1009 us. TX failure, TX queue, RX hardware overrun, jitter, and ISR overrun were zero.
- REC varied (1 or 9) while TEC stayed zero. This points toward a receive-side error indication, but is not proof of a physical-layer cause.
- The firmware stopped periodic TX on each first STUFF. Stop zero and MIT_EXIT were accepted, and every post-stop STATUS returned IDLE.
- These measurements isolate neither the physical root cause nor a specific board, cable, transceiver, or bit-timing defect.

## Raw logs

- `logs/first_stuff_trial1_20261112_141456.log`
- `logs/first_stuff_trial2_20261112_141507.log`
- `logs/first_stuff_trial3_20261112_141517.log`
- `logs/first_stuff_trial4_20261112_141530.log`
- `logs/first_stuff_trial5_20261112_141539.log`
