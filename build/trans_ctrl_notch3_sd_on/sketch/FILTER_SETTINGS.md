#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch3\\FILTER_SETTINGS.md"
# configからフィルタを変更する

1. control_config.hのNOTCH_CENTER_HZ（中心）、NOTCH_LOW_SIDE_HZ（中心から低側への幅）、NOTCH_HIGH_SIDE_HZ（中心から高側への幅）を編集する。
2. このフォルダでPowerShellを開き、次を実行する。

```powershell
.\design_and_build.ps1
```

MATLABで係数とゲイン・位相図を再生成し、Teensy 4.1をSD有効／無効でビルドする。アップロードは行わない。MATLABの場所が違う場合は-Matlabでmatlab.exeのパスを指定する。Arduino IDEを使用する場合は、先に `.\design_and_build.ps1 -DesignOnly` を実行してから通常のビルド／アップロードを行う。Arduino IDEだけでは係数を自動再生成しない。

数値はHz単位のリテラルで設定する（例12.0）。中心-低側幅>0、中心+高側幅<サンプリング周波数/2が必要。Depthが浅すぎて−3 dB点を持たない設定は拒否する。

2次IIRの非対称設計を試み、条件を満たさない場合は同じ2次のRBJ近似へ切り替える。この近似では左右の幅を指定どおりには実現できない。各−3 dB点の周波数誤差が目標の10%以内で、極が安定、DC利得1、周波数走査で増幅なしの場合のみ係数を保存する。条件外ではビルドへ進まず、古い係数と新設定の不一致はstatic_assertで検出する。任意の設定を必ず実現できるわけではない。

最新結果はverification/asymmetric_notch_results.txtとasymmetric_notch_response.png。以前のASYMMETRIC_NOTCH_REPORT.mdは低側幅4／高側幅20の旧設計の記録。

今回の設定は中心12、低側幅8、高側幅22 Hz。目標4/34 Hzに対して、実現4.210223654/34.177190785 Hz（誤差+0.210223654/+0.177190785 Hz）。RBJ近似を採用。最小点12 Hz、−26.020599913 dB、極半径0.981324246984と0.927332663771、最大利得1。制御アルゴリズム、Adaptive判定、摩擦モデルは変更していない。
