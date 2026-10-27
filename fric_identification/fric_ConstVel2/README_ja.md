# ConstVel Teensy sketch

`fric_ConstVel.ino`はTeensy 4.1とCAN2接続のAK80（ID 2）を、設定した速度指令で一定時間動かしながらデータ取得します。制御周期と記録周期は2 kHzです。実行中は速度PI制御、速度指令、位置、実速度、ドライバ推定トルク、積分トルク指令をSDカードへ記録します。

## 設定

`constvel_config.h`で次を指定します。

- `TARGET_SPEED_RAD_S`: 目標速度 [rad/s]。符号で回転方向を選びます。初期値は`+1.0`。
- `HOLD_DURATION_US`: 記録時間 [µs]。初期値は30秒です。SDの事前確保に合わせて最大120秒にしてください。
- `MOTOR_ID`, `CAN_BAUD`: CAN設定。
- `MIT_KD`, `SPEED_KI_NM_PER_RAD`, `MAX_I_TORQUE_NM`: 速度制御ゲインと積分トルク制限。

目標速度が`MAX_ABS_SPEED_RAD_S`またはMIT速度範囲を超える設定では開始しません。通信タイムアウト、モータエラー、速度超過、記録バッファ異常を検出した場合は停止指令を送り、可能な範囲のデータをSDカードへ確定します。

## 実行とデータ取得

1. Arduino IDEでTeensy 4.1を選び、`fric_ConstVel.ino`を開いて書き込みます。SDカードを挿入し、モータをMITモードにしてCAN2へ接続してください。
2. USBシリアルで`s`を送ると、フィードバック受信を確認してから一定速度の記録を開始します。設定時間が経過するとゼロ速度指令を送り、自動停止します。`x`で途中停止できます。
3. CSVをPCへ転送します。開始と転送を一括で行う場合は、PowerShellで次を実行します。

   ```powershell
   .\download_constvel.ps1 -Port COM5 -Start -DurationSeconds 30
   ```

   `-DurationSeconds`は`constvel_config.h`の`HOLD_DURATION_US`と一致させてください。すでに実験を終えている場合は`-Start`を外すと、SD上の最新ログを取得します。

CSVは`data\ConstVel_yyyyMMdd_HHmmss.csv`に保存されます。先頭の`#key,value`行にはサンプリング周期、制御ゲイン、目標速度、保持時間、終了状態が記録され、その後に列名とサンプル行が続きます。Teensy上では`CV0000.BIN`以降の名前でSDログを保存し、`d`で最新ログを転送します。`torque_estimate_nm`はモータドライバの推定値で、独立したトルクセンサの計測値ではありません。
