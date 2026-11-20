#pragma once
#include <stdint.h>

namespace ControlConfig {
// User-adjustable transparency settings.
constexpr bool INITIAL_ENABLED = false;
constexpr float INITIAL_K_R = 0.7f;
constexpr float INITIAL_K_F1 = 0.0f;
constexpr float INITIAL_K_F2 = 0.0f;
constexpr float TORQUE_LIMIT = 5.0f; // RFOB output limit [Nm]

// Jia polling schedule: receive every 100 us, control/transmit every 500 us.
constexpr uint32_t CONTROL_PERIOD_US = 500;
constexpr uint32_t RX_POLL_PERIOD_US = 100;
constexpr uint8_t RX_BUDGET_PER_POLL = 4;
constexpr uint32_t CAN_BAUD = 1000000;
constexpr uint32_t INITIAL_RX_TIMEOUT_US = 500000;
constexpr uint32_t RX_MAX_AGE_US = 3000;
constexpr uint32_t TX_QUEUE_FAULT_COUNT = 2;
constexpr uint8_t ISR_PRIORITY = 96;
constexpr uint8_t CAN_IRQ_PRIORITY = 128;
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint8_t COMMAND_BUFFER_BYTES = 48;
constexpr float BIAS_STATIONARY_SPEED = 0.05f;

// Original RFOB parameters and asymmetric Stribeck friction calibration.
constexpr float Q_HZ = 400.0f;
constexpr float J = 0.0116806f;
constexpr float US_TO_SECONDS = 1.0e-6f;
constexpr float G = 6.2831853071795864769f * Q_HZ;
struct FrictionParameters { float tc, ts, vs, b; };
constexpr FrictionParameters POSITIVE = {0.459212f, 0.526341f, 0.474762f, 0.00218903f};
constexpr FrictionParameters NEGATIVE = {0.410719f, 0.527041f, 0.473325f, 0.00341074f};

// Jia MIT feedback/command ranges.
constexpr float POSITION_MIN=-12.5f, POSITION_MAX=12.5f;
constexpr float VELOCITY_MIN=-37.5f, VELOCITY_MAX=37.5f;
constexpr float TORQUE_MIN=-32.0f, TORQUE_MAX=32.0f;
constexpr float MIT_KP_MAX=500.0f, MIT_KD_MAX=5.0f;

static_assert(CONTROL_PERIOD_US == 500 && RX_POLL_PERIOD_US == 100,
              "This controller expects 2 kHz control and 10 kHz RX polling");
static_assert((CONTROL_PERIOD_US % RX_POLL_PERIOD_US) == 0,
              "RX period must divide control period");
static_assert(TORQUE_LIMIT > 0.0f && TORQUE_LIMIT <= TORQUE_MAX,
              "Invalid RFOB torque limit");
}
