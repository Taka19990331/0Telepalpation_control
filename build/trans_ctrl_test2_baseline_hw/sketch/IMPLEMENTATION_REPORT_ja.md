#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_test2\\IMPLEMENTATION_REPORT_ja.md"
# trans_ctrl_test2 実装・検証報告

## 作成内容

`C:\0Telepalpation_control\controller_implementation\trans_ctrl_test1`の全38ファイル（data、既存テスト、文書、キャッシュを含む）を`trans_ctrl_test2`へ複製した。Arduinoのフォルダ名規則に合わせ、主スケッチだけを`trans_ctrl_test2.ino`へ改名した。元フォルダは、作業開始時の全ファイルSHA-256と終了時の比較で無変更を確認した。

既存ファイルの変更は次の4ファイルのみ。

| ファイル | 変更 |
| --- | --- |
| trans_ctrl_test2.ino | handleFeedbackから既存の位置値を追加引数として渡す1行のみ |
| control_config.h | 50 ms、3 LSB、0.8/1.2 rad/sの新規定数のみ追加 |
| control_algorithm.h | FF専用の値と位置履歴を追加。既存の速度LPF/RFOB計算を呼び出してからFF専用値を計算し、torqueFに使用 |
| build_verify.ps1 | 元版のビルド成果物を上書きしないよう、出力・一時フォルダ・結果ログの名前だけ変更 |

新規ファイルは`position_direction.h`、`tests/direction_test.cpp`、`tests/test_direction.py`、`tests/test_preservation.py`、本報告書、`test1_to_test2.diff`。

既存のREADME、過去の報告書、`trans_ctrl_test1_all_sources.txt`、ダウンロードスクリプト、実測CSVは複製時のまま保存している。従って旧名称や旧ソースの記載は歴史的な複製内容であり、今回の実装説明は本書を参照する。

## 方向判別

- 新しいCANフィードバックを処理するたびに位置と`rxUs`を履歴へ保存する。制御ISRのみの進行やcommand呼び出しでは更新しない。同一受信時刻のサンプルは既存オブザーバと同様に更新しない。
- 現在時刻から50 ms以上前にある最新の受信サンプルを比較基準とする。50 ms境界にサンプルがなければ、その直前のサンプルを使用する。履歴の時間幅は約50 msとなり、受信欠落時には長くなる。
- 50 ms分の履歴が揃うまではUNKNOWN。`delta = position_now - position_reference`を計算し、±12.5 radをまたぐ差分は25 rad周期の最短差分へ補正する。50 ms内の実際の移動はMIT速度範囲でも半周期未満であることを前提とする。
- `delta > 3*(25/65535)`でPOSITIVE、`delta < -3*(25/65535)`でNEGATIVE、それ以外はUNKNOWN。方向は保持せず、毎回再評価する。停止後は窓内変位が閾値以下になればUNKNOWNへ戻る。
- 時刻差はuint32_tの差で計算し、micros折り返しに対応する。履歴は1024件（8192 bytes）で、1 MbpsのCANで50 ms内に受信できる件数を上回る。万一満杯なら履歴を再開始し、完全な時間窓が揃うまでUNKNOWNとする。

## 摩擦FFとRFOB

摩擦モデル式と正負すべてのパラメータは無変更。速度の大きさには従来の15 Hz LPF後のCAN速度の絶対値だけを使用する。位置から速度を推定・置換しない。

- `|v| <= 0.8`：位置方向で符号を決めた速度を既存の摩擦モデルへ入力する。UNKNOWNならFF用摩擦値は0。
- `0.8 < |v| < 1.2`：`alpha=(|v|-0.8)/(1.2-0.8)`として、位置方向の摩擦値と元のCAN速度による摩擦値を線形補間する。UNKNOWNなら位置側は0となるため、FF全体は`alpha*friction_original`となる。
- `|v| >= 1.2`：既存摩擦関数の結果を直接返す。補間演算を通さず、従来結果との数値的一致を維持する。

既存RFOBに使用する`signals.friction`とその計算、LPF、初期化、input/state/externalは一切変更していない。FF用の値は別に保持する。旧3引数feedbackインターフェースは既存テストとの互換性のため維持し、実スケッチでは位置付き4引数を使用する。

ログの列・順序・バイナリ形式は無変更。`friction_model_Nm`は従来どおりRFOBへ入る摩擦モデル値であり、位置方向補正後のFF値ではない。実際の摩擦FFトルクは既存の`torque_FF_Nm`列で確認する。新しい方向ログ列は追加していない。

