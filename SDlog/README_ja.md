# 共通ロギング転送

このフォルダは複数プロジェクトから共有する転送部品の置き場です。各プロジェクトのSD保存形式・CSV列・制御処理は個別のままにし、TeensyとPowerShell間のログ取得だけを共通化します。

## 使用方法

PowerShellから次を実行します（既定ポートはCOM3、既定対象は最新ログ）。

```powershell
powershell -ExecutionPolicy Bypass -File C:\0Telepalpation_control\SDlog\fetch_log.ps1
```

ポートや過去ログを指定するときは`-Port COM6`、`-Which PREVIOUS`を付けます。取得CSVは`C:\0Telepalpation_control\data`に`Teensyプログラム名_yyyyMMdd_HHmmss.csv`で保存されます。同秒に同名が存在するときだけ連番を付け、既存ファイルを上書きしません。

## 通信仕様 v1

ホストはLF終端の`LOG,1,IDENTIFY`を送り、ファームウェアは`LOG,1,IDENTIFY,<firmware_id>,<schema_id>`を返します。続いてホストは`LOG,1,FETCH,LATEST`または`LOG,1,FETCH,PREVIOUS`を送ります。

```text
LOG,1,FILE,<firmware_id>,<schema_id>,<source_filename>,<byte_length>,<crc32>,<epoch_seconds>
LOG,1,BLOCK,<offset>,<length>,<crc32>\n
<length bytes of payload>
LOG,1,END,<byte_length>,<crc32>
```

データブロックは最大4096 byteです。ホストは1ブロックを受信しCRCを検証してから`+`を返します。不完全または破損したブロックには`-`を返し、Teensyが当該ブロックを再送します。全体の長さとCRC、および終端情報が一致した後にだけファイルを確定します。サイズに独自の上限を設けず、ホストは一時ファイルへ逐次書き込みます。Teensy上の原本は取得後も自動削除しません。

共通のファームウェア送信処理は`LogProtocol.h`です。変換はスキーマ別の既存コンバーターを使います。

| Firmware ID | Schema | 保存形式 | 変換 |
|---|---|---|---|
| `chirp_multi-cycle_LOGtest` | `CHIRPMC1` | MC1バイナリ | `convert_multi_cycle.py` |
| `fric_exp_1LSBmethod2_LOGtest` | `CSV-FRIC2` | CSV | 検証後そのまま保存 |
| `trans_ctrl_test3_LOGtest` | `TCBIN2` | TCBIN2バイナリ | `convert_trans_log.py` |

## テスト複製

共通仕様の試験対象は`*_LOGtest`複製です。通常版スケッチの記録・転送処理にはこの試験移行を反映しません。各複製をコンパイルしてからTeensyへ一つずつ書き込み、取得時に`fetch_log.ps1`を共通して使います。実験の開始コマンドは取得試験では送信しません。

制御ログのテスト複製には、実験やCAN送信を行わずに1行のTCBIN2試験ファイルをSDへ作る`LOG,1,TESTDATA`コマンドがあります。実ログがない状態で転送と変換を検証するときだけ使います。
