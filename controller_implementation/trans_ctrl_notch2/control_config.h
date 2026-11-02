#pragma once
#include <stdint.h>
#ifndef TRANS_ENABLE_SD_LOGGING
#define TRANS_ENABLE_SD_LOGGING 1
#endif
#ifndef TRANS_ENABLE_FIXED_NOTCH
#define TRANS_ENABLE_FIXED_NOTCH 1
#endif

namespace ControlConfig {
constexpr bool SD_LOGGING=TRANS_ENABLE_SD_LOGGING!=0;
constexpr uint32_t RX_QUEUE_ROWS=64;
constexpr uint32_t RX_BUDGET=4; // Bounded observer updates per 500 us tick.
constexpr uint32_t RX_STALE_US=2000; // Diagnostic only; timeout remains 100 ms.
constexpr uint32_t RX_INTERVAL_MIN_US=250, RX_INTERVAL_MAX_US=1000;
// ==================== USER SETTINGS / ユーザー設定 ====================
constexpr bool INITIAL_ENABLED=false; // 初期透明化制御ON/OFF [bool]
// 起動時とS開始時に以下を自動適用。KR/KFコマンドの送信は不要。
constexpr float INITIAL_K_R=0.75f; // RFOBフィードバックゲイン初期設定 [無次元]
constexpr float INITIAL_K_F=0.0f; // 摩擦FFゲイン初期設定 [無次元]
// KR/KF: 有限なfloat値のみ許可。追加のゲイン範囲制限なし。
constexpr float TORQUE_LIMIT=1.0f; // 指令トルク絶対値上限 [Nm]
constexpr uint32_t CONTROL_PERIOD_US=500; // MIT指令周期 [us]
constexpr float Q_HZ=400.0f; // RFOB Qフィルタ遮断周波数 [Hz]
constexpr float VELOCITY_HZ=1000.0f; // 角速度LPF遮断周波数 [Hz]
constexpr bool NOTCH_ENABLED=TRANS_ENABLE_FIXED_NOTCH!=0;
constexpr double NOTCH_CENTER_HZ=12.0;
constexpr double NOTCH_BANDWIDTH_HZ=6.0;
constexpr double NOTCH_DEPTH=0.95; // center amplitude = 1-depth = 0.05
enum NotchMode : uint8_t { NOTCH_OFF=0, NOTCH_FIXED=1, NOTCH_ADAPTIVE=2 };
constexpr NotchMode INITIAL_NOTCH_MODE = NOTCH_FIXED;
constexpr float ANGLE_BPF_LOW_HZ=8.0f, ANGLE_BPF_HIGH_HZ=20.0f;
constexpr float ANGLE_RMS_TAU_S=0.2f;
constexpr float ANGLE_NOTCH_START_DEG_RMS=0.1f, ANGLE_NOTCH_FULL_DEG_RMS=1.5f;
constexpr bool ENABLE_SPEED_GATE=true;
constexpr float SPEED_GATE_FULL_RAD_S=0.3f, SPEED_GATE_ZERO_RAD_S=0.5f;
constexpr float MEAN_SPEED_LPF_HZ=2.0f;
constexpr float NOTCH_ATTACK_S=0.05f, NOTCH_RELEASE_S=0.5f, NOTCH_HOLD_S=0.25f;
constexpr float ANGLE_WRAP_MIN_RAD=-12.5f, ANGLE_WRAP_MAX_RAD=12.5f;
static_assert(NOTCH_CENTER_HZ>0 && NOTCH_CENTER_HZ<500000.0/CONTROL_PERIOD_US,"Invalid notch frequency");
static_assert(NOTCH_BANDWIDTH_HZ>0 && NOTCH_DEPTH>=0 && NOTCH_DEPTH<=1,"Invalid notch bandwidth/depth");
static_assert(ANGLE_BPF_LOW_HZ>0 && ANGLE_BPF_HIGH_HZ>ANGLE_BPF_LOW_HZ && ANGLE_BPF_HIGH_HZ<500000.0/CONTROL_PERIOD_US,"Invalid angle BPF frequencies");
static_assert(ANGLE_NOTCH_FULL_DEG_RMS>ANGLE_NOTCH_START_DEG_RMS && SPEED_GATE_ZERO_RAD_S>SPEED_GATE_FULL_RAD_S,"Invalid adaptive notch thresholds");
static_assert(NOTCH_ATTACK_S>0 && NOTCH_RELEASE_S>0 && ANGLE_RMS_TAU_S>0,"Invalid adaptive notch time constants");
constexpr float OMEGA_0=0.7f; // 摩擦モデルのゼロ近傍平滑化速度 [rad/s]
constexpr float J=0.0116806f; // 出力軸換算慣性 [kg m^2]
enum FrictionModel : uint8_t { FRICTION_STRIBECK=0, FRICTION_TANH=1 };
constexpr FrictionModel FRICTION_MODEL=FRICTION_STRIBECK;
struct FrictionParameters { float tc, ts, vs, b; };
// tc: クーロン摩擦 [Nm], ts: 静止摩擦 [Nm], vs: Stribeck速度 [rad/s], b: 粘性 [Nm s/rad]
constexpr FrictionParameters POSITIVE={0.459212f,0.526341f,0.474762f,0.00218903f};
constexpr FrictionParameters NEGATIVE={0.410719f,0.527041f,0.473325f,0.00341074f};
constexpr uint8_t MOTOR_ID=2; // AK80モータID [CANノード番号]
constexpr unsigned CAN_BUS=2; // 接続CANポート [1=CAN1, 2=CAN2, 3=CAN3]
constexpr uint32_t CAN_BAUD=1000000; // CAN通信速度 [bit/s]
constexpr float POSITION_MIN=-12.5f, POSITION_MAX=12.5f; // MIT位置エンコード範囲 [rad]
constexpr float VELOCITY_MIN=-37.5f, VELOCITY_MAX=37.5f; // MIT速度エンコード範囲 [rad/s]
constexpr float TORQUE_MIN=-32.0f, TORQUE_MAX=32.0f; // MITトルクエンコード範囲 [Nm]
constexpr float MIT_KP_MAX=500.0f; // MIT内部位置ゲインのエンコード上限 [Nm/rad], 指令は0
constexpr float MIT_KD_MAX=5.0f; // MIT内部速度ゲインのエンコード上限 [Nm s/rad], 指令は0
constexpr uint32_t FEEDBACK_TIMEOUT_US=100000; // 受信開始待ち・途絶タイムアウト [us]
constexpr uint8_t ISR_PRIORITY=192; // CAN/制御タイマ共通割込み優先度 [0..255]
constexpr uint32_t SERIAL_BAUD=115200; // USBシリアル表示用ボーレート [bit/s]
constexpr float BIAS_STATIONARY_SPEED=0.05f; // OFF中bias取得時の静止判定 [rad/s]（制御デッドバンドではない）
constexpr uint32_t LOG_PERIOD_US=CONTROL_PERIOD_US; // microSDサンプル記録周期 [us], 制御周期の整数倍
constexpr uint32_t LOG_RING_ROWS=2048; // ISR→SDリング容量 [行]
constexpr uint32_t LOG_BATCH_ROWS=64; // 152-byte records; batch staging is serviced outside the ISR
constexpr unsigned LOG_FILE_COUNT=10000; // TC0000.CSV～の最大ファイル数 [個]
constexpr unsigned CSV_BUFFER_BYTES=768; // 39列CSV行整形・転送バッファ [byte]
constexpr unsigned COMMAND_BUFFER_BYTES=48; // シリアルコマンド最大バッファ [byte]
// ==================== DERIVED CONSTANTS / 設定から算出 ====================
constexpr float RADIANS_PER_REVOLUTION=6.283185307179586f; // 円周率の2倍 [rad/rev]
constexpr float US_TO_SECONDS=1.0e-6f; // 時刻単位換算 [s/us]
constexpr float G=RADIANS_PER_REVOLUTION*Q_HZ; // RFOB帯域 [rad/s]
constexpr uint32_t LOG_DECIMATION=LOG_PERIOD_US/CONTROL_PERIOD_US; // 記録間引き比 [制御回/行]
static_assert(CONTROL_PERIOD_US>0 && LOG_PERIOD_US>=CONTROL_PERIOD_US && LOG_PERIOD_US%CONTROL_PERIOD_US==0,"Invalid log/control periods");
static_assert(CAN_BUS>=1 && CAN_BUS<=3,"Invalid CAN bus");
static_assert(LOG_RING_ROWS>=LOG_BATCH_ROWS && LOG_BATCH_ROWS>0,"Invalid log ring");
static_assert(LOG_DECIMATION==1,"Keep every 2 kHz sample");
static_assert(TORQUE_LIMIT>0 && TORQUE_LIMIT<=TORQUE_MAX && -TORQUE_LIMIT>=TORQUE_MIN,"Invalid torque limit");
}
