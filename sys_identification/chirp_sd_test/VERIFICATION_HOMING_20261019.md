# ZEROING専用カスケード制御（2026-10-19）

## 変更ファイル

- chirp_sd_test/chirp_config.h：復帰パラメータ・段階・停止理由・SD6構造・最大行数と容量検査。
- chirp_sd_test/homing_control.h：radベースの位置→速度、加速度制限、速度PI、条件付き積分、絶対/変化率トルク制限、移行と連続整定ゲート。
- chirp_sd_test/chirp_sd_test.ino：状態遷移、試行初期化、非有限検査、既存sendTorque経路、SD6記録/ダウンロード。
- chirp_sd_test/motor_protocol.h：復帰用上限を符号化にも渡し、量子化後もその内側へ丸める。既定引数は従来の実験上限。
- chirp_sd6.py（新規）：SD6 BIN/Serial専用デコーダと検証。
- convert_sd_chirp.py / capture_chirp.py：SD6振り分け、取得期限900秒。
- plot_chirp_sd3.py：段階5、PI成分、目標速度、制限フラグ、実際の可変PRE_HOLDに対応。
- test_chirp_homing.py（新規）：制御本体のC++コンパイル時実行とSD6検証。
- chirp_sd_test/README_ja.md / 本書：現在の操作・仕様・検証限界。

旧SD3/4/5のデコーダとテストは変更しない。既存の別ディレクトリやdataの作業内容には手を加えていない。

## 制御の要点

ZEROING(4)→HANDOVER(5)→PDのみPRE_HOLD(0)→SWEEP(1)→POST_HOLD(2)→STOP(3)。
既存の多回転位置をそのまま使い、誤差の折り返し・原点再設定はしない。
速度5 deg/s、目標加速度5 deg/s²、位置ゲイン1 /s、速度Kp=2 Nm/(rad/s)、Ki=1 Nm/rad、指令上限1 Nm、変化率1 Nm/s。
Kpは速度誤差5 deg/sで約0.175 Nmとし、Kiはその誤差で約0.0873 Nm/sずつ摩擦補償トルクを蓄える初期試行値。実機整定の保証ではない。
積分状態はNm。実測dtを使い、絶対上限とslew到達範囲に対する条件付き積分を行う。

ZEROINGの連続整定中もPIを続ける。完了後は直前の制限後PI指令から現在のPDへ2秒で補間し、PI積分を凍結。
移行終端で上限・slewのため純PDに到達できなければstatus16で停止。到達すればPI状態をリセットして完全に外す。
PRE_HOLDは最低2秒と連続1秒整定の両方を要求し、開始から15秒で未達ならstatus15。戻り再試行はしない。
ZEROING自体は180秒でstatus14。1 Nmで動かなくても自動増力はしない。

360 degは理想加減速だけで約73秒、約713 degの位置境界付近なら約144秒を要する。近傍の比例減速・摩擦・整定余裕も必要。
記録最大476002行、88 bytes/行、Header188/Footer20、容量41888384 bytes。リング360448 bytes。
最大許容制御間隔4周期で全行を記録した場合の時刻もuint32に収まること、行インデックス、FAT32容量を静的検査する。

## 維持した条件

- KP_PD=0.00834 Nm/deg、KD_PD=0.1 Nm/(rad/s)、実験上限10 Nm。
- calculateChirpの式・40秒・1→100 Hz・1→2.5 Nm・指数1は編集前と同じ。
- SWEEP/PRE/POSTでは従来の制限前合計超過で停止。ZEROING/移行は1 Nm飽和を通常制御として扱う。
- packRawCommandの標準/拡張MIT内部Kp/Kdゼロを維持。電流上限書き換えなし。
- CAN RX期限・エラー、位置境界/ジャンプ、周期超過、SD/リング、ユーザー停止、停止処理/再始動ロックを維持。
- ISRに待機・SD書込みは追加していない。

## 非駆動検証

`python -m unittest discover -s sys_identification -p "test_chirp*.py"`：19テスト成功。
新規テストは実装ヘッダそのものをC++ constexprで実行し、Teensy ARM GCC 15.2.1のstatic_assertで検査する（Pythonへの制御実装の書き直しではない）。80A/80B/60の3プロファイルで実行：

- 正負6.283 radからゼロ方向の目標速度・トルク。全ステップで目標速度/加速度・1 Nm・1 Nm/sを検査。
- 400秒相当の固定位置入力でも積分が暴走しないこと。大きな速度誤差による飽和中の積分停止、解除方向の積分許容。
- reset後の速度・積分・トルク・比例/制限前出力・両フラグのゼロ初期化。
- 移行時の指令連続性・上限・変化率・積分凍結、未到達時の移行拒否。
- 連続整定タイマーの中断/再開始、整定前の進行禁止、ZEROING期限、PD単独PRE_HOLD最低時間と未整定期限。
- MIT内部Kp/Kdゼロ、および量子化後も指令1 Nm以内（標準/拡張）。
- SD6 BIN/Serial往復、破損サイズ/旧形式/不正段階/上限超過/目標速度逸脱/PI式不一致/未整定SWEEPの拒否、停止理由14/15/16の診断ログ。
- 既存SD1～5の変換とチャープ/PD検証の回帰テスト。

合成ログでplot_chirp.pyのPNG/PDFを生成し、段階5・PI欄・速度目標・両制限フラグの描画を目視確認。これは形式/描画検証用の人工データ。

## ファームウェアビルド

Arduino CLI / Teensy core1.62.0 / 同梱SdFat2.1.2・FlexCAN_T4、teensy:avr:teensy41。最終ソースをCAN2/ID2（80B）の両設定でコンパイル・リンク成功。

| 設定 | FLASH code | RAM1ローカル用残り | RAM2静的 / 残り |
| --- | ---: | ---: | ---: |
| CHIRP_NO_MOTOR=1 | 79156 | 400096 | 372864 / 151424 |
| CHIRP_NO_MOTOR=0 | 82100 | 402144 | 372864 / 151424 |

FlexCAN_T4内の既存の未使用値警告のみ。
サンドボックス内では既存ctagsがDLL配置エラーとなったため、ビルドのみ昇格実行。アップロード・Serial接続・実機駆動なし。
Teensy platform.txtではbuild.extra_flagsを使わないため、実際に使用されるbuild.flags.defsへマクロを渡して再検証した。
例（非駆動）：

```powershell
arduino-cli compile --fqbn teensy:avr:teensy41 --build-property "build.flags.defs=-D__IMXRT1062__ -DTEENSYDUINO=160 -DCHIRP_NO_MOTOR=1" --build-path C:/0Telepalpation_control/.build/chirp_homing_no_motor C:/0Telepalpation_control/sys_identification/chirp_sd_test
```

この環境ではARDUINO_DIRECTORIES_DATAをC:/Users/User/AppData/Local/Arduino15、ARDUINO_DIRECTORIES_USERをC:/Users/User/Documents/Arduino、TEMP/TMPを作業内.build/tmpへ設定して実行した。

## 実機で残る確認・調整

閉ループの整定性能、実測速度の追従、摩擦・負荷とPIゲインの関係、目標速度/加速度・トルクslew、移行時間、弱いPD単独での保持、期限の妥当性。
実トルク、CAN/SD込み2 kHzの処理余裕、停止TX到達、機械のゼロ姿勢、モータ側通信断時処理も実機確認が必要。
NO_MOTOR_TESTの模擬位置は制御指令と無関係であり、今回のテストや模擬入力から実モータの閉ループ整定性能を検証したとは扱わない。
