#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\REPORT_ja.md"
# trans_ctrl_notch1 実装・検証記録

作成日: 2026-11-02。既存フォルダのソースは変更していない。

## ベースの確認

現行test2はStribeck摩擦モデルと位置方向による低速摩擦FFを備え、上限1.5 Nm。
test3はtanh摩擦モデル（omega_0=0.7 rad/s）、上限1.0 Nm。
両方とも速度LPF=1000 Hz、RFOB Q=400 Hz、Kr=0.75、Kf=0、初期OFF。
test3にはTeensyビルド履歴があり、指定設定と一致するため、今回のベースに選択した。
test3のCOPY_REPORTはtest2と同一と記すが、現行コードは実際には異なる。
接続実機のフラッシュは読み出していないため、「実際に最後に使用した版」は断定できない。
123759 CSVはtest1名だが、記録メタデータは速度1000 Hz/Q400 Hz/Kr0.75/Kf0/上限1.5 Nmで、現在のtest1設定とは異なる。
このCSVからRFOBを再計算せず、記録されたexternal_torque_est_Nmを入力した。

## 変更箇所

| ファイル | 変更内容 |
|---|---|
| control_config.h | NOTCH_ENABLED、NOTCH_CENTER_HZ=12、NOTCH_BANDWIDTH_HZ=4、NOTCH_DEPTH=0.95と設定検査 |
| fixed_notch.h | 固定係数2次ノッチ、double内部状態、定常DC初期化、有限値検査 |
| control_algorithm.h | command()で1周期1回更新し、その出力にKrを乗算。reset()とfiniteState()へノッチ状態を追加 |
| save_data.h | BINヘッダにノッチ有効・中心・幅・深さ・設計方式を追加 |
| trans_ctrl_notch1.ino | test3からファイル名のみ変更、内容はSHA256一致 |
| can_protocol.h / can_diagnostics.h / position_direction.h | test3から無変更コピー |
| convert_trans_log.py | test3から無変更コピー |
| build_verify.ps1 | 出力名をnotch1へ変更 |
| verify.py / tests/verify.cpp / tests/test_binary_log.py | オフライン・C++差分・既存バイナリ変換の検証 |

制御設定: 速度LPF1000 Hz、Q400 Hz、Kr0.75、Kf0、周期500 us、上限1.0 Nm、初期OFF。
振幅依存・ダンピング制御は追加していない。RFOB、摩擦FF、CAN、タイムアウト、停止手順はtest3のまま。

## フィルタ定義と更新

RBJノッチ Hn を用いて H=(1-depth)+depth*Hn とする。
Q=f0/BW=3。幅4 Hzは基になる完全ノッチの-3 dB幅の定義（デジタル化による微小差あり）。
参照: https://www.w3.org/TR/audio-eq-cookbook/ 。設定はHzで、オクターブ幅ではない。
中心で振幅0.05、DCで1。係数と状態をdoubleとし、出力のみfloat。
係数計算の三角関数は構築時だけ。更新はcommand()だけで行い、CAN受信回数に依存しない。
受信がない周期は保持された推定値を入力する。運転中の制御OFFでも更新する。
初回ready時は入力一定の定常状態で初期化。S/BIASによる既存reset()で状態をクリアする。
制御ON/OFFはリセットしない。運転停止後は既存通りISR処理しない。
ノッチOFFはcontrol_config.hのNOTCH_ENABLED=false、またはTRANS_ENABLE_FIXED_NOTCH=0で指定。
OFF時の式は従来と同じfloat乗算 Kr*signals.external。
ノッチの有効設定はビルド時固定で、実行時にノッチだけを切り替えるコマンドは追加していない。

external_torque_est_Nmはノッチ前、torque_RFOB_NmはKrを含むノッチ後補償（制御OFF時0）。
Sample112 byte、Record120 byte、TCBIN2、CRC、CSV29列の順序を維持。

## 検証結果

verification/results.json、offline.csv、response.csvを参照。

- 解析応答: DC=1.000000000000075、12 Hz=-26.0205999 dB。
- 極半径=0.9937379094 < 1、安定。
- Python参照計算の全状態・出力が有限。一定入力2 Nmで初回からDC定常値を再現。
- 123759: 28159行、記録間隔498〜502 us。直前値保持で2 kHzへ再標本化した28158点を使用。
- 最初の2秒を除き、平均除去・Hann窓FFTで10〜14 Hzの合計パワー比=-10.7252 dB。
- 全区間RMS: 入力0.786260 Nm、出力0.758043 Nm。全帯域RMSの低下量は12 Hz減衰量とは異なる。
- これは既存記録へ適用した信号処理結果。新しい閉ループの振動抑制や安定性を実証する結果ではない。
- Python参照でOFF中も状態更新する切替と、同一状態で連続ONした出力の一致を確認。
- 制御OFFでは指令0。ON時には補償指令への段差が残り、当該CSVで取り得る段差は最大1.0 Nm。
  状態リセット由来の過渡は抑えるが、バンプレス制御やゲインランプは実装しない。
- ノッチOFFの参照乗算は完全一致。C++差分テストにはtest3とのRFOB・摩擦・指令比較、
  複数/ゼロ受信、100000周期、ON/OFF、非有限値ラッチ・resetの検査を用意。
- C++テストはノッチON/OFFともコンパイル成功。ただし生成exeはWindows Application Control
  (WinError4551)で実行できず、C++実行テスト合格とは扱わない。
- 既存バイナリ変換テスト3件（29列、CRC/連番、完了/回復）合格。

実機書込み・モータ運転は未実施。ISR所要時間、2 kHz実測周期、実機ON/OFFのトルク過渡、
12 Hz振動抑制効果、SD記録中の動作、安全停止の実動作は未確認。

再検証: `python verify.py`（C++コンパイラは.tools/ziglang/zig.exe）。
実行制限環境では `python verify.py --python-only` により参照計算だけを実施できる。
ビルド: `./build_verify.ps1`（Teensy 4.1、SD ON/OFF）。
