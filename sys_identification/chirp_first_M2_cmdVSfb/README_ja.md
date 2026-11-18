# M2 command vs feedback chirp amplitude

`analyze_chirp_first_M2_cmdVSfb.m` を MATLAB で実行します。入力は既存解析と同じ `../captures/chirp_*.csv` です。基準ファイル `chirp_20261020_102301_899188.csv` と照合し、Motor ID 2、同一チャープ条件、正常完了の試行を自動選択します。2026-10-20 10:23:01 より前の別試験は除外します（既存 `chirp_analysis_multi` の選定条件）。

## 出力

実行ごとに `results/yyyyMMdd_HHmmss` を作成します。

- `figure_1_torque_amplitude.png/.fig`: 時系列平均した指令トルク波とフィードバックトルク波の振幅。フィードバックは各試行の振幅標準偏差をシェーディング。
- `figure_2_feedback_speed.png/.fig`: 時系列平均したフィードバック速度波の振幅。シェーディングは各試行の振幅標準偏差。
- `mean_waveforms.csv`: チャープ開始を時刻0として試行波形を時系列平均したデータ。
- `amplitude_summary.csv`: 平均波形から求めた振幅と、試行ごとの振幅平均・標準偏差。
- `included_trials.csv`: 採用したCSV一覧。

周波数は対数チャープの既知位相を使い、中心周波数の前後2周期（合計4周期）の局所正弦波最小二乗フィットから振幅を求めます。グラフの線は時系列平均波形から求め、シェーディングは試行ごとの振幅の標準偏差です。波形は共通の名目サンプル時刻に補間して平均します。追加Toolboxは使いません。
