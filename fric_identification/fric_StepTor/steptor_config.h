#pragma once
#include <stdint.h>

// Experiment settings (Hz, number of steps per direction, Nm, microseconds).
constexpr uint32_t SAMPLING_HZ = 500;
constexpr uint32_t STEP_COUNT = 3;
constexpr float TORQUE_STEP_NM = 1.0f;
constexpr uint32_t STEP_HOLD_US = 5000000UL;
constexpr uint32_t ZERO_HOLD_US = 2000000UL;

constexpr uint8_t MOTOR_ID = 2;
constexpr uint32_t CAN_BAUD = 1000000;
constexpr float MIT_KP = 0.0f;
constexpr float MIT_KD = 0.0f;
constexpr uint32_t FEEDBACK_TIMEOUT_US = 20000;
constexpr uint32_t START_TIMEOUT_US = 500000;
constexpr uint32_t RING_SIZE = 8192;
constexpr uint32_t WRITE_BATCH_ROWS = 256;
static_assert(SAMPLING_HZ>0 && SAMPLING_HZ<=1000000, "Invalid sampling rate");
constexpr uint32_t CONTROL_PERIOD_US = 1000000UL/SAMPLING_HZ;
static_assert(1000000UL%SAMPLING_HZ==0, "Sampling rate must divide 1000000");
static_assert(STEP_COUNT>0 && STEP_COUNT<=127, "Stage must fit uint8_t");
static_assert(STEP_HOLD_US>0, "Step hold must be positive");
constexpr uint64_t TOTAL_DURATION_US = 2ULL*STEP_COUNT*STEP_HOLD_US+ZERO_HOLD_US;
static_assert(TOTAL_DURATION_US<0x100000000ULL, "Experiment exceeds micros range");
constexpr uint32_t PREALLOCATE_ROWS = (TOTAL_DURATION_US+CONTROL_PERIOD_US-1)/CONTROL_PERIOD_US;
static_assert(WRITE_BATCH_ROWS>0 && RING_SIZE>=WRITE_BATCH_ROWS, "Invalid SD buffer settings");
