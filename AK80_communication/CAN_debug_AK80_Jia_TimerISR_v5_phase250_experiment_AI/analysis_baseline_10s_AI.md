# v5 two-motor initial communication run

Date: 2026-11-12 (Asia/Tokyo)

## Conditions

- Source: `../CAN_debug_AK80_Jia_TimerISR_v5_phase250/CAN_debug_AK80_Jia_TimerISR_v5_phase250.ino`
- Source SHA-256: `7141017626BAD2E1554E8F779F90D5CDC184588CDF41CA46C30D30F7F3073333`
- Board: Teensy 4.1; serial port COM3 (USB VID:PID `16C0:0483`, serial `18363520`)
- Firmware identity/state: read-only `STATUS` confirmed `STATE,IDLE`, `PERIOD_US=500`, `RX_POLL_US=100`; both motor prestart RX counts were zero.
- CAN: 1 Mbit/s; M1 and M2 on; 500 us period (2 kHz per motor); M2 API phase 250 us.
- Command policy: only MIT_ENABLE, quantized zero torque (Kp=Kd=0), and MIT_EXIT. The host sent `S`, then `X` after the first CAN error. No nonzero drive command was sent.
- Capture: `logs/baseline_10s_status_resume_20261112.log`; post-stop read-only queries: `logs/poststop_history_20261112.log`.

## Results

At 1 s, M1 accepted 1,998 periodic zero frames and received 1,998 frames; M2 accepted 1,997 and received 1,998. At 2 s, both buses reported 2,000 TX and 2,000 RX in that interval. TX failures and queue depth were zero. Maximum RX gap was 1,000 us on M1 and 1,001 us on M2. ISR maximum was 38 us; ISR overruns, RX-poll overruns, and measured jitter were zero. Maximum API phase error was 36 us.

The first anomalous event was on M2/CAN2 at 2.556 s: `CAN_IRQ_ERROR_SNAPSHOT`, ESR1 `0x4040A`, decoded `STUFF`, TEC/REC `0/1`. Event history contains three records in this order:

1. `[0.000s] M1 START` (the firmware records the shared start event against M1 while both motors are enabled)
2. `[2.556s] M2 CAN_IRQ_ERROR_SNAPSHOT ... ERR=STUFF`
3. `[2.557s] M1 STOP` (the shared stop event is likewise recorded against M1)

No M1 CAN error event preceded it. The periodic stream was stopped at 2.557 s. `X` reported zero and disable frames accepted on both buses. Final STATUS reported IDLE. Final counters were M1 TX/RX `5114/5114`, TX fail `0`, max TEC/REC `0/0`; M2 TX/RX `5113/5114`, TX fail `0`, max TEC/REC `0/8`, four CAN IRQ snapshots, and one CAN error event. The one-error snapshot did not coincide with observed TX queue growth, TX API failure, RX rejection/overrun, ISR overrun, or RX-poll overrun.

## Interpretation and next cause-isolating run

Evidence localizes the first detected CAN fault to CAN2/M2, while both buses otherwise sustained the intended 2 kHz communication for about 2.56 s. The run does not establish whether the CAN2 STUFF error comes from the M2 bus/device or from interaction with the two-motor schedule.

The next minimal comparison disables only M1 and leaves M2 on CAN2 at the same 1 Mbit/s, 500 us period, zero-command policy, and 250 us phase slot. If CAN2 shows the same fault alone, the two-motor interaction is less likely; if it does not recur, shared scheduling or the removed M1 activity becomes a candidate. This is evidence for cause separation, not proof of a physical root cause. The original v5 remains unchanged.
