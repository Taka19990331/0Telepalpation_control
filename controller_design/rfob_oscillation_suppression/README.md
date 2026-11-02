# RFOB振動抑制 MATLAB解析環境

基準プラントの追加診断は `diagnosis=run_baseline_diagnosis` を実行してください。
初期0–1秒の5種類の信号、122519の無操作時変動モデル、把持・摩擦117条件の感度解析を
`baseline_diagnosis/` に保存します。結果と限界は [BASELINE_DIAGNOSIS.md](BASELINE_DIAGNOSIS.md) を参照。
実測の持続振動は未再現であり、制振器の閉ループ予測は未検証です。
追加機能は `cfg.baselineOnly`（制振比較を省略）と `cfg.noise.enabled/seed/block_s`。
どちらも通常解析の既定では無効です。`cfg.noise.samples/dt_s` は有効時に122519から取得します。
基準比較は計測速度同士で行い、真の運動指標は `closed{1}.metrics`、計測指標は
`closed{1}.measurementMetrics` に保存します。追加検証は `verify_baseline_diagnosis` です。

## 実行

MATLAB R2021b以降＋ **Signal Processing Toolbox** が必要です。実行検証は MATLAB R2026a で行いました。Control System Toolbox、Simulink、非公開APIは使用しません。

```matlab
cd('C:\0Telepalpation_control\controller_design\rfob_oscillation_suppression')
results = rfob_oscillation_analysis;   % 引数なしで全基本解析
% または
run_rfob_oscillation_analysis
```

- `rfob_oscillation_analysis.m`：全解析ロジックとローカル関数。ユーザー設定は冒頭の CONFIGURATION。
- `run_rfob_oscillation_analysis.m`：セクション付き実行インターフェース。Live Editorで開いて「名前を付けて保存」から `.mlx` に変換できます。`.mlx` の偽装生成はしていません。
- `verify_rfob_oscillation_analysis.m`：検証用スクリプト。解析ロジックの重複実装はありません。
- `results/`：MAT形式の結果・有効設定、指標CSV、任意保存PNG/FIG。再実行で同名ファイルを上書きします。実行ごとに保存したい場合は `cfg.outputDir` を変更してください。

元CSVとTeensyファームウェアは読み取り専用として扱い、変更・複製しません。

## 設定と解析スイッチ

構造体で指定した項目だけデフォルトを上書きします。不明なキー、閾値の逆転等はエラーにします。

```matlab
cfg = struct;
cfg.controller.mode = 'damping'; % none / notch / damping / compare
cfg.controller.damping = 0.15;   % Nm s/rad
cfg.controller.on_rad_s = .10;
cfg.controller.full_rad_s = .20;
cfg.controller.bandHz = [8 20];
cfg.controller.rmsTau_s = .2;
cfg.controller.gainTau_s = .05;
cfg.controller.gainRate_s = 10;
cfg.figures = false(1,10);
cfg.figures([3 6 7 8 9]) = true;
cfg.savePNG = true;
results = rfob_oscillation_analysis(cfg);
```

| スイッチ | 初期値 | 内容 |
|---|---|---|
| RUN_DATA_ANALYSIS | true | メタデータ・選択区間・基本統計 |
| RUN_SPECTRAL_ANALYSIS | true | Welch PSD、任意FFT |
| RUN_AMPLITUDE_DETECTION | true | 3条件の因果的振幅検出 |
| RUN_FRICTION_COMPARISON | true | 3条件の摩擦平滑化・RFOB反事実再計算 |
| RUN_REPLAY_ANALYSIS | true | 把持ログ固定でBaseline/Notch/Dampingを比較 |
| RUN_CLOSED_LOOP_SIM | true | 基準モデル検証、制振比較、随意操作試験 |
| RUN_PARAMETER_SWEEP | false | 2軸スイープ |
| RUN_ROBUSTNESS_ANALYSIS | false | 剛性・粘性・遅延の直積と各方式の比較 |

解析スイッチと `cfg.figures(1:10)` は独立です。必要な解析がOFFの図は理由を表示して省略します。再生解析と閉ループ解析は各自に必要な検出器を計算するため、RUN_AMPLITUDE_DETECTIONがOFFでも動作します。`showFFT=true` は別Figure 11を追加し、スペクトル解析OFFなら省略します。

`controller.mode` は閉ループ比較方式を選択します。Baselineと摩擦平滑化のみは常に含め、compareでは4条件を比較します。再生解析は設計比較用に常に3方式を返します。スイープの方式は `sweep.mode` で独立に指定します。

デフォルトはログのメタデータからシミュレーションの周期・J・Q・速度LPF・上限・Kr/Kf・正負摩擦係数を設定します。引数 `cfg.sim` に明示した値は、その後に上書きされます。したがって同定値を変える試験では引数を使ってください。`cfg.sim.useLogMetadata=false` なら冒頭の仮定値のみを使用します。実測再構成は常に各ログのメタデータを使います。

## 区間・周波数解析

先頭メタデータと末尾の `#diag` 等の診断コメントに対応します。

