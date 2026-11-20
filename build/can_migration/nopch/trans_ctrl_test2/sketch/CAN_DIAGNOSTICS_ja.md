#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_test2\\CAN_DIAGNOSTICS_ja.md"
# CAN送信診断（Teensy 4.1 / Teensy core 1.62.0）

## 原因の切り分け

`fault_code=2` は起動、制御中、OFF指令のCAN writeが0を返した場合。
今回確認した同梱FlexCAN_T4の `write(const CAN_message_t&)` は、空きTX MBへ書くか、空きがなければ `struct2queueTx()` へ進む。
`struct2queueTx()` はソフトウェアキュー満杯時に0を返す。RX継続・motor_error=0でも、TX詰まりは否定できない。
ACK不足、物理配線・終端・トランシーバ異常、エラー再送、CAN割込み処理の遅延など、詰まりの根本原因は従来ログだけでは確定できない。

注意: この版では `struct2queueTx()` の戻り値型がboolなのに、キュー追加時は-1、ESR1 bit5時は-2を返す。どちらもtrueへ変換される。
そのためwrite_successは**APIの非ゼロ戻り回数**であり、バスへの送信完了・ACK・モータによる実行を意味しない。Bus-offが必ずfault=2になるわけでもない。
ライブラリ自体の変更や新たなBus-off停止条件は今回追加していない。

確認ソース:
`C:/Users/Miyam/AppData/Local/Arduino15/packages/teensy/hardware/avr/1.62.0/libraries/FlexCAN_T4/`
- FlexCAN_T4.h: getTXQueueCount(), error(CAN_error_t&,bool)
- FlexCAN_T4.tpp: write(), struct2queueTx(), flexcan_interrupt(), error(), enableFIFO(false)
- imxrt_flexcan.h: ESR1/ECR/MB CSマクロ
独立したライブラリのバージョン情報ファイルはなく、core 1.62.0同梱版として特定。

## 記録

既存CSV列を先頭に維持し、以下を末尾追加:
- tx_queue_count: 制御周期でのwrite後キュー実測使用数（容量16）
- isr_interval_us: 今回の制御ISR開始間隔
- previous_isr_duration_us: 前回の制御ISR実行時間
- can_snapshot_time_us / can_esr1 / can_ecr: loopで約100 ms間隔の生レジスタ採取。時刻を見て鮮度を判断すること。初回採取前は0。
- can_write_success / can_write_failure: 起動・制御・停止を含むAPI結果累積数

ESR1とECRはFlexCAN_T4のレジスタマクロで読み取りのみ。ECR下位8bit=TXエラー数、次の8bit=RXエラー数。
ESR1: bit13=ACK、bit15/14=BIT1/BIT0、bit12=CRC、bit11=FORM、bit10=STUFF、bit9/8=TX/RX warning。
FLT_CONF=(ESR1>>4)&3: 0=active、1=passive、2/3=Bus-off。bit2はBus-off割込みフラグ。
ESR1の一部エラー要因はライブラリISRでクリアされるため、瞬間値に加え `error(e,false)` の履歴をloopで1回最大4件回収し、ORと観測最大値へ集約する。
履歴はライブラリ内16件であり、SD待機中のあふれや短い変化の取り逃しはあり得る。bus_off_observed=0はBus-off不発の保証ではない。
error_history_recordsは読み出した**状態変化履歴数**であり、バスエラー回数ではない。

CSV末尾の `#diag` と `#diag_bus` を確認する:
- write_success/failure、tx_queue_max_observed、rx_gap_max_us
- isr_interval_max_us / isr_duration_max_us、interval_over_500us / execution_over_500us
- sd_service_max_us、ring_max_rows
- esr_observed_or、tx_error_max_observed、rx_error_max_observed、bus_off_observed
- first_failure: 最初の失敗時刻、キュー、ESR1、ECR、空きTX MB数、送信待ちTX MB数
- failure_torque_Nm / failure_feedback_age_us / failure_interval_us
- failure_previous_isr_us: 直前周期の完了済み処理時間
- failure_entry_to_write_us: 失敗周期の入口からwrite直前まで
- failure_isr_duration_us: 失敗周期の停止処理も含む処理時間（ISR外失敗では0）
- failure_kind: 0=トルク、252=モード開始、253=モード退出
- failure_count_at_event、stop_reason / stop_time_us
- stop_zero_write_accepted / stop_disable_write_accepted: 停止2指令の各API結果

診断時刻はSでの診断リセットからの経過us。従来timestamp_usは最初の制御ISR開始からであり、起動指令分のずれがある。
RX間隔は受信コールバック実行時刻の差で、バス上のハードウェア受信時刻そのものではない。
最大値は実際に観測した最大値。メールボックスCSは失敗・停止時のみMB8..15を読む。RX MBを読み取りロックしない。

## 停止と保存

従来の停止原因とactive解除を維持し、ゼロトルク・モード退出を各1回ずつ非待機で試行。無制限再送やISRでの待機は行わない。
専用RAMの最初の失敗記録は後続の停止送信失敗で上書きしない。通常行は既存リングを使い、停止後はリングを最後まで排出して診断フッタを書き、syncして閉じる。
リング満杯で故障行を追加できなくても専用RAMからフッタを出力できる。記録/SD障害時も停止原因はstop_reasonとして保持。
SDが故障した場合や電源断では永続保存は保証できない。再起動・次のSの前に停止状態でSTATUSを送るとRAMの診断を取得できる。

**API成功でも実モータ停止は保証できない。** キュー内の古いトルク指令・ハードウェア送信待ちも残り得て、停止指令が配送されない可能性がある。
ソフトウェア停止だけに依存せず、実験設備の独立した非常停止・駆動電源遮断を使用すること。

## 制御周期への影響と検証

制御周期500 us、直接受信コールバック、CAN/タイマ優先度192、制御アルゴリズム、制御定数は維持。受信処理は時刻付きFIFOへ分離した。
ISR追加は定数時間のmicros、整数比較・累積、キュー数参照、ログへの整数コピー。失敗時のみ8個のTX MBとエラーレジスタを採取する。
文字列解釈を伴うerror()とSD I/Oはloop側。CSV整形は停止後のDのみ。通常レジスタ採取は約10 Hzで短時間の割込み禁止区間内。
CSVは既存29列を維持。リングSampleは112 byte、2048件。実測負荷はisr_*、sd_service_max_us、ring_max_rowsで確認する。
microsの計測分解能は1 us、処理時間は計測器自身の末尾処理等を完全には含まない。実機での500 us以内の保証はコンパイル検証だけではできない。

ビルド: arduino-cli compile --config-file build/diagnostics-arduino-cli.yaml --fqbn teensy:avr:teensy41 --build-path build/trans_ctrl_test1 controller_implementation/trans_ctrl_test1
Windowsサンドボックス内ではctagsがDLL再配置エラーになるため、通常環境でコンパイルする。書き込み・モータ作動は実施しない。

DのCSV転送プロトコルと29列は既存PowerShellに互換。TCBIN2以外の旧BINは読み込まない。

