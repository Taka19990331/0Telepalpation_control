# M1-only 100-us ISR comparison

This `_AI` derivative is based on the M1-only first-CAN-error sketch. Its only runtime timing change is `TIMER_BASE_PERIOD_US: 50 -> 100` for the RUNNING scheduler. It keeps M1 on CAN1/ID 1, M2 off, M1 phase 0, 500 us TX, 100 us RX polling, CAN initialization/bitrate, payloads, first-error diagnostics, and automatic stop.

Because M2 is disabled, only the compile-time requirement that M2's 250-us phase align to the timer grid is conditionalized. If M2 is enabled, the phase-alignment and phase-range assertions still apply. The M2 runtime path remains disabled in this test.

## Chat test plan

- M1-only, 2 kHz, 100 us ISR base, up to five trials capped at 20 seconds.
- Before every `S`, STATUS must show IDLE, M1 present, M2 absent, 500 us period, and 100 us RX polling.
- Firmware auto-stops periodic TX on the first CAN error; if no error arrives, host sends `X` at 20 seconds.
- Require post-stop IDLE before reading EVENTS/HISTORY. Stop batch if IDLE cannot be confirmed.
- Zero torque only: MIT_ENABLE/EXIT and `Kp=Kd=tau_requested=0`.

## Baseline comparison

The parent 50-us ISR M1-only batch had CAN errors in 5/5 runs: STUFF in 4 and FORM|CRC in 1. TX/RX_RAW/RX_VALID matched in every captured first-error snapshot; TX failure, queue, RX hardware overrun, and jitter were zero. See `../CAN_debug_AK80_Jia_TimerISR_v5_m1only_first_error_AI/README_M1_ONLY_AI.md` and its five logs.

## Artifacts

- Sketch: `CAN_debug_AK80_Jia_TimerISR_v5_m1only_100us_isr_first_error_AI.ino`
- Batch harness: `run_m1_100us_isr_batch_AI.ps1`
- Logs: `logs/m1_100us_isr_trial*.log`