- `selection.enabled=[NaN 0 1]`：自由回転はON/OFF両方を候補、無操作はOFF、把持はON。
- `startup_s=1`、`transitionGuard_s=.5`：起動と切替の両側を除外。0にすると含めます。
- `timeRange_s=[0 Inf]`：ログ相対時刻による選択。
- 有限値、受信有効、fault_code=0を必要条件とし、長い記録欠落で区切った**最長の連続区間**を使います。離れた区間を連結してFFTすることはありません。
- `equalDuration=true`：3条件を最短区間長へ揃えます。`maxDuration_s` で短縮可能。
- 元のON/OFF区間は `results.data(k).intervals`、実際に使った開始・終了は同構造体に記録します。
- nominal記録周期の等間隔時刻へ過去値保持で整列します。未来値を参照する補間・filtfiltは使いません。
- Welch：平均除去、8192点のperiodic Hann、50%重なり、NFFT=窓長、片側PSD。短い区間では窓を短縮します。デフォルト分解能は2000/8192=0.24414 Hz。
- 帯域RMSはPSDを周波数ビンと帯域の重なり幅で積分して平方根を取ります。単位は元信号と同じです。
- FFT表示は選択区間全体のHann窓、窓和で正規化、DC/Nyquist以外を2倍した片側ピーク振幅です。PSDとは別物です。

検出器・ノッチの再生状態は選択区間の先頭で0初期化します。図の最初にはフィルタ起動過渡が含まれます。摩擦反事実計算は選択前を含めた全ログで状態を進めてから区間を切り出します。

## RFOBとフィルタ

参照：`controller_implementation/trans_ctrl_test1/control_algorithm.h` と `control_config.h`。

ファームウェアの現在値は Q=10 Hz、速度LPF=15 Hz、上限1 Nm、Kr=.6、Kf=.2ですが、今回のログは Q=400 Hz、速度LPF=1000 Hz、上限1.5 Nm、把持Kr=.75、Kf=0です。**ログを優先**しています。

RFOBは `input = motorTorque - bias - friction + J*g*velocity`、`state += (1-exp(-g*dt))*(input-state)`、`external=J*g*velocity-state`。初回stateはJ*g*velocityです。ログ再構成は記録済みの最初のstateに合わせ、受信時刻を `timestamp_us-feedback_age_us` から推定して受信が進んだ場合のみ更新します。1記録内の複数受信フレームやログ開始前の履歴は復元できません。倍精度計算と実機float、CSV丸めの差も残ります。

従来モデルによる再帰的再構成、入力式の整合、`J*g*w-state` の恒等式、摩擦式をそれぞれ検証します。摩擦変更は `deltaInput=friction_original-friction_smooth` を同じQフィルタに通し、ログ推定値からdeltaStateを引く、ログに基準を合わせた反事実計算です。速度・モータトルクは固定しており、閉ループ抑制率ではありません。

BPFは2次連続時間帯域通過形を中心周波数でプリワープした双一次変換です。中心はsqrt(8*20)=12.649 Hz、帯域幅12 Hzで、中心の位相0度・ゲイン1です。帯域端は連続時間での半電力点で、離散化後はわずかにずれます。ノッチも中心プリワープを用い、notchDepth=.95なら中心振幅は.05。ノッチ幅は分母の帯域幅です。図5は正確な離散時間応答を示します。

RMSはBPF出力の二乗を係数 `1-exp(-dt/tau)` で一次平滑化し平方根。振幅閾値で線形ゲインを生成し、その後ゲインLPFと変化速度制限を加えます。ヒステリシスは実装せず、連続ゲインを基本としています。

## 閉ループモデルと指標

手：`-Kh*(theta-thetaHand)-Dh*w+voluntaryTorque`。基本試験は初期変位だけで開始し、12 Hzの外力注入はしません。透明性試験は別試験として0.5 Hz、0.8 Nmの随意外力を与え、Kh=0、Dh=.02を仮定します。外力振幅は静止摩擦を越えて運動が生じる出発値です。

プラント摩擦と推定摩擦は別設定です。プラントは速度予測後の摩擦インパルスを引く近接演算でスティック／スリップを扱い、粘性項は陰的に処理します。sign力を陽的Eulerでゼロ速度の両側に振らせる方法は使いません。ただしLuGre等の摩擦内部状態、弾性ヒステリシスを同定したモデルではありません。substeps=10で積分し、20への変更で収束を検証しました。

モータ指令には任意の分数サンプル遅延と一次遅れ、速度・トルクの任意量子化を実装。既定量子化は0（理想計測）。制御器は毎周期1回受信する近似で、実機の非同期受信ジッタやパケット欠落は模擬していません。トルクフィードバックは直前の実現モータトルクを観測します。

保存指標：帯域速度RMS、ピークPSD/周波数、最大速度、推定/補償トルクRMS、ピーク周波数での補償トルク対速度位相、後半/前半の帯域RMS比、制振開始時刻、追加トルクRMS、飽和率、平均仕事率・仕事・正仕事率の時間割合。閉ループ指標は `discard_s` 後、制振開始時刻は全時系列で評価します。ゼロ信号の周波数・位相はNaNとします。

