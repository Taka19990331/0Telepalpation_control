# AK80-8 KV60 摩擦同定（fric_identification）

既存 `sys_identification/chirp_simplified_test1` を変更せず独立して実装した。
**現在の設定は FRICTION_NO_MOTOR=0（実機モード）。書込み後、シリアルのsで掃引を開始する。実機へのアップロードや駆動は未実施。**
ドライランに戻す場合は1に変更する。PC用run_dry_run.pyはソース設定によらず1を明示してビルドする。
目的は出力軸の指令トルクと速度、内部トルク推定値、整定時間の記録であり、摩擦モデルのフィッティングは含めない。

## ファイル構成

| ファイル | 役割 |
|---|---|
| fric_identification.ino | Teensy 4.1、500 µs IntervalTimer、CAN、SD、シリアル開始/中止 |
| friction_config.h | 実験・通信・安全設定、実機/ドライラン切替 |
| friction_core.h | ハードウェア非依存の状態機械、回帰・静止検出、停止処理、ログ構造 |
| motor_protocol.h | 既存MITパケット生成・応答デコードを複製（includeのみ変更） |
| config_metadata.h | コンパイル時の設定をJSONとして保存 |
| convert_friction.py | バイナリ→全時系列CSV、ステップCSV、メタデータJSON |
| plot_friction.py | 5種類のグラフ（PNG） |
| run_dry_run.py | PC用C++ビルド、同じ制御コードのシミュレーション、変換テスト |
| download_friction.ps1 | PowerShellからTeensyのUSBシリアル経由でSD BINを受信 |
| tests/core_test.cpp | 回帰・状態遷移・異常停止・MIT符号化・全掃引の試験 |
| tests/test_conversion.py | CSV変換、有効性取消、破損ログ検査 |

## 既存コードからの流用と変更点

MITの位置/速度ゼロ、Kp=Kd=0のパケット、AK80標準CAN IDによる応答識別、1 Mbps、
CAN2/ID2（IS_80A=trueならCAN1/ID1）、速度符号化±37.5 rad/s、トルク符号化±32 Nmを流用。
AK60用分岐はコピーしたヘッダ内に残るがIS_60=false固定で、このプログラムでは選択できない。
符号化範囲は実験上限±1 Nmとは別。12-bitトルクの分解能は約0.01563 Nmで、
要求0 Nmも厳密なゼロにはならない。要求値と実際の符号化量子化を区別すること。

CANとタイマーの割込み優先度192、TX用単一メールボックスMB8、キュー投入前のbusy検査、
CAN error-passive/bus-off監視、ポーリングによるTX取消とゼロ→FD終了を流用した。
CANのACKはモータの実行確認・停止確認とは別に記録する。
4096行DMAMEMリング、128行バッチ、SDIO FIFO、事前領域確保、header/rows/footer形式も維持。
新しい行は80 bytesで、旧SI1形式とは別のFRIC001形式。

SD write/syncはすべて割込み外。書込み中もCAN/タイマー割込みが動作する。
SD busy中はバッチ書込みを延期する。SdFatのwrite自体は同期APIであり、完全な非同期DMAライターではない。
カードが長時間詰まればリング満杯で実験を中断する。書込み時間の絶対上限は未保証であり、
実機の最悪SD遅延とISR時間はログ・終了時表示で確認する。

## 設定と状態遷移

設定は `friction_config.h` 冒頭に集約。制御2 kHz、±1 Nm。abs(speed)<10 rad/sではMITトルク指令1 LSB刻み（64/4095 Nm ≈ 0.01563 Nm）、abs(speed)>=10 rad/sでは4 LSB刻み。速度上限25 rad/s。
初回のゼロ要求とMIT enable後、新鮮で正常な応答を受けて掃引を開始する。
`REQUIRE_INITIAL_STATIONARY` は初期静止確認のON/OFF。既定値 `false` では初期静止確認を省略し、最初の有効な応答後に掃引を開始する。`true` にすると従来どおり初期静止を確認する。いずれの設定でも初期応答の鮮度・モータエラー・過速度判定は有効で、正負切替と終了時の停止確認も省略しない。
開始時点でabs(speed)>=25 rad/sなら掃引を開始せず停止処理へ進む。

