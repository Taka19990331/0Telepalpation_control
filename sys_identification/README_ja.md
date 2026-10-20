# モータなしのチャープ生成試験

CAN1/ID2の外部PD＋40秒チャープを追加したSD版（既定は非駆動テスト）は `chirp_sd_test/chirp_sd_test.ino`。操作とCSV変換は [SD版の説明](chirp_sd_test/README_ja.md) を参照。以下は従来のRAM一括記録・シリアル転送版の説明。

`chirp_timer_test/chirp_timer_test.ino` をArduino IDEで開き、ボードに **Teensy 4.1**、USB TypeにSerialを選んで書き込む。
基盤は `AK80_communication/AK80_timer_test` のIntervalTimer割り込み・RAM記録・終了後出力方式。CANライブラリ、CAN初期化、モータへの送信は含まない。

## 操作

1. シリアルモニタを115200 baudで開く（Teensy USB Serialの実際の転送速度はUSBに依存）。
2. `s` を送信する。改行はあってもなくてもよい。
3. 約20秒間は出力せず、500 µsごとのタイマーISR内で指令を計算しRAMへ保存する。
4. 終了後、`CHIRP_BEGIN` から `CHIRP_END` までをまとめて出力する。出力には別途時間がかかる。
5. 出力終了後に再び `s` で再試験できる。測定中の `x` は次のISRでゼロ指令を記録して中断し、それまでの結果を出力する。

## PCで全行をファイル保存する（推奨）

Arduino IDEのシリアルモニタと、同じCOMポートを使う他のソフトを閉じる。上記スケッチを書き込んでからPowerShellで実行する。

```powershell
cd C:\0Telepalpation_control\sys_identification
python -m pip install -r requirements.txt
python capture_chirp.py --list-ports
python capture_chirp.py --port COM5
```

`COM5`は一覧に表示されたTeensyのポートへ置き換える。`python`が使えず`py`が使える環境では、コマンドの`python`を`py`へ置き換える。
このPCではCodex同梱Pythonも利用できる：

```powershell
$pythonExe = 'C:\Users\User\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $pythonExe -m pip install -r requirements.txt
& $pythonExe capture_chirp.py --list-ports
& $pythonExe capture_chirp.py --port COM5
```

接続後2秒待機し、古い受信バッファを捨ててから自動で`s`を送信する。マイコンは待機状態で開始すること。試験開始後はシリアルモニタを開かない。
デフォルトではこのフォルダ内の`captures`に日時付きの3ファイルを作成する。

- `chirp_日時_raw.txt`：受信した原文（設定、SUMMARY、開始・終了マーカーを含む）。
- `chirp_日時.csv`：ヘッダーと数値データのみ。Excelなどで開ける。
- `chirp_日時_validation.json`：完全性の検証結果。

正常なら`PASS`を表示して終了コード0。開始・終了マーカー、正常終了理由、40001行、index連番、nominal_us、実測周期の整合、数値の妥当性、SUMMARY件数、最終ゼロを検証する。波形の物理的妥当性や周期500 µsの達成をPASSが保証するわけではない。
切断・タイムアウト・Ctrl+Cでは受信済みデータを残して`FAIL`（終了コード1）とする。途中CSVは解析前に検証結果を確認する。Ctrl+CはPC受信だけを止め、マイコンは試験を継続する。
全体タイムアウトは180秒。必要なら`--timeout 300`、保存先は`--output-dir C:\0Telepalpation_control\data\chirp`で変更できる。

## 波形

- 対数チャープ：f(t) = 1 exp(k t)、k = ln(100/1)/20。
- 位相：φ(t) = 2π × 1 × expm1(k t)/k。
- 振幅：A(f) = 1 + 1.5 × (f − 1)/99 [N·m]。
- 指令：τ(t) = A(f(t)) sin(φ(t))。振幅は片振幅（±A）。位置保持項やトルク制限は加えていない。
- 0 ≤ t < 20 sで加振し、20 sの行ではゼロ。通常終了は **40001行**（加振40000行＋終了ゼロ1行）。加振の最後は19.9995 sなので、100 Hz・2.5 N·mは終了直前に近づく値。
- 今回は式そのものの確認用として終了フェードは入れていない。
- 経過時間はサンプル番号×500 µs。遅延が発生しても実測時刻に合わせて位相を補正しない。
- 位相精度確保のため関数内部はdouble、保存する指令はfloat。全波形の事前計算テーブルは使わない。

## RAMと時刻

12 bytes/行 × 40001行 = **480012 bytes** をTeensy 4.1のRAM2（DMAMEM）へ確保。RAM1のスタックを圧迫しない配置。Teensy 4.0などへの変更にはメモリ再設計が必要。

CSV形式のデータ行：

```text
DATA,index,nominal_us,actual_us,period_us,torque_Nm,ISR_us
```

| 列 | 意味 |
| --- | --- |
| index | 0からのサンプル番号 |
| nominal_us | 関数へ渡した経過時間（index × 500 µs） |
| actual_us | 初回ISR開始からの実測経過時間（micros） |
| period_us | 前回ISR開始との間隔。初回は比較対象がないため0 |
| torque_Nm | 計算したトルク指令。モータへの送信はしない |
| ISR_us | ISR入口から計算・記録・停止判定までの時間。末尾の時刻保存、カウンタ更新、割り込み復帰の時間は含まない |

`SUMMARY` は周期の最小・最大（初回除外）、ISR時間の最大、1000 µs以上の間隔の件数、500 µs以上のISRの件数。microsの分解能は1 µsなので短い処理は0になる場合がある。割り込みに遅延があると、ISR時間が短くても周期が乱れるため両方を確認する。

波形確認にはDATA行を抽出し、nominal_usまたはactual_usを秒へ変換してtorque_Nmをプロットする。20秒全体で振幅が増えること、開始時の瞬時周波数に対応する周期が1 s、終了付近が約10 msであること、最終指令が0であることを確認する。周期は概ね500 µs、actual_us − nominal_usに大きな累積ずれがないことも確認する。

検証：Arduino CLI / Teensy core 1.62.0、`teensy:avr:teensy41` でコンパイル成功。RAM2全体の静的使用量は492448 bytes、残り31840 bytes。実機書き込み、実測波形・ISR時間の確認は未実施。
