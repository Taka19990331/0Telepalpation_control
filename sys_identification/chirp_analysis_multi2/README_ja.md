# 複数チャープ解析（元版コピーと連結CSV解析）

このフォルダは `chirp_analysis_multi` の解析コードを複製したものです。今回の連結収録CSVは元版のメタデータ形式と異なるため、専用スクリプト `analyze_chirp_multi2.m` を追加しています。

MATLABで次を実行します。

```matlab
cd('C:\0Telepalpation_control\sys_identification\chirp_analysis_multi2')
analyze_chirp_multi2
```

入力は `../captures/MC0001_20261105_101724.csv` です。各掃引の開始は `command_Nm` がゼロから外れた点で検出します。終了は、非ゼロの`chirp_Nm`の後に記録された最初の`chirp_Nm=0`点（同時に`command_Nm=0`）です。記録器は掃引間のゼロトルク休止行を省略しているため、周期的なゼロ交差は指令トルクのゼロと一致しないことも終了条件にします。

このCSVの10回目はゼロ終端行が記録されていないため、解析から除外します。観測したゼロ終端がある9掃引だけを出力・同定に使用します。

出力は `results/yyyyMMdd_HHmmss_SSS` に保存します。

- `trial_01_trimmed.csv` ～ `trial_09_trimmed.csv`: 自動トリミングした完全な掃引データ
- `sweep_summary.csv`: 各掃引の開始・終了インデックス、継続時間、トルク情報
- `frf_by_trial.csv`: 掃引ごとの角度/チャープ指令トルク周波数応答
- `frf_ensemble.csv`: 元版と同じ9掃引のH1周波数応答とコヒーレンス
- `j_b_estimate.csv`, `j_b_multistart.csv`: 9掃引を等重みとしたJ/B同定結果と多点初期化の詳細
- `j_b_delay_summary.csv`, `delay_frf_scan.csv`, `delay_time_scan.csv`: J/B固定後のFRF基準・時間領域基準の無駄時間推定
- `phase_delay_by_trial_band.csv`: 試行ごとの帯域別位相遅延診断
- `figure_gain_phase_coherence.png/.fig`: 元版と同じ実測H1・Model A・遅延Model Bのゲイン/位相とコヒーレンス図
- `figure_trial_phase_delay.png/.fig`: 元版と同じ試行別位相遅延・30–100 Hz帯の平均±SD図
- `analysis.mat`: 入力、検出境界、周波数応答および集計データ
- `analysis.json`: 実行結果のJSON形式
- `analysis_summary_ja.md`: 今回の解析条件と結果の要約

今回の実行結果は `results/20261105_101724` にあります。

要約解析と図は元版と同じ `qm.summaryAnalysis`・`qm.summaryFigures` を使います。周波数応答には `qm.regression` と `qm.spectra` を使い、既知の1–100 Hz対数チャープ位相、4周期のHann窓、余弦・正弦・定数・時間傾向の4項回帰、独立試行のH1推定とコヒーレンス計算を元版と同じにしています。

J/Bと遅延の計算も、複製した元版の `qm.fit` と `qm.delay` を呼び出します。ただし、このCSVには元版が使うメタデータの基準角と1秒のphase=2保持データがありません。基準角は記録済みPD則から試行ごとに復元し、欠落している保持区間は使わず、各phase=1掃引だけで評価します。そのため計算手順・モデルは元版と揃えていますが、推定に使うデータ範囲は元版の個別CSVと完全一致しません。

元版の `analyze_chirp_multi.m` と `qdd_chirp_multi.m` も保持しています。今回の入力には専用スクリプトを使用してください。
