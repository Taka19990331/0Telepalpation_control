# 通信・ロギング分離の変更報告

## 比較と変更理由

|項目|AK80_timer_test|変更前trans|修正版trans|
|---|---|---|---|
|RXコールバック|RAMへフレーム保存|デコード後にcontroller.feedbackを実行|時刻付き64件FIFOへ保存|
|観測器・摩擦・LPF|なし|RX ISRでexpfを含む計算|制御ISRで新規フレーム最大4件／周期|
|記録|RAM保持、停止後出力|loopで全サンプルをCSV整形してSD書込み|RAMコピー、loopで64件バイナリ書込み|
|CAN設定|全MB割込み・共通コールバック|MB0 RX割込み・MB0コールバック|変更前を維持|
|試験長|各周波数1秒|停止まで|2 kHz連続、Xまで（60秒以上も可）|

重い処理は`receiveFeedback→handleFeedback→Controller::feedback`の速度LPF・Stribeck・Qフィルタ（expf）と、`SaveData::service`の多数の浮動小数点snprintfだった。
これらが13.4秒の送信失敗原因だったとは断定していない。SDなしでも同じCAN・観測器を動かせるため、負荷とバス状態を比較できる。

制御アルゴリズムのファイル`control_algorithm.h`は変更前と同一。J、帯域、摩擦係数、ゲイン初期値、±1 Nm、ON/OFF時の観測継続、NaN/Inf・100 msタイムアウトを維持した。
新たな補償・速度停止・デッドバンドは追加していない。RX欠損が確定するソフトFIFO満杯／HWオーバーランはfault=9で停止する。
古さ2000 usと受信間隔250～1000 usの逸脱はカウンタのみで、観測器の時刻差や制御則を変更しない。

## FlexCAN_T4実装確認

確認対象はローカルTeensy core 1.62.0同梱`libraries/FlexCAN_T4/FlexCAN_T4.tpp`。

- `enableFIFO(false)`（約181～218行）はFIFOなしでMB0～7=RX（0～3標準、4～7拡張）、MB8～15=TXとし、TX割込みを有効化する。
- `setMaxMB(16)`（約457行）は内部でdisableFIFOを呼ぶ。本構成では16 MBの割当てを維持する。
- `enableMBInterrupt(MB0)`（約235行）は他のTX割込みを消さない。MB0のみRX受信有効でもTX8～15の割込みは残る。
- 全RXをREJECT_ALLにし、MB0だけACCEPT_ALL。既存の「標準8 byte・payload ID2」判定をソフト側に維持する。受信CAN IDが送信IDと違う機器応答を排除する新フィルタは追加しない。
- `write()`（約1051行）は空きTX MBを探索し、なければソフトTXキューへ入れる。戻り値非ゼロは配送完了・ACKの証明ではない。
- `struct2queueRx()`（約1171行）はevents未使用ならISR内で直接callbackを呼ぶ。`events()`は追加していない。

旧版の`enableMBInterrupts()`へ変える必然性は確認できず、RX経路を増やす設定変更は行わなかった。TXキュー最大値・空きMB・ESR1/ECR・最初の送信失敗を採取する。

## 記録形式と診断の確認

TCBIN2は条件メタデータ＋120 byteレコード（連番4、Sample112、CRC32 4）＋CRC付き終了レコード＋従来の診断テキスト。
CRC32はPCのzlib.crc32と同じ。Teensy ABIのサイズ・オフセットをstatic_assertで固定。2 kHz全行、約240 kB/s。
64行単位7680 byteで書き、2048行RAMリングは229376 byte、約1.024秒分。書込み中にtailを解放しないのでISRによる上書きを防ぐ。
終了時に残りを排出しsyncする。SD障害時は欠損数を保存して停止し、正常完了を装わない。電源断直前のRAMは失われる。

停止後`STATUS`、または`D`で取得したCSVの末尾を見る。SDなしではSTATUSを使う。

|診断|キー|
|---|---|
|制御ISR最大時間・開始間隔|isr_duration_max_us、isr_interval_min_us/max_us|
|500 us超過|execution_over_500us、interval_over_500us（別カウント）|
|RXコールバック最大時間|rx_callback_max_us|
|RX件数・受信間隔|rx_frames/accepted/rejected/processed、rx_gap_min_us/max_us|
|FIFO滞留・欠損|rx_queue_max/pending/overflow、rx_hardware_overruns、rx_budget_hits|
|古さ・間隔異常|rx_stale、rx_abnormal_interval、rx_startup_discarded|
|送信受付結果・キュー|write_success/failure、tx_queue_max_observed|
|CANエラー|esr_observed_or、tx_error_max_observed、rx_error_max_observed、bus_off_observed|
|SD・RAM|sd_service_max_us、ring_max_rows、log_pending_rows、log_dropped、sd_write_failures|
|最初の送信失敗|first_failureの時刻・ESR/ECR・TX MB数、failure_*のRX/ログ滞留・実行時間|