追加トルクの仕事率に加え、飽和後指令の差を遅延・モータ遅れに通した `appliedExtra` の仕事率も評価します。これはアクチュエータ経路内の増分であり、別の閉ループ軌道との差ではありません。透明性は外力周波数における速度の正弦・余弦最小二乗近似から振幅/外力振幅と位相を計算します。入力振幅に依存する非線形モデル上の指標です。

基準モデルの自動判定は周波数差、帯域RMS比、振動継続比、最小振幅によるスクリーニングです。推定トルク・補償トルク・位相も実測と並べて出力しますが、判定に通っても物理モデルの同定を保証しません。

## 初期結果（2026-11-02、MATLAB R2026a実行）

共通解析長8.916秒。自由回転16.131–25.047秒、無操作1.000–9.916秒、把持1.9475–10.8635秒。

- 把持角速度PSDピーク **11.963 Hz**、8–20 Hz RMS **0.33316 rad/s**。
- RFOB再帰再構成誤差 RMS **4.8925e-7 Nm**、最大 **2.3193e-6 Nm**。
- 初期ダンピング再生：追加トルクRMS **0.03253 Nm**、平均仕事率 **−0.01061 W**、飽和率 **0.841%**（Baseline **0.908%**）。瞬間的な正仕事も約19.9%の時点に存在するため、常に受動的であるとの証明ではありません。

| tanh幅 rad/s | 摩擦10–14 Hz RMS Nm | RFOB推定10–14 Hz RMS Nm | 低速(|w|≤.1)摩擦RMS Nm |
|---:|---:|---:|---:|
| 0（従来） | .31695 | .21801 | .52550 |
| .05 | .31555 | .21690 | .37679 |
| .10 | .31112 | .21539 | .25517 |
| .20 | .29256 | .20771 | .14378 |
| .40 | .23605 | .18249 | .07452 |

幅.4で固定軌道上のRFOB帯域RMSは約16.3%減少する一方、低速の補償トルクも大きく変わります。この表から実機の振動抑制率は算出できません。

**基準閉ループモデルは約12 Hzの持続振動を再現しませんでした。** 初期変位から静止し、3秒後以降の帯域RMSは0です。4方式が0になっても制振成功という意味ではなく、初期閾値では制振ゲインもほぼ作動しません。図7–9に「baseline NOT validated」と表示します。抑制率の除算や最適値の主張は行いません。

別の随意運動試験の0.5 HzモビリティはBaseline **17.04**、摩擦平滑化 **17.57**、Notch **16.97**、Damping **16.30 (rad/s)/Nm**。この仮定モデルでは初期ダンピングによる低周波応答低下は約4.3%。ダンピングの遅延後平均追加仕事率は **−0.0191 W**、ノッチは **+0.0131 W** であり、ノッチも常に制動方向とは限りません。これらは未同定モデルの予測で、実機透明性の保証ではありません。

## スイープ・ロバスト性

```matlab
cfg = struct;
cfg.RUN_PARAMETER_SWEEP = true;
cfg.sweep.axes = {'controller.damping','controller.rmsTau_s'};
cfg.sweep.values = {0:.05:.3, .1:.05:.3};
cfg.sweep.mode = 'damping';
cfg.RUN_ROBUSTNESS_ANALYSIS = true;
cfg.robustness.Kh = [40 66.4 90];
cfg.robustness.Dh = [.02 .05 .15];
cfg.robustness.delay_s = [.0005 .001477 .003];
r = rfob_oscillation_analysis(cfg);
```

2階層のスカラー設定は同じ方法で掃引できます（full_rad_s、notchHz、notchWidthHz、notchDepth、sim.Kh、sim.Dh、sim.delay_s等）。検出帯域の両端もスイープ専用の名前 `controller.bandLowHz` / `controller.bandHighHz` で掃引できます（通常設定は `cfg.controller.bandHz=[下限 上限]`）。各設定について把持試験と随意操作試験を実行し、図10に帯域RMSとモビリティを表示します。デフォルト最大200組。閾値などの不整合は黙って修正せずエラーになります。全指標は `r.sweep.metrics` に格納します。ロバスト性は各把持パラメータ組の各方式を `r.robustness` とCSVに保存します。

## 検証と残る課題

`verify_rfob_oscillation_analysis` を実行し、実データ全基本解析・Figure 1–9/11のPNG出力、再構成・再生Baseline一致、ゲイン範囲、トルク上限、再生平均負仕事、基準未再現の正しい判定、随意入力での運動、無外力摩擦散逸、積分刻み収束、2×2スイープとFigure 10、2条件×3方式ロバスト性、不正設定の拒否を確認しました。MATLAB Code Analyzerも指摘なしです。

実機への適用前に、手の剛性・粘性、静止摩擦と微小変位の内部状態、センサ量子化と実際の更新タイミング、トルク応答の遅延・帯域を追加同定する必要があります。量子化やインピーダンスを変えた安定性と追加仕事率を調べ、基準振動の再現性を改善したうえで制振パラメータの比較を進めてください。現在の値は設計開始点であり最適値ではありません。