```
STARTUP → (初期静止確認ONなら停止確認) → Phase1: 0 → +1
                        Phase2: +1 Nmから減少 → 0
                        0 Nm停止判定（過速度時は1秒ローリング窓）
                        Phase3: 0 → -1
                        Phase4: -1 Nmから増加 → 0
                        最終停止確認 → ゼロ/FD → FINISHED
```

ステップ数は速度履歴に応じて変わる。±1の重複を省略し、逆方向ブレイクアウェイの基準としてPhase3の0を残す。端点±1 NmはMITトルクカウントへ量子化され、刻み幅を切り替えた場合も端点をclampして到達する。
Phase0は開始前/正負移行の停止確認。停止待機はstateで区別し、摩擦曲線から除外する。

各ステップ：SETTLING → ACQUIRING（0.5秒）またはSTATIONARY（0.1秒）→次ステップ。
2秒までに整定しなければNOT_SETTLED、取得中の整定逸脱はUNSTABLEとして当該区間全体を無効にする。
安全異常の場合は次ステップへ進まない。総実行上限は900秒。

## 整定・静止アルゴリズム

最新0.2秒の**異なるCAN受信シーケンス**だけをリングに保持し、実受信時刻を説明変数、
速度を目的変数として最小二乗直線の傾きを推定する。
和(t, v, t², tv, v²)を逐次更新し、全400点を毎割込みで走査しない。
ステップ開始より前の応答は除外。同じ応答を2 kHzで繰り返しても判定窓の点数は増えない。
窓の有効スパンは通信期限20 msを許容した180 ms以上、最低3点。

最短待ち0.1秒に加え、abs(回帰加速度)<=0.3 rad/s²、速度標準偏差<=0.08 rad/sが
0.1秒連続した場合に動的整定。判定窓が必要なので、通常0.1秒ちょうどで整定とはならない。
取得中も同じ条件を監視。閾値は実測で再評価が必要。

静止判定は別の1.0秒確認期間でabs(speed)<=0.04 rad/sかつ角度最大最小差<=0.003 rad。
確認後に0.1秒取得する。低速ではこの静止確認を優先し、早期に動的整定へ分類しない。
速度逸脱や角度範囲逸脱で確認期間を再開するため、遅延ブレイクアウェイは取得中も無効化される。
位置は受信したraw角度を保存する。角度折り返し・不連続は静止確認をリセットし、
速度回帰には影響させない。未知の25 rad/2πなどを仮定して多回転角度を捏造しない。

## 安全停止

過速度検出時は直ちに指令トルクを0 Nmにし、同じ制御周期から停止判定用履歴を蓄積する。
2 kHzの2,001サンプル（両端を含む実時間幅1.000秒）で、位置変動幅<0.003 radかつ
窓内平均速度の絶対値<0.03 rad/sを判定する。位置は±12.5 rad間の25 rad折り返しを
補正してから変動幅を求める。窓が満たされるまでは停止確認を成立させず、0 Nmを維持する。
停止確認のタイムアウトは過速度検出から8秒。タイムアウト時も0 Nmのまま終了し、
停止未確認として再開をロックする。停止窓の極値は単調キューで更新し、毎周期の全走査を避ける。

通常終了・正負移行・ユーザー中止・SD異常・リング満杯でも能動制動は行わず、指令0 Nmで
既存の静止確認を行う。掃引中に過速度を検出した場合は、停止確認後に反対方向の掃引を一度だけ行う。
反対方向の掃引中にも過速度になった場合は、同じ停止判定を行って終了し、三度目の掃引は開始しない。
通信断20 ms、CAN/送信失敗、モータエラー、非有限フィードバック、制御deadline異常では
信頼できる停止確認を続けられないため、ゼロ/FDを期限100 ms以内でbest effort送信する。
未確認の回転停止を成功とは報告しない。最初の中断理由を保持し、stop_confirmed=falseや
exit_acknowledged=falseを別に残して再開をロックする。

