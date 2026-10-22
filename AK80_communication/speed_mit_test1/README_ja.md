# AK80 MIT速度制御試験

`speed_mit_test1.ino` は、`servo_mode/speed_servo_test1` と同じ1秒ランプアップ、2秒保持、1秒ランプダウンの速度指令試験をMITモードで行うTeensy 4.1用スケッチです。CAN2、ID 2、1 Mbps、100 Hzで制御します。

## MIT速度指令

MITフレームは `sys_identification/chirp_simplified_test1` と同じAK80標準ID・範囲（位置±12.5 rad、速度±37.5 rad/s、トルク±32 Nm）を使います。各周期で `p_des=0`、`kp=0`、`kd=MIT_KD`、`t_ff=0` とし、ランプした速度目標を `v_des` に設定します。MIT内部の速度誤差ダンピングがトルクを発生させます。サーボ版のERPM指令とMIT版のrad/s指令は同じ単位ではありません。

`TARGET_SPEED_RAD_S` と `MIT_KD` は実験前にソースで設定してください。既定速度は2 rad/s、KDは0.2です。速度安全しきい値8 rad/sは停止条件で、実速度上限を保証しません。MITモードとモータIDは実機側で事前に設定してください。

保存先はスケッチ冒頭の `SAVE_TO_SD` で選びます。`true` はSDに保存、`false` はRAMに全サンプルを保持して試験終了後にシリアルへCSV出力します。RAMログは次の試験開始または電源断で失われます。

## 操作・通信・保存

Serial Monitorから1文字を送ります（改行不要）。開始時にMITモード有効化とゼロ指令を送信し、新しいフィードバックを受信してから速度試験を開始します。0.5秒以内に応答がなければ停止してエラーを記録します。

- `g` または `s`: MITモードを有効化して試験開始
- `x`: 試験停止。速度0指令後にAK80終了コマンドFDを送信
- `d`: 最後に保存した試験をCSVでシリアルへ出力

CANフィードバックはID 2の標準8-byte MIT応答を検査し、位置・速度・内部トルク推定値・エラーを復号します。古いフィードバック、モータエラー、速度しきい値超過、CAN送信失敗、タイマーtickの取りこぼしで停止します。通信はFlexCAN_T4を使います。

試行ログをRAMに記録後、終了時にTeensy 4.1内蔵SDへ `SM0000.BIN` の連番で保存します。バイナリ構造は `SPDMIT01` ヘッダ、24-byteサンプル、`ENDSP001` フッタです。`d` コマンドの出力は `SPEED_BEGIN` / `SPEED_END` で囲まれたCSVです。SDへの保存は終了時に行うため、試行中に電源が切れるとその試行は残りません。

PC側は `download_speed_mit.py` で取得できます。Serial Monitorを閉じ、PowerShellで次を実行します（COM番号は環境に合わせて変更）。

```powershell
python -m pip install -r AK80_communication/speed_mit_test1/requirements.txt
python AK80_communication/speed_mit_test1/download_speed_mit.py --list-ports
python AK80_communication/speed_mit_test1/download_speed_mit.py --port COM5
```

最後のコマンドは最新ログをシリアルから読み、`AK80_communication/speed_mit_test1/captures` にCSV・生シリアルログ・検証JSONを保存します。`SAVE_TO_SD=true` なら最新のSDログ、`false` なら現在RAMに残っているログを取得します。新しい試験を実施してから取得する場合は `--run` を付けます。これはモータを駆動するので、試験準備後に明示的に実行してください。

```powershell
python AK80_communication/speed_mit_test1/download_speed_mit.py --port COM5 --run
```

ログ列は `timestamp_us,command_rad_s,position_rad,velocity_rad_s,torque_estimate_nm,error_code,feedback_valid` です。MIT内部トルクはドライバ推定値で、外部トルクセンサ測定値ではありません。

実機への書込み・駆動は行っていません。
