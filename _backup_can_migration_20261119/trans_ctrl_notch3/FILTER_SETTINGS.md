# ノッチ設定（MATLABによる係数事前生成が必要）

変更するのはcontrol_config.hだけです。保存後、Arduino IDEで通常どおりコンパイル／アップロードしてください。中心周波数、低側幅、高側幅、Depth、制御周期から、電源投入時の初期化で一度だけ係数を求めます。notch_coefficients.hは生成データではなく設計処理のC++実装です。

低側の目標点=center-low、高側の目標点=center+highです。幅そのものを端の周波数として指定しないでください。

MATLAB設計の目的関数、Nelder-Mead探索、非対称設計の採用条件、既存のRBJ近似への切替、10%の境界周波数誤差制限を移植しています。任意の設定を必ず実現できるわけではありません。実現不能・不安定・増幅などの条件で不採用になった場合は、FixedNotchのfiniteState()をfalseにして既存の数値異常停止処理に渡します。古い係数や無断で緩めた条件で運転しません。

計算はコンストラクタから一度だけ呼び出し、結果をキャッシュします。reset()/update()および2 kHz制御割込みでは係数探索・係数生成用の三角関数を呼びません。初期化中に約64 kBの一時スタックを使い、探索のため起動待ち時間が増えます。実機起動時間は別途測定が必要です。

verification/design_asymmetric_notch.mは任意のオフライン確認用です。MATLABからC++ヘッダを書き換える処理は削除しました。design_and_build.ps1は互換用の入口で、通常はMATLABを使わずビルドします。-DesignOnly指定時だけMATLABでオフライン確認します。

最新の検証はverification/runtime_comparison.txtを参照。従来のasymmetric_*結果は過去のオフライン設計記録です。

