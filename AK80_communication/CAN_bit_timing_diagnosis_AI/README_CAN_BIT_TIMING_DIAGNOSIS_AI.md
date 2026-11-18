# CAN bit timing diagnosis

This diagnostic-only derivative records the active FlexCAN clock and timing registers after CAN initialization. It does not change CAN bitrate, phase, ISR timing, RX polling, CAN initialization calls, or the existing first-CAN-error stop path.

## Variants

- M1_only/CAN_debug_AK80_Jia_TimerISR_v5_m1only_bit_timing_diag_AI.ino: M1 on, M2 off.
- M2_only/CAN_debug_AK80_Jia_TimerISR_v5_m2only_bit_timing_diag_AI.ino: M1 off, M2 on.

Both use the existing first-error firmware stop behavior, 500 us TX period, 50 us ISR base, and 100 us RX polling. They preserve the zero-torque-only command profile.

## Logged values and decoding

After begin() and setBaudRate(1,000,000), the sketch prints the CCM clock source selector, source frequency from the Teensy 4.1 clock definitions, PODF, calculated peripheral clock, and raw MCR/CTRL1/CBT/CTRL2. It also prints both the bitrate calculated from source/(PODF+1) and the FlexCAN_T4 1.62.0 model bitrate. The installed library's private getClock() maps the source selector to 60/24/80 MHz and setBaudRate() uses that value directly; logging both makes any PODF/model discrepancy visible rather than assuming they match. The sketch then decodes classic CTRL1 timing fields:

- PRESDIV divisor = encoded field + 1
- PROPSEG, PSEG1, PSEG2, RJW in time quanta = encoded field + 1
- SMP = CTRL1 bit 7
- total TQ = SyncSeg (1) + PROPSEG + PSEG1 + PSEG2
- calculated bitrate = peripheral clock / (prescaler divisor × total TQ), plus the library-model calculation
- sample point = (SyncSeg + PROPSEG + PSEG1) / total TQ

CBT is recorded as a raw register value only. The installed FlexCAN_T4 1.62.0 setBaudRate() implementation configures the classic CTRL1 fields; no CBT interpretation is assumed.

## Test protocol

Run each variant for two trials, each capped at 20 seconds. Harnesses verify the expected single motor, IDLE, 500 us period, and 100 us RX poll before sending S; on first CAN error they capture the firmware stop report, send X to normalize the state, and require post-stop IDLE. A clean 20-second run is stopped with X. Logs include startup register lines, runtime status, and stop output.

- M1_only/run_m1_bit_timing_diag_batch_AI.ps1
- M2_only/run_m2_bit_timing_diag_batch_AI.ps1

No oscilloscope or CAN analyzer is currently available. No wiring, termination, supply, or CAN timing settings were changed.