通信断20 ms、CAN/送信失敗、モータエラー、非有限フィードバック、制御deadline異常では
停止判定を継続できないため、ゼロ/FDを期限100 ms以内でbest effort送信する。
未確認の回転停止を成功とは報告しない。最初の中断理由を保持し、stop_confirmed=falseや
exit_acknowledged=falseを別に残して再開をロックする。
正常終了でも静止確認が必須。実機のFD ACKが得られない場合もロック。
通信・電源・ドライバ故障時の停止をソフトだけで保証するものではない。

**未確認**：実機ファームウェアとMIT範囲の一致、出力軸トルク換算、内部watchdog、
過速度/過電流/回生電圧保護、停止トルク・減速度・停止時間の妥当性。
既存ソースにはマニュアルv1.0.18 p.63の記載があるが、今回現物の保護機能は検証していない。
`BRAKE_VERIFIED=false`、`VERIFIED_MOTOR_TIMEOUT_MS=0`を維持。
初回実験を可能にするため、これらの確認値によるビルド制限は撤去した。
false/0は未確認というメタデータとして残す。架空の確認値を入力する必要はない。
速度上限、通信監視、停止失敗時の再開ロックは引き続き有効。

## ログ、CSV、メタデータ

SDにFR0000.BINから未使用名で保存。実験終了後、`d`でBIN、`c`でCSVをUSBへ転送できる。
シリアル`s`で開始、`x`で中止、`d`で最後に保存したBIN、`c`でCSVを取得する。
`d`/`c`は記録中・モータ動作中には拒否する。静止未確認などの異常終了ログも、フッターまで保存できていれば取得対象になる。起動後でファイル名を覚えていない場合は、SDのFR9999.BINから検索して最新ログを送る。
転送プロトコルは1行の`FRICTION_BIN,filename,size`、指定サイズの生BIN、終端の`FRICTION_END,filename,size`。
CSV転送は`FRICTION_CSV,filename,rows`の後にコメント行と時系列CSVを送り、`FRICTION_CSV_END,filename,rows`で終わる。
Arduino IDEのシリアルモニタを閉じてからPowerShellで以下を実行する。例ではCOM5を使う。出力先dataに同名ファイルがあれば上書きせずエラー。

```powershell
.\fric_identification\download_friction.ps1 -Port COM5
python .\fric_identification\convert_friction.py .\data\FR0001.BIN
python .\fric_identification\plot_friction.py .\data\FR0001.csv
# BINを取得できない場合は、チャンク受信でCSVを取得（Python不要）
.\fric_identification\download_friction_csv.ps1 -Port COM5
```

CSV取得スクリプトは受信を最大64 KiB単位で行い、生のシリアル受信データを`data\friction_*_raw.txt`に保存する。終端マーカー、行数、列数、連番を確認できた場合だけCSVを確定する。通信が中断した場合も生ログと途中CSVを残す。Pythonのインストールは不要。

直接取得したCSVは時系列データです。BIN変換時と同じ列構成で、冒頭コメントに実験設定と終了理由・停止確認結果を含みます。ステップ集計CSVやグラフが必要な場合はBIN取得と`convert_friction.py`を使います。

ドライランでもTeensy上ではSDが必要。

ファイル構造：little-endian Header20 bytes + UTF-8設定JSON + Sample80 bytes×行数 + Footer24 bytes。
Headerはmagic FRIC001\0、rowBytes、periodUs、configBytes。
FooterはENDFR001、rows、最初の中断理由、静止確認、FD送信ACK。
設定JSONには全設定定数を保存し、結果・状態/理由コード表を変換後のmetadata.jsonにまとめる。
最終化に失敗したファイルは通常の変換で拒否する。書込み失敗時の既知の完全行prefixのみbest effortで最終化する。

全時系列CSV：nominal_us、index、actual_us、phase、step_index、settling_status、reason、event、
target_torque_Nm、position_deg、speed_rad_s、feedback_torque_estimate_Nm、estimated_acceleration、
speed_std、feedback_rx_us、feedback_age_us、feedback_sequence、motor_error、can_tx_accepted、ISR_us、period_us。
nominal_us=index×500、actual_us/feedback_rx_usは実験開始を原点とするmicros値。
CAN受信時刻はコールバック時刻であり、物理的なバス到着時刻とは少し異なる。
event bit0=step開始、bit1=step終了、bit2=終了処理。stateの変化も各行に残る。
送信成功はメールボックス受付であり、応答との同期確認ではない。ドライランでは受付0。
回帰未確定時はNaN。motor_errorはドライバのエラーバイトそのもの。

