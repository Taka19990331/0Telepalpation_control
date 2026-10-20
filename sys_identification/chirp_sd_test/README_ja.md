# Teensy 4.1：既存ゼロへの低速復帰・整定＋チャープ＋SD

## ハードウェアの選択

`chirp_config.h`冒頭の1行を変更して再ビルドする。

```cpp
constexpr char MOTOR_SELECT[] = "80B";
```

| MOTOR_SELECT | 接続 | ID | 形式 | 停止 |
| --- | --- | ---: | --- | --- |
| `"80A"` | CAN1：TX22 / RX23 | 1 | 標準MIT、AK80 | ゼロ送信→FD |
| `"80B"` | CAN2：TX1 / RX0 | 2 | 標準MIT、AK80 | ゼロ送信→FD |
| `"60"` | CAN3：TX31 / RX30 | 3 | 拡張MIT、AK60 | ゼロ送信を継続 |

既定の選択は80B。各CANピンは外部CANトランシーバへ接続する。ID・CAN・IRQ・レジスタ・RXフィルタ・パッキング・受信換算・起動表示を選択から一括導出する。未定義文字列はコンパイルエラー。モータ側IDを自動変更する機能ではなく、選択先の実モータが表のIDで設定済みである必要がある。

AK80は従来どおり±37.5 rad/sを維持する（参考元AK80_timer_testの±45 rad/sには変更しない）。AK60は参考元の拡張MITを使い、指令範囲は位置±12.56 rad、速度±60 rad/s、トルク±12 N·m。内部kp/kdは両方0。送信IDは0x803、応答IDは0x2903。応答は位置0.1 deg/bit、速度10 ERPM/bitを14極対・減速比6で出力軸rad/sへ換算。電流0.01 A/bit×公称0.5994 N·m/Aでトルクを推定する。既存プロジェクトからの換算であり、AK60の実機仕様を新たに確認したという意味ではない。

**AK60へAK80のFC/FDは送らない。** 停止後・待機中・SD/USB処理中もタイマーからゼロトルクを送る。これはモード終了やブレーキ保持ではない。StopFlagsはAK60正常停止で3（ゼロ受付＋TX完了）、AK80で15、非駆動で16。AK60の位置フィードバックは符号付き16-bitの0.1 deg単位で読む。電流生値は符号付き16-bitのビット列を0～65535でfeedback_rawへ保存し、換算時に符号を復元する。

## 現在の進行（CHIRPSD6）

`chirp_config.h`に設定を集約。既存の`CHIRP_NO_MOTOR=0`設定を維持している。
`CHIRP_NO_MOTOR=1`のビルドはCANを初期化・送信しない。模擬位置は指令に依存しないため、閉ループ整定性能を示さない。
本変更で実機への書き込み・開始・駆動は行っていない。

`s`後の流れ：

1. SD事前確保、有効な新鮮な応答とMITモードを確認。AK80は0→FC→0、AK60は既存の拡張MIT。
2. ZEROING：既存ドライバゼロ0 radとの差（多回転、折り返しなし）→位置比例で速度目標→速度・加速度制限→速度PI→トルク・変化率制限→既存sendTorque。
3. 位置±2 deg・速度±0.1 rad/sを連続1秒満たすまで復帰制御を継続。整定判定中も積分を維持。180秒で整定しなければstatus14。
4. 移行：直前のPI最終指令を固定して、2秒かけて現在の実験PDへ線形補間。積分は更新しない。±1 Nm・1 Nm/s制限を維持。期限時点でPDに一致できなければstatus16で停止。
5. PRE_HOLD：PI状態を外し、既存PDのみ。最低2秒保持し、位置・速度の連続1秒整定を再確認してSWEEPへ。15秒で満たせなければstatus15。復帰への再試行なし。
6. SWEEP：従来の1～100 Hz、40秒、振幅1～2.5 Nm、指数1の対数チャープ＋既存PD。
7. POST_HOLD：チャープのみ0にして既存PDを1秒継続。最後に既存の停止経路（AK80は0→FD、AK60は0指令継続）。

