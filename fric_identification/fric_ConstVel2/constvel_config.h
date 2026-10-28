
#pragma once
#include <stdint.h>

// AK80 MIT constant-velocity test on CAN2 / motor ID 2.
constexpr uint8_t MOTOR_ID = 2;
constexpr uint32_t CAN_BAUD = 1000000;
constexpr uint32_t CONTROL_PERIOD_US = 500; // 2 kHz

// MIT mode: external PI control on Teensy
constexpr float MIT_KP = 0.0f;
constexpr float MIT_KD = 0.0f;

// Speed PI gains (pole placement: fn=10 Hz, zeta=1.0)
constexpr float SPEED_KP_NM_PER_RAD_S = 1.44566f;
constexpr float SPEED_KI_NM_PER_RAD = 46.112f;

constexpr float MAX_I_TORQUE_NM = 1.0f;
constexpr float MAX_ABS_SPEED_RAD_S = 20.0f;

// Set the signed target velocity and hold duration for each acquisition.
constexpr float TARGET_SPEED_RAD_S = 8.0f;
constexpr uint32_t HOLD_DURATION_US = 10000000UL; // 30 s

constexpr uint32_t FEEDBACK_TIMEOUT_US = 20000;
constexpr uint32_t START_TIMEOUT_US = 500000;
constexpr uint32_t RING_SIZE = 8192;
constexpr uint32_t WRITE_BATCH_ROWS = 256;
constexpr uint32_t PREALLOCATE_ROWS = 240000; // 120 s at 2 kHz
