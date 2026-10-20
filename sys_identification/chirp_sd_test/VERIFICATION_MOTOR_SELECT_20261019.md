# MOTOR_SELECT切替追加の検証（2026-10-19）

## 選択

`chirp_config.h`先頭の`constexpr char MOTOR_SELECT[] = "80B";`から、CAN・ID・IRQ・全レジスタ・受信フィルタ・送受信形式・換算・起動表示を導出する。

- 80A：CAN1 / ID1 / 標準MIT / ゼロ→FD。
- 80B：CAN2 / ID2 / 標準MIT / ゼロ→FD。
- 60：CAN3 / ID3 / 拡張MIT TX0x803・RX0x2903 / ゼロ継続（FDを送らない）。

AK80の±37.5 rad/s・±32 N·mは維持。AK60は既存AK80_timer_test/Mode1_Motor3の範囲と換算（位置±12.56、指令速度±60、トルク±12、応答位置0.1deg、速度10ERPM/14極対/減速比6、電流0.01A×公称0.5994Nm/A）を流用。

選択先のモータ自身に設定されたIDを書き換える機能ではない。実機は表のID・形式に設定されている必要がある。AK60のゼロ継続はモード解除・ブレーキ保持ではない。

## 形式と互換性

CHIRPSD5：Header144 / Sample64 / Footer20 bytes。CANPortとMotorProtocolを追加保存。正常StopFlagsはAK80=15、AK60=3、非駆動=16。AK60の符号付き電流生値はuint16ビット列として記録し、PCは0～65535を受理する。PC変換・Serial取得は旧SD1/2/3/4も維持。Teensyのdは現行SD5のみ。

## 机上テスト

`python -m unittest discover -s sys_identification -p "test_chirp*.py"`：16件成功。

- 従来の加振・整定・中断・破損検証。
- 実際のARM C++コンパイラのconstexpr検査で、全3選択のkp/kdゼロ配置、TX ID/拡張フラグ、RX ID・DLC・RTR不一致拒否、AK60の符号付き位置/速度/負電流換算を検証。
- 未定義MOTOR_SELECTのコンパイル拒否。
- SD5の3選択についてBIN/Serial変換、保存プロファイル、ID1/2/3、AK60停止フラグ3、停止未確認拒否を確認。

Teensy 4.1 / core1.62.0 / SdFat2.1.2 / 同梱FlexCAN_T4で、80A・80B・60の実駆動設定と80Bの非駆動設定をコンパイル確認。FlexCAN_T4内部setMBの既存警告は残る。

## 現在の設定・実機確認

MOTOR_SELECT=80B、CHIRP_NO_MOTOR=0、振幅1→2.5 N·mのユーザー設定を維持した。今回の変更版は実機へ書き込んでいない。80Bの従来版はユーザーが中断・停止TX完了まで確認しているが、新しい切替版・80A/60の実機通信や安定性を確認したという意味ではない。原点移動・整定・40秒加振・前後保持・SDリング・まとめ書きの設計は維持した。
