# 外部PD＋CAN＋SD版の検証記録（2026-10-19）

## 実装した内容

- `chirp_config.h`に実験条件、外部PD、独立した±10 N·m実験上限、旧コードのAK80-8 KV60プロファイル（位置±12.5 rad、速度±37.5 rad/s、CAN符号化トルク±32 N·m）を集約。
- 2 kHz IntervalTimer、リングバッファ、SDまとめ書き、保存ログのSerialダウンロードを維持。
- CAN1 / 1 Mbps / ID2、内部MIT kp/kd=0。最新の有効な非同期応答を使う外部PDとチャープをt_ffへ送信。
- 2秒前保持、40秒加振、1秒後保持、停止の86001行。開始目標を新鮮な応答から一度だけ取得。
- ±10 N·m超過は0指令への安全切替と異常終了。位置誤差の±180°折り返し・推測アンラップなし。
- 送信キュー蓄積を防ぐ専用メールボックス、厳密なRX形式検査、応答途絶・CAN/モータ/記録異常時の共通停止状態。
- CHIRPSD3（Header112 / Sample64 / Footer20 bytes）、異常statusと停止TX診断、PC変換・Serial取得・グラフ更新。旧SD1/2のPC変換を維持。
- 既定は非駆動。実機プロファイルとモータ側タイムアウトの確認フラグが未設定なので実駆動開始を拒否。

## コンパイル

Arduino CLI、Teensy core 1.62.0、SdFat 2.1.2、Teensy同梱FlexCAN_T4、`teensy:avr:teensy41`。

| ビルド | 結果 | FLASH code | RAM1残り | RAM2静的使用 / 残り |
| --- | --- | ---: | ---: | ---: |
| CHIRP_NO_MOTOR=1 | 成功 | 76084 bytes | 403328 bytes | 274560 / 249728 bytes |
| CHIRP_NO_MOTOR=0 | 成功 | 78516 bytes | 403328 bytes | 274560 / 249728 bytes |

CAN側ビルドでも実機確認フラグはfalseのまま。書き込み・起動・モータへの送信は実施していない。FlexCAN_T4のsetMB内部に未使用値の既存警告あり。プロジェクトコードのコンパイルエラーはなし。

両方のビルドには次の`build.flags.defs`を使用（最後の値を1/0に変更）：

```text
-D__IMXRT1062__ -DTEENSYDUINO=160 -DCHIRP_NO_MOTOR=1
```

## PC・設定の机上確認

```powershell
& 'C:\Users\User\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' -X utf8 -m unittest discover -s sys_identification -p 'test_chirp*.py'
```

11テスト成功。

- 43秒、86001行、4000/80000/2000/1行の区間境界と時刻。
- 前後区間でチャープ0、後保持でPD継続、正常停止行で最終指令0。
- PD単位・数式と、開始周波数が1以外の場合の位相式。
- 合成ログのBIN→CSV、Serial→CSV、設定JSON・結果JSON。
- ユーザー中断と異常statusの区別、開始前の0行異常ログ。
- トルク超過の安全用0指令、受信エラー・経過時間を残す停止スナップショット。
- CAN側正常ログの停止TXフラグ検査（物理到達を実測したものではない）。
- 欠落フッター、数値・区間・時刻・数式の破損、非ASCII Serial破損の拒否。
- 旧SD1/2形式と従来Serialログの互換。
- 実際のARM C++コンパイラで、設定ヘッダーの通常条件・開始周波数変更を受理し、非整数周期・非整数掃引/前後時間・符号化範囲を超える上限を拒否。

新CSVのグラフ生成を実行し、表示内容も確認した。テストデータはPythonで生成した合成データであり、TeensyのISRやCAN/SDハードウェアの実行結果ではない。11テストのうち設定ヘッダーのコンパイル検査は、このPCのARMコンパイラがない環境ではskipとなる。

## 実機では未確認

- Teensyへの書き込み、非駆動モードでの実SD記録とUSBダウンロード。
- CAN1配線・ID2・8-byte応答・実際のFWプロファイル。
- モータ位置の境界動作、速度/トルク換算、モータ側通信タイムアウトのMIT適用。
- SD write中の周期・ISR時間、RX欠落、TX_ABORT、通信断での停止挙動。
- 外部PDの安定性・実トルク・温度・機械的可動範囲。

駆動前の具体的な上位機画面・診断方法は`README_ja.md`の「実駆動前に確認する画面・診断手順」を参照。通信断ではゼロ/FDの到達は保証できず、CAN TX完了もモータ側の実行確認ではない。USBのxはSD書込み待ちによる処理遅延があり得る。
