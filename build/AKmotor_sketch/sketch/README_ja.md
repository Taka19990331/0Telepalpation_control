#line 1 "C:\\0Telepalpation_control\\plain_programs\\AKmotor_sketch\\README_ja.md"
# AKmotor_sketch

Teensy 4.1用の一定トルク送信・SD記録スケッチです。タイマ周期は`can_protocol.h`内の`SAMPLE_HZ=2000`で、メインスケッチ先頭の`tau_cmd=0.8f`を変更すると指令トルクを変えられます。送信フレームはMIT形式で、位置・速度・Kp・Kdは0、トルクだけ指定します。

## モータ選択

`can_protocol.h`の`MOTOR_SELECT`を変更してビルド・書込みします。

| 選択 | CAN | ID | 形式 |
|---|---:|---:|---|
| `"80A"` | CAN1 | 1 | AK80標準CAN |
| `"80B"` | CAN2 | 2 | AK80標準CAN |
| `"60"` | CAN3 | 3 | AK60拡張CAN |

## 操作と記録

- `s`: 新しいログを作成してMITモードに入り、2 kHzで指令・記録を開始。
- `x`: ゼロトルク指令とAK80のMIT無効化を送り、記録を確定。時間による自動停止はありません。
- `d`: 最新の確定ログをCSVとしてUSBシリアルへ転送。

SDファイルは`AK0000.BIN`からのバイナリ形式で、ヘッダ・サンプル・フッタを含みます。各サンプルは開始からの経過µsと指令トルクです。CSVには周期・ID・トルクのメタデータとサンプル列を出力します。PCへの転送例:

```powershell
.\download_AKmotor_sketch.ps1 -Port COM5
```

実験を始める前にモータを安全な状態にし、選択したCAN配線・IDを確認してください。`x`またはCAN送信失敗・バッファ異常で停止します。現状、受信フィードバック監視や速度制限はありません。AK60の停止はゼロ指令で行い、AK80のようなFC/FDモード指令は送信しません。

## 開き方と読み出し手順

Arduino IDEで、このフォルダ内の `AKmotor_sketch.ino` を開き、ボードを Teensy 4.1、USB Typeを Serial に設定して書き込みます。Teensyボードパッケージ、FlexCAN_T4、SdFatが必要です。元の `controller_implementation` フォルダは参照しません。初期設定は `80B`（CAN2 / ID 2）、0.8 Nmです。

1. Teensy内蔵SDスロットにカードを入れ、起動時の `SD,OK` と `TIMER,OK` を確認します。
2. シリアルモニタ（115200 baud）で `s` を送り、記録を開始します。
3. `x` を送り、`SAVED,AKxxxx.BIN,ROWS,...,STATUS,1` が出るまで待ちます。
4. シリアルモニタを閉じてCOMポートを解放し、このフォルダで `powershell -ExecutionPolicy Bypass -File .\download_AKmotor_sketch.ps1 -Port COM5` を実行します。COM番号は実機に合わせます。
5. `data\AKmotor_sketch_yyyyMMdd_HHmmss.csv` に保存されます。保存先は `-OutputDirectory` で変更できます。先頭の `#` 行はメタデータで、データ列は `time_us,command_torque_nm` です。

転送識別子は `AKMOTOR_BEGIN` / `AKMOTOR_END`、バイナリの識別子は `AKMOTOR1` / `ENDAK001` です。このスケッチと同梱のダウンロードスクリプトを組み合わせて使用してください。旧スケッチの `TCxxxx.BIN` は対象にせず、同じSDカードでも別名で記録します。記録対象は送信に成功した指令トルクで、モータから受信した実測値ではありません。