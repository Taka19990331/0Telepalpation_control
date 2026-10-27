# VelRamp Teensy sketch

`fric_VelRamp.ino` is for Teensy 4.1 and the AK80 (motor ID 1 on CAN1 or ID 2 on CAN2). It samples and commands at 2 kHz, buffers rows in RAM, and writes them to the built-in SD card while the run is active. It emits no status or telemetry text during the run.

## Profile

The schedule uses 0.1 rad/s increments. It ramps 0 to +1 rad/s in 1 s per step, +1 to +10 in 0.2 s per step, +10 back to +1 in 0.2 s per step, and +1 to zero in 1 s per step. It then mirrors the schedule to -10 rad/s and returns to zero. The request specified +10 rad/s followed by a descending branch starting at 20 rad/s; to avoid a discontinuous speed command, this implementation starts the descending branch at +10 rad/s. The negative peak is -10 rad/s.

## Use

1. Set the Teensy board to Teensy 4.1, insert the SD card, and load the sketch.
2. Ensure the motor is in MIT mode and the wiring matches `velramp_config.h`: ID 1 on CAN1 or ID 2 on CAN2.
3. Send `s` over USB Serial to start. Send `x` to abort; the run is then finalized on SD.
4. After the run, use PowerShell:

   ```powershell
   .\download_velramp.ps1 -Port COM5
   ```

   To start and then automatically retrieve the run, use `-Start`:

   ```powershell
   .\download_velramp.ps1 -Port COM5 -Start
   ```

The CSV is saved in `data` as `VelRamp_yyyyMMdd_HHmmss.csv`. Its leading `#key,value` rows contain sample rate, gains, limits, increment timing, and run status; the header and sample rows follow. The Teensy retains the latest run as a binary SD file (`VR0000.BIN`, incrementing without overwriting older logs). `d` transfers the last saved run. `x` is an abort command and is not part of normal operation.

`torque_estimate_nm` is the motor driver's estimate, not a separate torque sensor measurement. Gains and safety limits are in `velramp_config.h`.