ステップCSV：phase、step_index、要求トルク、平均内部推定トルク、平均速度、速度標準偏差、
平均回帰加速度、整定までの実時間、取得時間、有効点数、STEADY/NONSTATIONARY/STATIONARY、
ステップ中の最大絶対速度、終了理由。未整定の整定時間は空欄。
集計では取得区間内の新規応答だけを数える。取得が途中で無効になった場合は有効点数0、平均値空欄。
定常曲線にはSTEADYだけを使い、静止点は静止摩擦解析用にステップCSVへ別分類する。

**速度とトルクは同じ行の組で描画するが、その行の新しい指令への応答という意味ではない。**
行のフィードバックは送信前の最新スナップショット。応答遅延はrx時刻/age/sequenceで追跡する。
内部トルク推定を実トルク計測とみなさない。取得区間の指令は一定であり、独立した時刻平均の組合せは行わない。

## 実行手順

プロジェクトルートから（python/g++/arduino-cliは各環境の実行パスに置換）：

```powershell
python fric_identification/run_dry_run.py --compiler g++ --plot
python -m unittest discover -s fric_identification/tests -v
arduino-cli compile --fqbn teensy:avr:teensy41 fric_identification
python fric_identification/convert_friction.py path/to/FR0000.BIN
python fric_identification/plot_friction.py path/to/FR0000.csv
```

ホストシミュレーションは実機と同一Engineを使い、仮の慣性0.01、動摩擦0.12、静止摩擦0.18、
粘性0.2の単純モデル。AK80の物性測定値ではない。ポート接続・モータ駆動は一切しない。
変換はPython標準ライブラリだけ、描画にはpandas/matplotlib、ホスト試験にはC++17コンパイラが必要。
Zigを使う場合は`--compiler path/to/zig.exe`でも可。

## 検証結果（2026-10-20）

- 初版のドライラン設定でTeensy 4.1/core1.62.0/SdFat2.1.2/FlexCAN_T4ビルド成功。
  FLASH code 73,588 bytes、RAM1 variables 33,248 bytes、RAM2 variables 340,096 bytes。
  外部FlexCAN_T4内の既存unused-value警告のみ。
- ホストC++試験成功：回帰傾き、重複/ステップ前応答除外、角度折返し、静止待ち、
  未整定・再加速、速度超過、初期/走行中通信断、モータ/非有限値/トルク範囲異常、
  SD/リング/ユーザー停止、制動失敗・過大減速度、MITゲイン0/量子化範囲。
- 変更前の0.02 Nm刻み設定では202ステップ、310,598行（約155.3秒）の全掃引を完了、停止確認成功。
- Python変換テスト4件成功。合成BIN→CSV/steps/metadata→5段PNGを生成し目視確認。
- 初版に存在した確認値によるビルド制限は、初回実験に対応する変更で撤去。
- 制限撤去後の実機設定でもビルド成功（FLASH code 76,404 bytes）。
  変更前の0.02 Nm刻み設定では、PCドライランで202ステップ・310,598行と変換テスト4件を再確認。
- 実機での摩擦試験・USB転送は未実施。2 kHzの実測ISR最悪時間、CAN応答頻度、SD最悪遅延、停止性能は未測定。USB転送機能追加後は実機スケッチを再ビルド済み。

実機前にCAN2/ID2・配線、ファームウェアのMIT範囲、上記未確認の保護機能、無負荷固定、
外部停止手段、SD空き容量（事前予約は約144 MB）、制動の回生と機械条件を確認すること。
書込み後の表示がFRICTION: HARDWAREであることを確認し、sで開始、xで停止要求。
内部watchdogの動作は未確認のため、通信不能時の停止には外部の電源遮断手段を用意する。
