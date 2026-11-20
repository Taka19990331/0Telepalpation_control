# trans_ctrl_movAve_noSD

`trans_ctrl_movAve_noSD.ino` is the Teensy 4.1 controller sketch derived from `trans_ctrl_notch5` without microSD access.

This version does not initialize an SD card or write log files. After `S`, it records CAN feedback timestamp, position, and raw velocity in a 16,384-sample RAM ring buffer. When full, the buffer overwrites its oldest samples. Send `D` while the control session is stopped to print the retained samples as CSV over Serial. Starting a new session with `S` clears the previous log.

RFOB velocity uses a 5-point moving average instead of the velocity LPF. The notch mode defaults to OFF and can still be changed at runtime while control is OFF. The controller, CAN handling, safety stops, and runtime serial settings are retained. Available commands are shown at startup: `S`, `ON`, `OFF`, `KR`, `KF`, `NOTCH`, `ANGLE_START`, `ANGLE_FULL`, `STATUS`, `BIAS`, `D` (dump log while stopped), and immediate stop `X`.

Open this folder as an Arduino sketch and select **Teensy 4.1**. Configuration defaults remain in `control_config.h`.