RXの処理時間はユーザーコールバック部分であり、FlexCAN_T4全体のIRQ入口／出口やTX処理を含まない。microsは1 us分解能で、末尾の計測更新・ISRエピローグは計測外。必要なら外部GPIO計測と併用する。
SD service時間は割込みで中断された時間も含む壁時計時間。ロギングOFFと同条件で比較する。
CSVのprevious_isr_duration_usは前回値。全体の最大値と失敗周期の値はフッタを参照する。
RX待ちが残ったまま停止した場合はrx_queue_pendingへ残し、停止後に観測器を進めない。

## PC検証と実機検証の区別

PCで既存Controllerのスナップショットと修正版を比較し、120000制御周期、119364フィードバック更新で速度LPF・摩擦・RFOB状態・推定値・トルク指令の完全一致を確認した（最大差0）。
受信なし、複数到着、ON/OFF、ゲイン変更、同一時刻、micros折り返し、FIFO満杯・順序、最大処理数、形式不一致、NaNラッチを確認した。
これは合成入力の計算テストで、実時間60秒の試験ではない。ホストのCAN/ArduinoスタブはAPIと時刻を模擬し、割込み・バス・SDの実ハードウェア挙動を検証しない。
Pythonは29列対応、CRC破損、連番不一致、短いレコード、正常終了、終了フッタ欠落、正常先頭の救出を検証する。

```powershell
python .\tests\run_verify.py --compiler <zig.exeまたはg++のパス>
.\build_verify.ps1             # SD On/Off両方、Teensy 4.1、uploadなし
```

Teensy 4.1向けのビルド結果とメモリ使用量はworkspaceの`build/trans_ctrl_build_sd_on.txt`、`build/trans_ctrl_build_sd_off.txt`へ保存する。
最終版はSD ON/OFFともコンパイル・リンク成功、エラー0。RAM2使用量はON=241792 byte、OFF=12416 byte。ホストC++比較とPython 3テストも成功した。
FlexCAN_T4同梱実装由来の未使用値・strncpy警告は残る（ライブラリ自体は変更していない）。

**RX ISRと制御ISRの実測時間、500 us以内、長時間の送信安定性は未確認。実機への書込み・運転は実施していない。**

実機では同じ配線・電源・ゲイン・操作を用い、SD ONとOFFをそれぞれ30～60秒以上実行する。
まずOFF状態の観測・通信、続いてON/OFF切替を含む条件を比較し、X→SAVED→STATUSで診断を取得する。SDなしはX→STATUS。
旧版とのRX時間比較が必要な場合は旧版にも同じcallback入口／出口の計測を置き、同じ計測範囲で採取する。
execution_over_500us=0、isr_duration_max_us<500、RX欠損・log_dropped・CAN失敗0を確認する。interval_over_500usは1 us程度の位相揺れでも増えるため最小／最大間隔と一緒に判断する。
失敗が残る場合はfirst_failureのTX詰まり・ESR/ECRと、RX／ログ滞留・周期超過を照合する。負荷原因、バスACK／再送、SD長時間停止を分けて評価する。
RAM約1秒以上のSD停止、SDライブラリ内部の割込み禁止、ライブラリエラー履歴の取りこぼし、CAN送信受付後の配送失敗は残る実機確認項目。

## 変更ファイル

- `can_protocol.h`: 時刻付き受信FIFO、軽量callback、制御側デコード。
- `trans_ctrl_test1.ino`: 受信処理上限、起動応答の境界処理、RX欠損停止、失敗時滞留採取、SD無効時の記録除外。
- `control_config.h`: SDコンパイル設定、RX容量・処理上限・診断閾値、64行バッチ。
- `can_diagnostics.h`: RX・周期最小値・SD欠損・失敗時状態。
- `save_data.h`: バイナリバッチ、CRC・連番・終了検査、Dの停止後CSV変換、SDなし実装。
- `convert_trans_log.py`: カード上BIN→既存CSV、破損検出・明示的救出。
- `build_verify.ps1`: SD ON/OFFのTeensyビルド。
- `tests/`: 変更前アルゴリズムのスナップショット、C++比較・FIFO検証、ホストスタブ、Python形式テスト、実行スクリプト。
- `README_ja.md`、`CAN_DIAGNOSTICS_ja.md`、本報告、`trans_ctrl_test1_all_sources.txt`: 操作・診断・形式・ソース一覧の更新。

`download_trans_ctrl_test1.ps1`と`control_algorithm.h`は今回の変更前の内容を維持した。
