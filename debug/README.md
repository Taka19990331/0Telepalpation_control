# Dual CAN zero-command diagnostic

Teensy 4.1 diagnostic sketch for **Motor ID 1 on CAN1** and **Motor ID 2 on CAN2**. It enables both motors, then sends a zero MIT torque command to each bus on every 500 µs timer tick (2 kHz per motor). The timer interrupt remains enabled during normal operation.

At the first detected error the sketch latches the fault, stops the timer, attempts one zero command and disable command on both buses, prints the fault snapshot and the most recent 4096 control ticks, then remains halted until reset. It never resumes the run automatically.

## Capture

1. Open `debug.ino` in Arduino IDE and upload it to the Teensy 4.1. The sketch uses the installed Teensy `FlexCAN_T4` library.
2. In PowerShell, start serial capture:

   ```powershell
   .\capture.ps1 -Port COM7
   ```

   Replace `COM7` with the Teensy's port. The script writes timestamped serial output to `debug\logs\dual_zero_YYYYMMDD_HHMMSS.log` and exits with code 2 after a fault dump completes. Press Ctrl+C to stop a run manually.

## What is logged

- One paired sample per 500 µs tick: both `write()` results, both TX queue depths, both feedback ages and counts, both CAN ESR/ECR snapshots, motor error bytes, tick interval and ISR duration.
- A 10 Hz live line while running.
- A rolling 4096-row history (2.048 s) around the first fault, followed by shutdown command results.
- PowerShell UTC receive timestamps for every line.

In the installed Teensy FlexCAN_T4 implementation, `write()` returns `1` when assigned to a mailbox, `-1` when accepted into the software queue, `0` when rejected, and `-2` when the bus is off. This diagnostic treats anything other than `1` as a fault so queued or rejected transmissions cannot be mistaken for a direct 2 kHz send.

The sketch also stops on a nonzero motor error byte, a CAN error counter/event, no startup feedback within 500 ms, feedback silence longer than 5 ms, or a control ISR duration of 500 µs or more. A fault is latched until reset; preserve the complete log before restarting.
