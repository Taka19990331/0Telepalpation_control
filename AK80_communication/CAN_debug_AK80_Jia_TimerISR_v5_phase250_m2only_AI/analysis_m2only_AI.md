# CAN2/M2-only cause-isolation result

Date: 2026-11-12 (Asia/Tokyo)

## Change from the two-motor run

The derivative disabled only `MOTOR1_ON` (`true` → `false`). M2 remained enabled on CAN2/ID 2. The 1 Mbit/s CAN bitrate, 500 us period (2 kHz), 250 us phase slot, zero-command payload, and safety-stop code were unchanged. The original v5 file was preserved.

## Measurements

- Before `S`, read-only STATUS confirmed `STATE,IDLE`, only an M2 statistics section, zero prestart RX frames, and zero TEC/REC.
- At 1 s: M2 TX/RX `1999/1999`; TX fail `0`; max RX gap `1000 us`; TX queue max `0`; TEC/REC `0/0`.
- At 2 s: M2 TX/RX `2000/2000`; TX fail `0`; max RX gap `1000 us`; TX queue max `0`; TEC/REC `0/0`.
- ISR maximum was `24 us`; ISR overrun, RX-poll overrun, and measured jitter were zero.
- First anomaly: `2.606 s`, `M2 CAN_IRQ_ERROR_SNAPSHOT`, ESR1 `0x4040A`, decoded `STUFF`, TEC/REC `0/1`.
- `X` stopped periodic traffic at `2.608 s`; M2 zero and exit TX were accepted. Final STATUS reported IDLE. Final counters: TX/RX `5214/5215`, TX fail `0`, TEC/REC `0/7`, max TEC/REC `0/8`, three CAN IRQ snapshots, and one CAN error event.
- Read-only post-stop event history: M2 START → M2 STUFF snapshot at `2.606 s` → M2 STOP at `2.608 s`.

## Comparison and interpretation

The two-motor run produced the same CAN2/M2 STUFF error at `2.556 s`; M2-only reproduced it at `2.606 s`. Both runs were normal through roughly 2 s, with no TX failure, queue growth, or ISR/RX-poll overrun. M1 activity is therefore not required for the observed CAN2 fault. The close recurrence time (~50 ms apart) warrants checking whether the trigger follows elapsed time or a frame count.

Because REC increased while TEC remained zero and the decoded error was STUFF, the evidence is more consistent with a receive-side CAN2 frame error than with a TX API rejection. This is an inference from the controller counters, not proof of a cable, transceiver, motor, or firmware root cause.

Next discriminating comparison: retain M2-only zero traffic and change only the periodic command interval from 500 us to 1000 us (1 kHz). A recurrence near 5.2 s would suggest a frame-count-linked trigger; recurrence near 2.6 s would suggest a time-linked trigger. The test still stops on the first error.
