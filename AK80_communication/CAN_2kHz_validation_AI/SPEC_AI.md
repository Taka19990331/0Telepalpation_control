# Teensy 4.1 2 kHz timing validation (Stage 1A)

## Scope and safety

This is an offline implementation artifact. It does not authorize uploading to a board or connecting to a motor. `MODE_TIMER_ONLY` is the default and does not initialize or access CAN. `MODE_PASSIVE_RX` is an opt-in compile-time variant; it initializes CAN1/CAN2 at 1 Mbps in `LISTEN_ONLY` and reads frames only. The sketch contains no CAN transmit call. The real-motor CAN transmit allowlist is empty. Do not add MIT enable/exit, position, velocity, torque, or other transmit frames.

## Configuration

- Target: Teensy 4.1, Arduino framework, FlexCAN_T4
- CAN1 / ID 1 and CAN2 / ID 2 labels, 1 Mbps for passive receive mode
- 50 us `IntervalTimer` ISR base by default (20 kHz); `-DTIMER_BASE_PERIOD_US=100` selects the 100 us comparison variant
- 500 us virtual event period (2 kHz)
- 100 us receive polling period (10 kHz per bus) in passive mode only
- One-second serial reporting from `loop()`

`MODE_TIMER_ONLY` is defined as `1` in the sketch. Change it to `0` only for a separately authorized passive receive exercise after checking the installed FlexCAN_T4 API and board/transceiver setup. Listen-only behavior of the installed library and transceiver must be independently verified before any physical connection. This artifact alone does not establish that a bus is safe.

## Metrics

Each one-second record is comma-separated `KEY=value` text, including `TIMER_BASE_US`, `TICK_COUNT`, `TICK_INTERVAL_US`, `VIRTUAL_EVENTS`, `VIRTUAL_INTERVAL_US`, maximum absolute `VIRTUAL_JITTER_US`, cumulative `VIRTUAL_JITTER_VIOLATIONS`, cumulative `MISSED_VIRTUAL_EST`, maximum absolute base-tick `JITTER_US`, maximum `ISR_DURATION_US`, `ISR_OVERRUNS`, `MISSED_DEADLINES`, `RX_POLL_COUNT`, maximum `RX_POLL_DURATION_US`, `RX_RAW_CAN1`, `RX_RAW_CAN2`, and `CAN_TX_COUNT`. `TICK_INTERVAL_US` and `VIRTUAL_INTERVAL_US` are the latest observed intervals. RX values remain zero in timer-only mode. Counters are cumulative since startup. The ISR scheduler and measured intervals describe software behavior and do not prove wire-level CAN timing.

## Provisional evaluation criteria

Evaluate a continuous 60-second to 10-minute capture for each `TIMER_BASE_US` selection. Stage 1A software timing targets from the design review are: 2 kHz virtual event cadence (500 us), maximum absolute virtual-event jitter no more than 50 us, maximum ISR duration no more than half the selected base period (25 us at 50 us, 50 us at 100 us), maximum 100 us receive-poll work no more than 50 us, zero ISR overruns, zero missed deadlines, and zero CAN transmissions. Timer-only mode cannot evaluate receive-poll load. Results from this sketch are measurements, not a hardware test report; no board upload or hardware run is part of Stage 1A.

## Capture and offline analysis

After the physical CAN isolation and formal Stage 1B execution approval, capture each base-period variant for 60 seconds to 10 minutes. The serial capture utility is read-only: it sends no firmware commands and starts timing from the first valid record. Confirm the serial port before use. Opening a USB serial port can reset some boards, so the capture remains gated by physical isolation. Example:

```text
python capture_timing_AI.py COMx logs\50us.log --duration 60
python analyze_timing_AI.py logs\50us.log
```

The capture appends host receive elapsed time to each serial record. The analyzer checks 2 kHz event rate (1990–2010 Hz), at least 60 seconds of capture, virtual-event jitter no more than 50 us, ISR duration no more than half the selected base period, zero virtual jitter violations, virtual misses, ISR overruns, missed deadlines, and CAN transmissions. Timer-only mode cannot evaluate receive-poll load. No capture utility was run during offline preflight.