原点再設定0xFEも電流上限変更コマンドも送らない。MIT内部Kp/Kdは常に0。
`x`は共通停止へ移る。USB入力の解釈はloop内なので、SD writeが戻るまで遅れる可能性は従来どおり。

## ZEROINGの初期値と調整

| 設定 | 初期値 | 単位・意図 |
| --- | ---: | --- |
| HOMING_MAX_VELOCITY | 5 deg/s相当 | 内部rad/s。実測速度そのものの保証ではない |
| HOMING_MAX_ACCELERATION | 5 deg/s²相当 | 目標速度をゼロからランプ |
| HOMING_TORQUE_LIMIT_NM | 1.0 | Nm、復帰・移行の指令制限 |
| HOMING_POSITION_K | 1.0 | 1/s。誤差5 deg以上で速度上限、近傍で比例減速 |
| HOMING_VELOCITY_KP | 2.0 | Nm/(rad/s)。速度誤差5 deg/sで約0.175 Nm |
| HOMING_VELOCITY_KI | 1.0 | Nm/rad。速度誤差5 deg/sで約0.0873 Nm/sの積分増加 |
| HOMING_TORQUE_SLEW | 1.0 | Nm/s。急なトルク立ち上がりを抑制 |
| ZEROING_TIMEOUT_S | 180 | 秒 |
| HANDOVER_DURATION_S | 2 | 秒 |
| PRE_HOLD_TIMEOUT_S | 15 | 秒、最低保持時間を含む |

PIゲインは小さい初期比例トルクから、摩擦を超えるトルクを時間をかけて蓄えるための試行値。5 deg/s・5 deg/s²とともに実機での安定性・整定を保証しない。
積分状態はNm。`Ki*(目標速度−実測速度)*実測dt`を加算する。絶対上限とその周期で到達可能なトルク変化率の両方に対して条件付き積分し、飽和を強める増分を止め、戻す増分を許容する。
新規試行時に速度目標・PI・前回指令・制限フラグ・整定追跡・区間カウンタをリセットする。
1 Nmで動かなければ上限を上げず期限停止する。

想定初期位置±360 degでは、理想的な加減速だけでも約`360/5 + 5/5 = 73秒`が必要。さらに近傍比例減速・整定・負荷による遅れがある。符号化範囲端近くの約713 degなら理想でも約144秒。180秒は初期設定であり、十分性を保証しない。`HOMING_EXPECTED_INITIAL_DEG=360`は計画用で位置異常境界を変えない。移行2秒＋PD単独整定待ち最大15秒は別途確保する。

実機では、摩擦・負荷に対するPI、速度応答、近傍の行き過ぎ、移行時の変位、PD単独で±2 degに保持できるかを調整・確認する。PDゲインや実験上限を自動で強める処理はない。

## 維持した実験条件と安全監視

実験PDは`0.00834*(0−position_deg) − 0.1*speed_rad_s` [Nm]。
チャープ式・振幅・周波数・実験上限±10 Nmは変更していない。
PRE_HOLD/SWEEP/POST_HOLDでは制限前の合計が実験上限を超えるとstatus9で0指令へ停止する。
ZEROING/移行の±1 Nm飽和は通常動作で、飽和だけではstatus9にしない。
CAN符号化は両モードともその指令上限の内側へ丸める。12-bit量子化があるため0指令にも誤差がある。

既存のフィードバック期限20 ms、モータエラー、CANエラー、位置境界0.05 rad・不連続ジャンプ、制御周期超過、SD異常、リング満杯、ユーザー停止、停止未確認時の再始動ロックを全制御段階で維持。非有限のフィードバック・制御値はstatus17で停止する。
位置の±180 deg折り返しや推測アンラップは行わない。
CANとタイマーは同優先度192。ISRにSD・Serial・待機ループを追加していない。
モータ側タイムアウト確認値0は未確認という記録であり、CAN断線やMCU停止時のモータ側停止は未検証。

