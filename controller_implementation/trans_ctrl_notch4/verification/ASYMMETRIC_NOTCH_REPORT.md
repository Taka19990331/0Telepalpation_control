# 非対称FIXEDノッチ（2 kHz）

Teensy 4.1 / teensy:avr 1.62.0でSD ON・OFF両構成のビルド成功（終了コード0）。FlexCAN_T4ライブラリ内の警告あり。ログはasymmetric_build_sd_on.txt / asymmetric_build_sd_off.txt。出力はC:/0Telepalpation_control/build/asymmetric_notch2_onとasymmetric_notch2_off。Windowsの一時ファイルロックを避けるため、新規一時フォルダでビルドした。

2次IIRの零点を12 Hzに置き、極の位置をずらした近似設計。DC正規化したノッチと入力をDepth=0.95で混合する。状態数2、double演算、サンプル単位処理。FFT・ブロックバッファ・追加の純遅延はない。フィルタ自体の位相変化は図に示す。

MATLABによる係数応答の評価結果：

| 項目 | 実現値 | 誤差 |
|---|---:|---:|
| 低側−3 dB点 | 8.000000000 Hz | <0.000001 Hz |
| 高側−3 dB点 | 32.000000000 Hz | <0.000001 Hz |
| 12 Hz利得 | −26.020599913 dB | Depthからの理論値と数値精度内で一致 |
| 最小利得の周波数 | 11.909500 Hz | −0.090500 Hz |
| 最小利得 | −26.412870727 dB | −26 dBに対し−0.412871 dB |
| DC利得 | 1 | 約2e−12 dB |
| 最大極半径 | 0.986033794889 | 1未満、安定 |

最小点は0.0005 Hz刻みの探索。8/12/32 Hzでの位相は−54.430080 / 0 / +16.165662度。DCからNyquistまでの数値走査で最大利得は1（丸め誤差程度）。高周波通過域は0 dBには戻らず、Nyquistで約−2.280487 dBとなる。この減衰を伴う低次数近似であり、指定形状への完全一致ではない。

## 変更と再生成

- control_config.h: NOTCH_LOW_SIDE_HZ=4、NOTCH_HIGH_SIDE_HZ=20。中心からの−3 dB点の距離。
- fixed_notch.h: 生成済み係数を使用。既存の更新・初期化・有限値チェック・Depth混合を維持。
- notch_coefficients.h: MATLAB生成の係数と設定一致のstatic_assert。
- save_data.h: SDメタデータに低側・高側幅と新しい設計名を記録。ログレコード形式は維持。
- verification/design_asymmetric_notch.m: 設定ファイルを読み、係数生成、周波数応答、極確認、図と結果テキストを出力。

設定変更後は、このフォルダの親をMATLABのカレントフォルダにし、run('verification/design_asymmetric_notch.m')を実行してからビルドする。係数を再生成しない場合はstatic_assertでビルドを停止する。

control_algorithm.h、摩擦モデル、Kr=0.75、LPF=1000 Hz、Q=400 Hz、Adaptiveの振幅判定・ゲイン制御、安全停止処理は変更していない。ノッチ本体はモード間で共有されるため、ADAPTIVEを選択すれば新しいノッチ特性が適用されるが、今回の評価はFIXED（適用率1）のみ。

MATLAB評価はフィルタ単体の数値応答であり、実機の閉ループ安定性や500 µs以内の実測処理時間の確認ではない。実機書き込み・駆動は行っていない。Teensy_program_ver1.txtは旧版スナップショットとして保持し、ビルド対象は.inoと各ヘッダ。
