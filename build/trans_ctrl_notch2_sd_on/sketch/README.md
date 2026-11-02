#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch2\\README.md"
# trans_ctrl_notch2

Teensy 4.1 / AK80-8 ID2 / CAN2 / 1 Mbps 用の角度振幅依存RFOBノッチ制御です。固定ノッチが動作する `trans_ctrl_notch1` を複製して作成しました。複製元には変更を加えていません。

初期モードは `NOTCH_ADAPTIVE`、制御はOFF、Kr=0.75、Kf=0、トルク上限1.0 Nm、周期500 µsです。`S` による再開始時も初期モードへ戻ります。`NOTCH OFF`、`NOTCH FIXED`、`NOTCH ADAPTIVE` は制御OFF中に変更できます。角度しきい値はプログラム実行中に `ANGLE_START <deg_RMS>` / `ANGLE_FULL <deg_RMS>` で調整できます。値は正数かつ開始値<全適用値である必要があります。ADAPTIVE運転中の変更はAttack/Releaseで適用率へ滑らかに反映します。`STATUS`で現在値を表示します。従来の `S`、`ON`、`OFF`、`KR`、`KF`、`BIAS`、`STATUS`、`X`、`D` コマンドも維持しています。

角度検出器は新しいCANフィードバックフレームごとに1回更新します。角度は±12.5 radの折り返しを除去してから、8 Hz 4次Butterworth高域通過部と20 Hz 4次Butterworth低域通過部（合計8 biquadセクション）を通し、0.2秒LPFでRMS化します。BPF係数は2 kHz設計です。角速度の平均は2 Hz LPFです。

閾値、ゲート、Attack/Release/Hold、固定ノッチ設定は [control_config.h](control_config.h) のUSER SETTINGSにあります。デフォルトの開始/全適用閾値は0.15/0.25° RMS、速度ゲートは0.3〜0.5 rad/s、Attack/Release/Holdは0.05/0.5/0.25秒です。

検証手順:

- Python単体・変換テスト: `python -m unittest discover -s tests -p "test_*.py"`
- 既存3ログの角度検出再生: `python verify.py`（NumPyが必要、計算2 kHz・CSV出力200 Hz）
- Teensy 4.1 SD ON/OFFビルド: `./build_verify.ps1`
- BIN変換: `python convert_trans_log.py <TCxxxx.BIN>`（旧TCBIN2と新TCBIN3に対応）

実装・オフライン結果・ビルド状況・実機未確認事項は [REPORT_ja.md](REPORT_ja.md) を参照してください。オフライン再生は角度検出と適用率の評価であり、閉ループの振動低減率を予測するものではありません。
