# AK80 通信プログラム（Jia の既存コード）

既存プロジェクトから次のスケッチを無変更でコピーした。
コピー元: `QDD_Jia_Shengxiang_NYU/01_Motor_Identification/Arduino_Sketches/`

| スケッチ | 内容 | Serial |
| --- | --- | --- |
| `motor1ak80/motor1ak80.ino` | MIT CAN通信、外部PD＋1–100 Hz・100秒のトルクチャープ | 115200 |
| `motor2ak80/motor2ak80.ino` | MIT CAN通信、位置保持＋1–20 Hz・20秒のトルクチャープ、Leptrino同期出力 | 921600 |

## 接続・開発環境

- Arduino / Teensy環境、`FlexCAN_T4` ライブラリを使用。
- 両方とも `CAN1`、1 Mbps、標準CAN、`MOTOR_ID = 2`。フォルダ名のmotor1/motor2はCAN IDとは一致しない。
- CANトランシーバ、配線・終端、実機のMITモードとIDを確認すること。Teensyの具体的な型番・配線はこのスケッチだけでは確定できない。
- Arduino IDEでは対象フォルダ内の同名 `.ino` を開く。

## 最初の動作確認の前に

これらは通信だけのテストではなく、同定入力を発生する既存スケッチ。
`motor1ak80` は起動時にモータを有効化し、受信開始後に自動でチャープを実行する。
`motor2ak80` も起動時に有効化し、`AUTO_START = true` により有効な応答から5秒後に試験を開始する。
単純な動作確認に使う場合は、先に自動試験を外したテスト用スケッチを用意すること。

AK80のMIT変換範囲は `sys_identification/chirp_simplified_test1` に合わせ、位置±12.5 rad、速度±37.5 rad/s、トルク±32 N·mに統一した。
AK60の範囲と受信換算は同フォルダのAK60設定に合わせている。

このコピーではコンパイル・実機通信・モータ駆動は未実施。

## 0.14 N·m 開始テスト（新規）

`ak80_low_torque/ak80_low_torque.ino` を使用する。上記2本の原本コピーとは別のスケッチ。
Jiaのmotor1ak80からMIT CANパッキングを流用し、チャープ・位置保持・自動始動を除いた。

- 1 Mbps、Serial 115200。CAN・ID・変換範囲は下記のMOTOR_SELECTに従う。
- Serialで `s` を送るとMITモードを有効化し、応答確認後に正方向0.14 N·m指令を2秒間送る。`x` で停止。改行不要。
- 起動時はゼロトルク指令と、AK80の場合は無効化指令を送る。エンコーダのゼロ設定は行わない。
- 内部kp/kdは0。速度制御ではない。速度絶対値0.10 rad/s以上、応答途絶100 ms、8バイト応答のエラー検出でゼロ指令と無効化指令を送る。
- 速度しきい値0.10 rad/sは約0.95 rpm。これは停止判定であり、実速度の上限を保証しない。停止は駆動解除であり、ブレーキ保持ではない。
- トルク変換は原本と同じ12 bit、±32 N·m、切り捨て。0.14 N·mはコード2056（復号値約0.13285 N·m）となる。0指令にも約−0.00781 N·mの量子化誤差がある。実トルクは未検証。
- ログは `ms,position_rad,speed_rad_s`（20 Hz）。通信送信は1 kHz。

Arduino CLIが見つからないためコンパイル未実施。実機への書込み・駆動も未実施。


## 配線を維持して単体試運転するための選択

スケッチ冒頭の `constexpr char MOTOR_SELECT[] = "80A";` を変更し、再ビルド・書込みする。
char一文字ではなくchar配列で指定する。未定義の文字列はstatic_assertでコンパイルエラーにする。

| 選択 | モータ | CANポート | モータID | フレーム |
| --- | --- | --- | --- | --- |
| `"80A"` | Motor1 / AK80 | CAN1 | 1 | 標準MIT |
| `"80B"` | Motor2 / AK80 | CAN2 | 2 | 標準MIT |
| `"60"` | Motor3 / AK60 | CAN3 | 3 | 拡張MIT、送信0x803、受信0x2903 |

選択したCANコントローラだけを初期化・送受信する。使わないモータは電源を抜き、既存のCAN配線は維持する。
マッピングとプロトコルは `sys_identification/chirp_simplified_test1` の設定に合わせた。
AK80の範囲は位置±12.5 rad、速度±37.5 rad/s、トルク±32 N·m。
AK60は位置±12.56 rad、速度±60 rad/s、トルク±12 N·m、Kp→Kd→位置→速度→トルクのパッキング。
AK60の受信は位置0.1度/bit、速度10 ERPM/bit、14極対・減速比6で出力軸rad/sに変換する。
実機の設定確認は未実施。

全選択で0.14 N·m指令・2秒・速度停止しきい値0.10 rad/sを使用する。AK60のブレークアウェイは未確認。
AK60にはAK80用のFC/FDコマンドを送らず、停止中もゼロトルク指令を1 kHzで継続する。
この停止は駆動無効化やブレーキ保持ではない。AK80は停止時にゼロ指令とFDを送る。
前述の量子化値約0.13285 N·mはAK80の場合。AK60では0.14 N·m指令の復号値は約0.13773 N·m。

## 通信修正 LOW_PROTOCOL_V2

AK80応答の長さ・ID判定を統合版Mode1_Motor1/2.hと合わせた（6バイト以上、CAN IDまたはDATA[0]が対象ID）。AK60はMode1_Motor3.hの拡張フレーム、開始時ゼロ指令、events処理に合わせた。手動開始・0.14 N·m・2秒・100 msタイムアウトはLow独自の試験条件。
停止時のCAN_DIAGにはTX_ATTEMPTS（送信要求）、TX_QUEUE_FAILED（ドライバ書込み失敗）、RX_ALL（試験中の全受信）、RX_ACCEPTED（有効応答）を表示。送信要求の成功は相手への到達を保証しない。
RX_ALL=0なら受信なし、RX_ALL>0かつRX_ACCEPTED=0なら受信したフレームが条件不一致。LAST_RXに最後のフレームを表示する。
USB Serialの接続を最大5秒待ってLOW_PROTOCOL_V2とREADYを表示する。

## LOW_PROTOCOL_V3 診断ログ

READYおよびRUNの数値は設定変数を参照する。DATA行の列名は起動時と各試験開始時に表示する。

- Time_ms、Position_rad、Speed_rad_s：時刻・位置・速度。
- QueuedCommand_Nm：FlexCANが最後に受け付けた通常指令。モータへの到達確認ではない。
- EncodedCommand_Nm：その指令の12bit量子化後の復号値。
- FeedbackRaw：AK80は受信DATA[4]下位4bitとDATA[5]の12bit値。AK60はDATA[4:5]の符号付き16bit電流値。
- FeedbackTorque_Nm：AK80は統合Mode1_Motor1/2と同じ±32 N·m換算。AK60は電流×0.5994 N·m/A。外部センサで測った実トルクではなく、既存コードの換算であり実機仕様の検証は未完了。
- FeedbackCurrent_A：AK60は生値×0.01 A。AK80は換算未確認なのでnan。
- Temperature_C：統合コードと同様に温度バイトの値。フィールドがない場合nan。
- ErrorCode：応答のエラーコード。フィールドがない場合-1。
- FeedbackAge_ms：最後の有効応答からの経過時間。表示の指令と応答は厳密に1対1対応していない。

ログは20 Hz。USB送信バッファに空きがなければ行を省略してCAN処理の遅延を避ける。
停止時はLAST_FEEDBACKにも最後の生値・換算値・エラーを表示する。
