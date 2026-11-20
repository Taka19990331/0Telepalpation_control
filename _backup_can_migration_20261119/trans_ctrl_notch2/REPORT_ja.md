# trans_ctrl_notch2 実装・検証報告

作成日: 2026-11-02

## 1. 複製元と変更範囲

新規フォルダ `controller_implementation/trans_ctrl_notch2` を、固定ノッチを持つ `trans_ctrl_notch1` から複製して作成しました。`trans_ctrl_notch1` は変更していません。複製元は、直近の固定ノッチ実装および12 Hz/4 Hz幅/深さ0.95の設定を持つコードです。

| ファイル | 変更 |
|---|---|
| `control_config.h` | 角度BPF/RMS、速度ゲート、モード、Attack/Release/HoldのUSER SETTINGSを追加 |
| `control_algorithm.h` | ±12.5 rad角度アンラップ、4 biquadの8 Hz高域部＋4 biquadの20 Hz低域部、RMS/平均速度LPF、モード選択とγ補間を実装 |
| `trans_ctrl_notch2.ino` | CAN角度を新着フィードバックごとに検出器へ渡し、制御OFF中のみ `NOTCH OFF/FIXED/ADAPTIVE` を許可 |
| `save_data.h` | 適応制御の9項目をCSVへ追加し、ログをTCBIN3/152 byteへ更新 |
| `convert_trans_log.py` | TCBIN2/120 byteとTCBIN3/152 byteの双方を変換 |
| `verify.py`、`tests/` | 3記録の再生解析、モード式とバイナリ変換のテストを追加 |
| `build_verify.ps1` | 新フォルダ名をビルド成果物とログに使用 |

RFOB推定式、CAN処理・通信失敗対応、制御周期、指令飽和、停止処理は複製元のままです。摩擦モデルは `control_config.h` の `FRICTION_MODEL` でStribeck型またはtanh型を選択でき、初期値は `trans_ctrl_test2` と同じStribeck型です。角速度LPF 1000 Hz、RFOB Q 400 Hz、Kr 0.75、Kf 0、12 Hz固定ノッチ（幅4 Hz、深さ0.95）、500 µs周期、上限1.0 Nmを維持しました。

## 2. 角度振動検出器

実装はCANからの新しい角度フレームごとに1度だけ更新します。同じ角度を2 kHzで重複処理しません。設計サンプル周波数は2 kHzで、既存記録のサンプル間隔は自由回転/無操作でほぼ500 µs、把持記録で498〜502 µsでした。

角度差を±12.5 radの半幅で補正して連続角へ変換し、初期角度のDC成分を初期状態に反映します。8 Hz 4次Butterworth高域通過（2 biquad）と20 Hz 4次Butterworth低域通過（2 biquad）の積を使用し、合計8 biquad、両端の減衰は各4次です。中心近傍を単位利得に正規化しています。各セクションはdouble状態のtransposed direct-form IIです。

BPF出力を rad で保持し、500 µs設計刻みの0.2秒LPFで二乗平均を取り、平方根でrad RMSを得ます。平均速度は符号付きCAN速度を2 Hz LPFに通した値の絶対値です。ゲインは角度閾値間で線形補間し、速度ゲートは0.3 rad/s以下で1、0.5 rad/s以上で0とします。

## 3. ノッチモードと初期値

補償トルクは次の式です。

`tau_R = Kr * (external + gamma * (notch(external) - external))`

| 設定 | 初期値 |
|---|---:|
| 初期モード | `NOTCH_OFF` |
| 開始/全適用振幅 | 0.15 / 0.25° RMS |
| 速度ゲート | 有効、0.3〜0.5 rad/s |
| Attack / Release / Hold | 0.05 / 0.5 / 0.25 s |
| ノッチ | 12 Hz、4 Hz幅、深さ0.95 |

`NOTCH_OFF` はγ=0、`NOTCH_FIXED` はγ=1、`NOTCH_ADAPTIVE` は角度振幅と速度ゲートの積をAttack/Release/Holdで整形します。固定ノッチはすべてのモード・制御OFF中も毎周期更新し、状態を保ちます。モード切替は制御OFF中に限り、制御出力ゼロの間に適用します。

制御モードはシリアルで `NOTCH OFF`、`NOTCH FIXED`、`NOTCH ADAPTIVE` と入力します。角度しきい値は実行中に `ANGLE_START <deg_RMS>` と `ANGLE_FULL <deg_RMS>` で個別に変更できます。設定条件は `0 < start < full` です。拒否条件は現在値を保ち、設定値は新規運転 `S` 後も維持し、電源再投入時にUSER SETTINGSへ戻ります。ADAPTIVE運転中に変更しても適用率はAttack/Releaseで滑らかに追従します。`STATUS` はモード番号と両しきい値を表示します。ログには角度BPF rad、RMS rad/deg、平均速度、γ各値、ノッチ前外部トルク、固定ノッチ出力、モードを記録します。

