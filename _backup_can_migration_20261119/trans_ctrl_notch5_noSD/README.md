# trans_ctrl_notch5_noSD

`trans_ctrl_notch5_noSD.ino` is the Teensy 4.1 controller sketch derived from `trans_ctrl_notch5` with microSD access and all session logging / CSV transfer features removed.

This version does not initialize or reset an SD card, allocate a recording ring buffer, record control samples, write log files, or provide the `D` CSV transfer command. The `S` command starts or reinitializes the controller session only.

The controller, CAN handling, safety stops, notch modes, and runtime serial settings are retained. Available commands are shown at startup: `S`, `ON`, `OFF`, `KR`, `KF`, `NOTCH`, `ANGLE_START`, `ANGLE_FULL`, `STATUS`, `BIAS`, and immediate stop `X`.

Open this folder as an Arduino sketch and select **Teensy 4.1**. Configuration defaults remain in `control_config.h`.

