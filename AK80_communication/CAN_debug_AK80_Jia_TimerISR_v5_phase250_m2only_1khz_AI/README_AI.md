# M2-only 1 kHz cause-isolation comparison

This derivative is based on the M2-only v5 experiment. It changes exactly one additional setting: `COMMAND_PERIOD_US` from `500` to `1000` us. M1 remains disabled, M2 remains on CAN2/ID 2, CAN bitrate remains 1 Mbit/s, `TX_M2_PHASE_US` remains 250 us, and the MIT command remains Kp=Kd=τ=0.

The two preceding 2 kHz runs both detected the same CAN2/M2 STUFF error at about 2.6 seconds. This comparison distinguishes a trigger tied more closely to elapsed time from one tied to transmitted frame count. It is diagnostic only; it does not replace the required 2 kHz v5 result. Stop at the first communication error.

Before `S`, the host must use read-only STATUS to confirm `STATE,IDLE`, `PERIOD_US,1000`, `RX_POLL_US,100`, and that only M2 is enabled. The capture harness refuses to send `S` unless all expected conditions match.

## Result (2026-11-12)

The prestart status check confirmed M2-only, IDLE, and the 1000 us period. At 1 s, M2 TX/RX was `999/999`, TX fail `0`, TX queue max `0`, TEC/REC `0/0`, and max RX gap `1201 us`. ISR max was `18 us`; ISR/RX-poll overruns and measured jitter were zero.

At 1.119 s the first error was M2 `CAN_IRQ_ERROR_SNAPSHOT`, ESR1 `0x4040A`, decoded `STUFF`, TEC/REC `0/9`. `X` stopped the stream at 1.121 s; the zero/exit frames were accepted and STATUS reported IDLE. Final M2 TX/RX was `1120/1121`, TX fail `0`, max TEC/REC `0/9`, and three CAN IRQ snapshots. Post-stop event order: M2 START → M2 STUFF at 1.119 s → M2 STOP at 1.121 s.

The error also occurred in both 2 kHz runs (2.556 s and 2.606 s), so the 1 kHz result does not support a simple fixed elapsed-time or fixed frame-count threshold. It indicates M2/CAN2 remains the common locus; it does not prove a physical or software root cause. The capture stopped on the first error as required.