## ログSD6

CHIRPSD6 / ENDSD006、リトルエンディアン。Header188 / Sample88 / Footer20 bytes。
旧SD3/4/5はPC側の従来の専用デコーダで引き続き処理する。ファームウェアの`d`はSD6のみ受け付け、旧ファイルを新形式で読まない。

| phase | 内容 | 最大行数（既定） |
| --- | --- | ---: |
| 4 | ZEROING（PI復帰・整定） | 360000 |
| 5 | PI→PD移行 | 4001（終端を含む） |
| 0 | PDのみPRE_HOLD・再整定 | 30000 |
| 1 | SWEEP | 80000 |
| 2 | POST_HOLD | 2000 |
| 3 | 停止 | 1 |

最大476002行、事前割り当て41888384 bytes（約40 MiB）。最終的に記録済み行数へ切り詰める。
ExpectedRowsはSD6では**最大確保行数**。HomingRows / TransitionRows / PreHoldRowsは実績値。
リング4096×88＝360448 bytes（2.048秒）、書き込み128×88＝11264 bytes。
区間時間・行数をuint64で合算し、uint32インデックス・最大許容周期間隔での時刻・FAT32単一ファイル容量をコンパイル時検査する。実行時にも記録上限の前で停止行を確保する。

既存18列に次の6列を追加：
`target_position_rad, target_velocity_rad_s, pi_proportional_Nm, pi_integral_Nm, raw_torque_Nm, slew_limited`。
位置実測は従来互換のposition_deg、速度実測はspeed_rad_s。ZEROING内部はrad/rad/s。
pd_NmはZEROINGでは0、移行では補間先の純PD、以後は実験PD。PIの流用はしない。
移行中のPI比例・積分欄は最終ZEROING値を保持する参考値で、実際の補間は直前の制限後PI指令から行う。移行中の速度目標欄は0で速度PIは更新しない。
raw_torque_NmはZEROINGではP+I、移行では補間値、実験ではchirp+PD。
command_Nmは絶対上限・変化率制限後にsendTorqueへ渡す要求値（CAN量子化前）。can_tx_acceptedは送信要求受付で、モータによる実行確認ではない。
torque_limitedは絶対トルク上限、slew_limitedは変化率制限の作動を示す。

停止理由は従来0～14に加えて15=PRE_HOLD_TIMEOUT、16=HANDOVER_FAILED、17=NONFINITE_CONTROL、18=RECORD_LIMIT。
14=ZEROING_TIMEOUTは位置・速度の連続整定を期限内に満たせなかったことを表す。移動不能・整定不足の詳細は最終付近の位置・速度・PI・飽和ログで判断する。

## PCでの取得・変換・表示

```powershell
python sys_identification/capture_chirp.py --port COM5 --sd --timeout 900
python sys_identification/capture_chirp.py --port COM5 --download --timeout 900
python sys_identification/convert_sd_chirp.py path/to/CH0000.BIN
python sys_identification/plot_chirp.py path/to/CH0000.csv
python -m unittest discover -s sys_identification -p "test_chirp*.py"
```

上の最初のコマンドは実験を開始するため、実機で実施する際のみ使用する。本修正の検証ではSerialポートを開いていない。
既定の取得期限は900秒に延長（実験待機に加え拡張CSVの転送時間も必要）。実際の転送速度に合わせて変更する。
BIN/Serialの形式・サイズ・段階順序・トルク・チャープ/PD式・連続整定・停止理由を検証し、CSVとconfig/validation JSONを保存する。グラフは移行段階、目標/実測速度、PI成分、制限前後のトルク、両制限フラグを表示する。
検証結果の詳細は`VERIFICATION_HOMING_20261019.md`を参照。過去のVERIFICATION_*は旧版の履歴。
