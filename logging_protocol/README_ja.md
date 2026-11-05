# 共通ロギング転送プロトコル

## 目的

実験ごとの記録列・制御ロジック・変換後CSVの形式は維持し、PCとTeensy間の識別、ログ選択、転送、整合性確認を共通化する。Teensy 4.1には一度に一つのファームウェアを載せる。各ファームウェアがこの仕様を実装すれば、PC側は同じPowerShellコマンドでどの実験ログも取得できる。

## コマンド

ホストはASCIIコマンドをLF終端で送る。ファームウェアは次の共通応答を返す。

```text
LOG,1,IDENTIFY,<firmware_id>,<schema_id>
LOG,1,FETCH,<latest|previous>
```

識別子とスキーマIDはファームウェア／実験ごとに異なる。取得対象がない場合は`LOG,1,ERROR,NO_FILE`を返す。

## ファイル転送

転送開始時に、ファームウェアはファイル名、形式、スキーマ、長さ、全体CRC-32を送る。

```text
LOG,1,FILE,<name>,<format>,<schema_id>,<length>,<crc32>
```

ファイル本体は最大4096 byteのバイナリブロックに分ける。各ブロックの前にオフセット・長さ・ブロックCRC-32を送り、ホストがブロック全体を受信・検証してから1 byteの`+`を返す。受信失敗時は`-`を返し、そのブロックだけ再送させる。

```text
LOG,1,BLOCK,<offset>,<length>,<crc32>\n
<length bytes of binary data>
```

全ブロック送信後に終端を送り、ホストは受信長と全体CRC-32を照合する。

```text
LOG,1,END,<length>,<crc32>
```

ファイル長は転送の終端を決める情報として使う。固定の最大ファイルサイズでログを拒否しない。ホストは全体をメモリに載せず、一時ファイルへ順次書き込む。長さ・ブロックCRC・全体CRCが一致した時だけ完成ファイルとして確定する。元のSDログはPC側の検証成功だけで削除しない。

## CSVと変換

転送プロトコルはCSV列を規定しない。`schema_id`に対応する既存のプロジェクト別コンバーターが、その実験固有のBIN／CSV形式を解釈する。これにより記録列や解析コードを変えずに、取得と再送の挙動を統一できる。

## 移行

既存スケッチには`d/p`でCSV行を直接送る方式、`FETCH`でファイルを一括送信する方式、`TRANS_CHUNK`方式がある。移行中は各スケッチで旧コマンドも残し、共通コマンドと共通転送を検証してから旧経路を外す。SDに保存するスケッチを優先し、リアルタイム制御中のSerial出力を新しい待ち合わせ転送に置き換えない。

| ファミリー | 現在の方式 | 主な例 |
|---|---|---|
| システム同定 | SDのMC1バイナリをCSV行として逐次送信 | `sys_identification/chirp_multi-cycle` |
| 摩擦同定 | SD CSVファイル取得、ブロックCRC付きの版と一括送信の版が混在 | `fric_identification/fric_exp_1LSBmethod*` |
| 透過制御 | `TRANS_BEGIN` / `TRANS_CHUNK` / ACK | `controller_implementation/trans_ctrl_test*` |

