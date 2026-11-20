# trans_ctrl_notch1

Teensy 4.1 / AK80-8 ID2 / CAN2 / 1 Mbps、固定12 Hzノッチ付きRFOB透明化制御。
実装詳細・設定・検証・未確認事項は [REPORT_ja.md](REPORT_ja.md) を参照。

設定はcontrol_config.h。初期OFF、Kr=0.75、Kf=0、上限1.0 Nm、周期500 us。
速度LPF1000 Hz、RFOB Q400 Hz、固定ノッチ12 Hz / 幅4 Hz / 深さ0.95。

USBシリアル115200 bps、改行付きで `S`（新規運転、OFF）→ `ON`。
`OFF`はゼロトルクで観測・ノッチ・ログを継続。`X`は即時停止・MIT退出・ログ確定。
`STATUS`、`KR <値>`、`KF <値>`、`BIAS`、停止後の`D`は既存と同じ。
ON/OFFによる指令の段差は残る。実機書込み・動作検証は未実施。

ビルド: PowerShellで `./build_verify.ps1`。Teensy公式コア1.60.0を使用。
SDカード上のBIN変換: `python convert_trans_log.py <BINファイル>`。
オフライン参照計算: `python verify.py --python-only`（NumPyが必要）。
