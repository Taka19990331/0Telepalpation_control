#line 1 "C:\\0Telepalpation_control\\fric_identification\\fric_exp_1LSBmethod2\\README_ja.md"
# 1LSBmethod2 固定ステップ摩擦掃引

AK80-8 と Teensy 4.1 を用い、トルク指令を固定時間間隔で掃引するファームウェアです。各ステップを1LSB/2LSB単位で100 msごとに更新します。定常判定による待ち時間やステップ送りは行いません。

## 掃引順序

| phase | 指令変更 | ステップ数 | 到達指令（目安） |
| --- | ---: | ---: | ---: |
| 1 | +1 LSB | 64 | +1 Nm |
| 2 | +2 LSB | 64 | +3 Nm |
| 3 | -2 LSB | 64 | +1 Nm |
| 4 | -1 LSB | 64 | 約0 Nm |
| 5 | -1 LSB | 64 | -1 Nm |
| 6 | -2 LSB | 64 | -3 Nm |
| 7 | +2 LSB | 64 | -1 Nm |
| 8 | +1 LSB | 64 | 約0 Nm |

すべての更新間隔は100 msです。phase 2の最終ステップ後、phase 4のゼロ指令到達後、phase 6の最終ステップ後、phase 8のゼロ指令到達後に、それぞれ100 ms保持します。全512ステップの指令掃引時間は約51.2秒です（開始待ちと停止確認時間を除く）。ゼロ指令はカウント2047で、デコード値は約-0.007814 Nmです。

## 実行

Arduino IDEまたはTeensy対応ビルド環境で `fric_exp_1LSBmethod2.ino` を開き、Teensy 4.1へ書き込みます。`fric_config.h` のモーターID/CAN設定が接続先に合っていることを確認してください。

シリアル端末から直接開始する場合は、115200 baudで接続し、改行を送る設定にして、`READY`の後に小文字の `s` を送信します。`STARTED`はログ準備の受理応答です。Pythonの`start`サブコマンドも引き続き使用できます。

```powershell
python .\fric_identification\fric_exp_1LSBmethod2\retrieve_csv.py start --port COM5
python .\fric_identification\fric_exp_1LSBmethod2\retrieve_csv.py status --port COM5
python .\fric_identification\fric_exp_1LSBmethod2\retrieve_csv.py fetch --port COM5
```

各実験はmicroSD上に別々のCSV（`R2001.CSV`など）として保存されます。未取得ログが残っていても次の実験を開始できます。ログは上書きされず、最大999件まで保管します。`fetch`は古い未取得ファイルを1件ずつ取得してmicroSDから削除するため、複数件まとめて取得するときは`fetch`を繰り返してください。取得したCSVはプロジェクトの `data` フォルダに `1LSBfric2_*.csv` として保存されます。

開始時の空き容量確認は、保守的な最大行サイズで75秒分（約48 MB）を要求します。

## ログと停止

ログにはphase、phase内ステップ、指令カウント、目標トルク、モーター推定トルク、速度、位置、CAN送信状態、停止理由を記録します。定常統計用の旧CSV列は互換性のため残してあり、値は `nan` です。

15 rad/sおよび20 rad/sの速度による停止条件はありません。モーター/CAN異常、フィードバック途絶、制御周期超過、SD異常、15分の実行時間上限などの停止条件は残しています。正常完了時と異常停止時のどちらも停止確認後にDisableを送り、CSVはmicroSDへ保存されます。