## 4. オフライン再生

`python verify.py` は指示書の3 CSVを2 kHzへ直前値保持で再標本化し、実装と同じSOS係数・角度アンラップ・初期状態・RMS LPF・ゲート・Attack/Release/Holdを再生します。再生計算は2 kHz、出力CSVは容量を抑えるため200 Hzに間引いています。結果CSVは `verification/*_replay.csv`、要約は `verification/offline_results.json` です。

| 記録 | 最大角度RMS | γ>0.5時間割合（角度のみ/速度ゲート併用） | γ>0.5までの時間 | ゲート併用γ>0.5の立上がり回数 |
|---|---:|---:|---:|---:|
| 自由回転 `122211` | 0.1221° | 0% / 0% | 該当なし | 0 |
| 無操作 `122519` | 0.00163° | 0% / 0% | 該当なし | 0 |
| 把持振動 `123759` | 0.2984° | 87.56% / 87.56% | 約62 ms | 1 |

3記録では把持中の平均速度最大値が0.219 rad/sでゲート閾値0.3 rad/s未満だったため、速度ゲートによる差は現れませんでした。自由回転では平均速度最大5.43 rad/sでしたが、角度RMS自体が開始閾値未満でした。

Attack/Release/Hold候補は0.03/0.2/0.1秒で把持中87.65%、0.05/0.5/0.25秒で87.56%、0.1/1.0/0.5秒で87.26%がγ>0.5でした。既存の把持軌道では解除後の再立上がりは1回でした。実測角度軌道に制御を後付けで再生した検出器評価であり、新しい閉ループによる振動低減率・指令トルク応答・安定性の予測ではありません。

## 5. モード互換性とログ

PythonテストではOFF式が`Kr * external`、FIXED式が`Kr * FixedNotch(external)`になること、補間式が有限なことを確認します。制御モードは出力オフ中のみ切替可能です。C++単体テスト `tests/verify.cpp` にはDC初期化、12 Hz角度RMS、境界折り返し、各モードの確認を入れていますが、実行環境でホストC++テストの実行完了を確認できていません。

バイナリレコードはTCBIN2 120 byteからTCBIN3 152 byteに更新し、CSVは従来29列の後ろに角度・ゲインの10列を追加して39列です。コンバータはTCBIN2の旧29列とTCBIN3の新39列を両方出力します。Pythonの変換テストでは両形式のCRC、完了フッター、列数を確認しています。記録レコードは1サンプル32 byte増えます。リング行数とバッチ行数は維持され、リングRAM使用量は約64 KiB増えるため、SD書込み時間と制御ISR時間の実機測定が必要です。

## 6. テストとビルド

- Python単体・コンバータテスト: 3件すべて成功。
- 3ログオフライン再生: 成功。
- Teensy 4.1 SD ON/OFFビルド: 実施を試みましたが未完了。Arduino CLIが選択したTeensyコア1.62.0のctags起動でWindowsの `Illegal System DLL Relocation` が発生し、SD ONのコンパイル開始前に停止しました。以前の固定ノッチレポートで使用した1.60.0コアは現在のArduino15に存在しません。
- ホストC++テスト: Zigコンパイラでの起動もローカル一時出力先のアクセス拒否により未完了。
- 実機フラッシュ、モータ運転、2 kHz ISR時間、SD書込み中のCAN受信、停止動作: 未実施。

## 7. 実機試験前の確認事項

1. Teensyコア/ctags環境を復旧し、SD ON/OFFの双方をコンパイルする。
2. 新レコードのRAM使用量、ISR実行時間、500 µs周期維持、CAN受信遅延、SDバッチ書込みの最大時間を確認する。
3. OFF→FIXED→ADAPTIVEを制御OFF中に切り替え、モード表示とTCBIN3/CSV39列を確認する。
4. まず低トルク・非常停止待機でNOTCH OFF/FIXED/ADAPTIVEを比べ、把持振動、静止からの動き始め、自由回転、解除後再発振をログ付きで評価する。
5. 閾値は既存3ログ向けの暫定値です。特に連続回転時の±12.5 rad角度折り返し、速度ゲートを無効にした比較、実際の回転過渡は実機で確認してください。

適用率補間を行っても、OFFからONへの既存の出力段差自体をなくす設計ではありません。実機での安全性・制振性能はこのオフライン結果から保証できません。