J=0.0116806、Q_HZ=10、VELOCITY_HZ=15、INITIAL_K_R=0、INITIAL_K_F=0.6、トルク制限、2 kHz周期、MIT/CAN、受信キュー、タイムアウト、シリアルコマンド、安全停止処理は従来どおり。

## 不連続と判別の限界

速度域の境界0.8/1.2 rad/sは、方向状態が同じなら連続となる。一方、UNKNOWNから確定した瞬間や方向反転の瞬間には、低速域のモデル値に段差が生じ得る。`|v|=0.01 rad/s`でUNKNOWN→正方向は約0.526333 Nm、正→負は約−1.053356 Nm（いずれもK_F適用前）。初期K_F=0.6ではそれぞれ約0.315800 Nm、−0.632014 NmのFF変化に相当する。実際の指令は既存RFOB項と合成後、従来の制限を受ける。

この段差を除去するランプ、レート制限、ヒステリシス等は今回追加していない。段差が実機評価で問題となる場合の平滑化は別の検討項目とする。低速すぎて50 ms中に3 LSBを超えない移動ではUNKNOWNのままとなる（定速なら概ね0.0229 rad/s以下）。

静止ノイズの検証は提示された実測「最大幅1～2 LSB」に基づき、±1 LSB（全幅2 LSB）で行った。もし±2 LSBが全幅4 LSBを意味する場合、3 LSB閾値では移動とノイズを常に区別できない。今回、閾値を勝手に変更していない。

## 検証結果

- 既存ホストC++テスト：120,000制御tick、119,364フィードバック更新で従来結果と完全一致。FIFO、処理予算、overflow、時刻折り返し、NaN停止を確認。
- 既存バイナリログテスト3件：すべて成功。
- 追加方向テスト：静止揺れ、3 LSB閾値、正負移動、反転、停止、位置折り返し、時刻折り返し、不規則受信、同一時刻、履歴リング更新を確認。
- 遷移テスト：正負速度、3方向状態で0～2 rad/sを走査し、指定の補間式と一致。0.8/1.2 rad/sの境界連続性も確認。
- 元のtrans_ctrl_test1のヘッダを直接比較対象に、位置付きの実インターフェースへ同一の120,000フィードバックを入力。全サンプルでLPF速度・元摩擦値・RFOB input/state/external・RFOBトルクが厳密一致。通常速度域80,491サンプルでFFと指令トルクも厳密一致。
- 静止相当系列で有効出力中14,995サンプルのFFが0となること、CAN速度と位置方向が逆符号の場合に位置側の符号が採用されること、CAN受信なしでは履歴が進まないことを確認。
- ファイル保全テスト：通信、ログ、既存テストなどのバイト一致と、スケッチの差分が位置引数1箇所だけであること、既存設定値の完全保存を確認。Python unittest計5件すべて成功（上記C++追加テストを含む）。
- Teensy 4.1 / teensy:avr 1.62.0：SD有効・無効の両方でビルド成功。FlexCAN_T4内の未使用値とstrncpyに関する警告あり。
- 実機への書き込み、モータ駆動、実機でのISR実行時間測定は未実施。2 kHzタイマ設定は無変更であり、追加処理による実測実行時間の影響は未確認。

ビルドとテストはサンドボックス内でツール起動エラーがあったため、通常実行環境で実施した。

## 成果物と再実行

差分は本フォルダの`test1_to_test2.diff`を参照。既存ファイル4件の変更に加え、新規実装・テストも含む。ビルド成果物はワークスペースの`build/trans_ctrl_test2_sd_on`と`build/trans_ctrl_test2_sd_off`。実行記録は`build/trans_ctrl_test2_build_sd_on.txt`、`build/trans_ctrl_test2_build_sd_off.txt`、`build/trans_ctrl_test2_test_results.txt`。元ファイルのハッシュ記録は`build/trans_ctrl_test2_source_hashes.json`。

PowerShellから次を実行する（pythonは使用可能なPython実行ファイルへ置換可能）。ホストテストの直接比較には隣の元フォルダも使用する。

```powershell
python C:/0Telepalpation_control/controller_implementation/trans_ctrl_test2/tests/run_verify.py
& C:/0Telepalpation_control/controller_implementation/trans_ctrl_test2/build_verify.ps1
```
