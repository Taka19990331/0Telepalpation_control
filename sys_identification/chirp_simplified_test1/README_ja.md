# chirp_simplified_test1：初期位置平均を基準にした20秒チャープ

開くスケッチ：`chirp_simplified_test1.ino`。旧`chirp_sd_test`は変更せず残している。

## 動作

1. `s`でSD記録と既存のモータ通信開始処理を行う。
2. 有効な新鮮なフィードバックを確認した時点から**2秒間、トルク要求0 Nm**。この間はP/D/PIをすべて無効にする。位置を能動保持する処理ではない。
3. この2秒の区間内に新しく受信した有効CAN応答の位置radを各1回加算し、算術平均を計算。その平均を固定のPD基準位置とする。同一受信値を2 kHzで重複加算しない。
4. 平均位置を基準にした既存PD＋対数チャープを**20秒**実行。ドライバゼロへの復帰・整定待ち・PI・移行は行わない。
5. 加振終了後はPDのみを従来どおり1秒続け、既存停止処理へ進む。開始待ちを除く所要時間は約23秒。

タイマー開始時刻のずれを考慮し、平均区間は4000制御周期以上、かつ実時間2秒以上とする。通常4000行、必要なら最大4周期の余裕内で延長する。加振開始後は40000周期（20秒）で固定。
2秒経過後の位置応答は平均に加えない。平均はradの多回転位置の算術平均で、角度折り返しや原点再設定はしない。

## 設定

`chirp_config.h`で変更する。

| 項目 | 既定値 |
| --- | --- |
| MOTOR_SELECT | 80B：CAN2 / ID2 / TX1・RX0 |
| サンプリング | 2000 Hz（500 µs） |
| 初期ゼロトルク平均 | 2秒：PRE_HOLD_DURATION_S |
| 加振時間 | 20秒：SWEEP_DURATION_S |
| 周波数 | 1→100 Hz |
| 振幅 | 1→2.5 Nm、指数1 |
| P | 0.00834 Nm/deg |
| D | 0.1 Nm/(rad/s) |
| 実験トルク上限 | ±10 Nm、超過で停止 |
| 後保持 | 1秒：POST_HOLD_DURATION_S |

`tau_PD = KP_PD*(mean_position_deg - measured_position_deg) - KD_PD*measured_velocity_rad_s`。
平均区間の`chirp_Nm / pd_Nm / command_Nm`はすべて0。加振中はPDとチャープを合計して既存sendTorqueで送信する。
ドライバ内部MIT Kp/Kdは0。電流上限変更・0xFE原点再設定はしない。0 Nmは送信要求値であり、既存12-bit CAN符号化による微小な量子化誤差は残る。

既存のCAN受信期限20 ms、CAN/モータエラー、位置境界・不連続ジャンプ、非有限値、周期超過、リング/SD異常、ユーザー停止を維持。
AK80は0→FDの停止処理、AK60はゼロトルク継続の既存仕様。ISRにSD書込みや待機を追加していない。

## ログと取得

専用形式`CHIRPSI1 / ENDSI001`、SDファイル`SI0000.BIN`等。旧CHIRPSD形式とは識別子を分けている。
Header144 / Sample64 / Footer20 bytes。既定の最大予約46005行、2944484 bytes。
通常の正常完了は46001行（初期平均4000＋加振40000＋後保持2000＋停止1）。
ヘッダ`Hold_deg`に確定した平均位置、`ReferenceSamples`に平均に使った受信数、`ReferenceMin_rad / ReferenceMax_rad`に観測範囲、`AverageRows`に平均中の記録行数を保存する。途中中止の場合はその時点までの平均を診断用に保存する。
ExpectedRowsは最大予約行数、HoldPolicy=2は初期位置平均方式を示す。

| phase | 内容 |
| --- | --- |
| 0 | トルク0・初期位置平均 |
| 1 | 平均位置基準のPD＋チャープ |
| 2 | PDのみの後保持 |
| 3 | 停止 |

停止理由0～13は旧版と同じ。14=REFERENCE_INVALID、15=NONFINITE_CONTROL、16=RECORD_LIMIT。

従来のツールにSI1対応を追加済み：

```powershell
python sys_identification/capture_chirp.py --port COM5 --sd
python sys_identification/capture_chirp.py --port COM5 --download
python sys_identification/convert_sd_chirp.py path/to/SI0000.BIN
python sys_identification/plot_chirp.py path/to/SI0000.csv
```

`--sd`は実験を開始する。実機操作はユーザーが行う。Serial Monitorで`s`開始、`x`中止、`d`保存済みデータ取得も可能。

### CSV先頭の実験条件

SI1のCSVは、PC側で取得完了後に `# ` で始まるコメントを先頭に付ける。
チャープ式、周波数に対する振幅調整式、変数名付き設定値（例：`SWEEP_DURATION_S (T)`）、
PD式と単位、実験で求めた基準位置、最初のSWEEP行の実測開始時刻、全CONFIG値、検証結果を含む。
掃引前の中止などで開始行がない場合、開始時刻は `unavailable` とする。
チャープ内部の時間はサンプル数から計算した公称時間であり、`actual_us`の実測時間とは区別する。
途中中止時の平均位置は診断値で、`REFERENCE_VALID_FOR_PD`がFalseならPD基準として使用されていない。

マイコンの制御・SD記録・シリアル送信処理は変更していない。既存の終了後ダウンロードから
PC上でCSVを生成するため、計測中のマイコンに追加の処理や通信は発生しない。
SDカードを取り外す必要はない。従来の `.config.json` / 検証JSONも引き続き保存する。

`plot_chirp.py`はコメント付きCSVと従来のCSVの両方に対応する。
他の解析では `#` 行を除外してから列名を読み込む（例：pandasの `read_csv(path, comment='#')`）。
NumPyの `genfromtxt(..., names=True)` では、先に `#` 行を除外する。

## 非駆動検証

実機への書き込み・Serial接続・駆動は行っていない。
`python -m unittest discover -s sys_identification -p "test_chirp*.py"`：既存を含む22テスト成功。
新規テストで、実装C++ヘッダの新規応答のみの平均・重複除外・正負多回転平均・試行リセット、2秒の指令ゼロ、平均位置基準のPD、20秒のチャープ、SI1 BIN/Serial変換、破損/不適切な平均/初期区間の非ゼロ指令の拒否、中止ログを確認。
合成ログをPNG/PDFへ描画し、平均区間・基準位置・加振開始/終了の表示を確認した。実モータの応答や閉ループ性能を検証したものではない。

Teensy 4.1 / core1.62.0 / SdFat2.1.2 / FlexCAN_T4でビルドする。Teensyはbuild.extra_flagsを使わないため、非駆動ビルド時は以下のように定義を渡す。

```powershell
arduino-cli compile --fqbn teensy:avr:teensy41 --build-property "build.flags.defs=-D__IMXRT1062__ -DTEENSYDUINO=160 -DCHIRP_NO_MOTOR=1" --build-path C:/0Telepalpation_control/.build/chirp_simplified_test1_no_motor C:/0Telepalpation_control/sys_identification/chirp_simplified_test1
```

ソースの既定CHIRP_NO_MOTORは0（実駆動設定）を引き継ぐ。コンパイルだけでは駆動しない。

CAN2/ID2設定でCHIRP_NO_MOTOR=1/0の両ビルド成功。FLASH codeは非駆動78068 bytes・実駆動80820 bytes、RAM2静的274560 bytes・残り249728 bytes。既存FlexCAN_T4内の未使用値警告のみ。
