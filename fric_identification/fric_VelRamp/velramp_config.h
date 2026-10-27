#pragma once
#include <stdint.h>

// AK80 MIT velocity control. ID 1 uses CAN1; ID 2 uses CAN2.
constexpr uint8_t MOTOR_ID = 2;
constexpr uint32_t CAN_BAUD = 1000000;
constexpr uint32_t CONTROL_PERIOD_US = 500; // 2 kHz
constexpr float MIT_KP = 0.0f;
constexpr float MIT_KD = 2.0f;
constexpr float SPEED_KI_NM_PER_RAD = 0.2f;
constexpr float MAX_I_TORQUE_NM = 1.0f;
constexpr float MAX_ABS_SPEED_RAD_S = 20.0f;
constexpr uint32_t FEEDBACK_TIMEOUT_US = 20000;
constexpr uint32_t START_TIMEOUT_US = 500000;
constexpr uint32_t RING_SIZE = 8192;
constexpr uint32_t WRITE_BATCH_ROWS = 256;
constexpr uint32_t PREALLOCATE_ROWS = 240000; // 120 s at 2 kHz
constexpr float SPEED_STEP_RAD_S = 0.1f;
constexpr uint32_t SLOW_STEP_US = 1000000;
constexpr uint32_t FAST_STEP_US = 200000;
