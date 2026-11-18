#pragma once
#include <stdint.h>

// ========== MOTOR SELECTION ==========
constexpr bool MOTOR1_ON = true;   // CAN1 / Motor ID1
constexpr bool MOTOR2_ON = false;  // CAN2 / Motor ID2

// ========== TEST SETTINGS ==========
constexpr uint32_t COMMAND_PERIOD_US = 500;
constexpr uint32_t PRINT_INTERVAL_MS = 1000;
constexpr uint32_t RX_WARNING_US = 5000;
constexpr uint32_t RX_TIMEOUT_US = 500000;

static_assert(MOTOR1_ON || MOTOR2_ON, "Enable at least one motor");
constexpr uint32_t CAN_BAUD = 1000000;
constexpr uint8_t MOTOR_ID[2] = {1, 2};
constexpr uint32_t RX_QUEUE_SIZE = 64;
constexpr uint16_t HISTORY_SIZE = 200; // 2 seconds at 10 ms/sample
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint8_t ISR_PRIORITY = 192;

constexpr float POSITION_MIN = -12.5f, POSITION_MAX = 12.5f;
constexpr float VELOCITY_MIN = -37.5f, VELOCITY_MAX = 37.5f;
constexpr float TORQUE_MIN = -32.0f, TORQUE_MAX = 32.0f;
constexpr float MIT_KP_MAX = 500.0f, MIT_KD_MAX = 5.0f;

inline constexpr bool motorOn(uint8_t i) { return i == 0 ? MOTOR1_ON : (i == 1 ? MOTOR2_ON : false); }
