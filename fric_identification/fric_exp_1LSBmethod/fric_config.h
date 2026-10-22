#pragma once
#include <stdint.h>
#include <math.h>

// AK80-8 / Teensy 4.1 fixed hardware profile.
constexpr uint32_t MOTOR_ID = 2, CAN_BITRATE = 1000000, CAN_PORT_NUMBER = 2;
constexpr uint32_t PERIOD_US = 500, ISR_PRIORITY = 192;
// MIT's 16-bit position field is encoded over this range. This is a wire
// representation range, not an absolute experiment position limit.
constexpr float POSITION_MIN_RAD = -12.5f, POSITION_MAX_RAD = 12.5f;
constexpr float POSITION_WRAP_PERIOD_RAD = POSITION_MAX_RAD - POSITION_MIN_RAD;
constexpr float VELOCITY_MAX_RAD_S = 37.5f, MIT_TORQUE_MAX_NM = 32.0f;
constexpr float TORQUE_LIMIT_NM = 1.0f;
constexpr float TORQUE_QUANTUM_NM = 64.0f / 4095.0f;
constexpr uint32_t TORQUE_ZERO_COUNT = 2047;
constexpr float TORQUE_LOW_REAL_COUNT = (MIT_TORQUE_MAX_NM-TORQUE_LIMIT_NM)*4095.0f/(2.0f*MIT_TORQUE_MAX_NM);
constexpr uint32_t TORQUE_MIN_COUNT_NEG = uint32_t(TORQUE_LOW_REAL_COUNT)+(float(uint32_t(TORQUE_LOW_REAL_COUNT))<TORQUE_LOW_REAL_COUNT);
constexpr uint32_t TORQUE_MAX_COUNT_POS = uint32_t((MIT_TORQUE_MAX_NM+TORQUE_LIMIT_NM)*4095.0f/(2.0f*MIT_TORQUE_MAX_NM));
constexpr uint32_t LOW_RAMP_US = 100000;
constexpr float LOW_SPEED_BOUNDARY_RAD_S = 1.0f;
constexpr uint32_t TRANSITION_WINDOW_US = 50000;
constexpr uint32_t STEADY_WINDOW_US = 1000000, MAX_STEP_WAIT_US = 3000000;
constexpr float STEADY_ACCEL_LIMIT = 0.3f, STEADY_SPEED_STD_LIMIT = 0.08f;
constexpr uint32_t FEEDBACK_TIMEOUT_US = 20000, MAX_RUN_US = 900000000;
constexpr float NORMAL_SPEED_LIMIT_RAD_S = 15.0f, EMERGENCY_SPEED_RAD_S = 20.0f;
constexpr uint32_t STOP_WINDOW_US = 1000000, STOP_TIMEOUT_US = 8000000;
constexpr uint32_t DISABLE_TIMEOUT_US = 100000;
constexpr float STOP_POSITION_RANGE_RAD = 0.003f, STOP_MEAN_SPEED_RAD_S = 0.03f;
constexpr uint32_t RING_SIZE = 4096, BATCH_ROWS = 32, SD_SYNC_US = 1000000;
constexpr uint32_t POSITION_JUMP_TOLERANCE_RAD_X1000 = 50;
static_assert(TORQUE_ZERO_COUNT >= TORQUE_MIN_COUNT_NEG && TORQUE_ZERO_COUNT <= TORQUE_MAX_COUNT_POS, "invalid torque counts");
static_assert(TORQUE_LIMIT_NM>0&&TORQUE_LIMIT_NM<=MIT_TORQUE_MAX_NM,"invalid torque limit");

enum RunState : uint8_t { IDLE, WAIT_FEEDBACK, SWEEPING, STOPPING, STOPPED };
enum ResultCode : uint8_t { RESULT_NONE, SETTLED, NOT_SETTLED, SAFETY_STOP, NORMAL_END };
enum EventCode : uint8_t { EVENT_NONE, STEP_START, STEP_SETTLED, STEP_TIMEOUT, PHASE_CHANGE, STOP_CONFIRMED, STOP_TIMEOUT, ABORT_EVENT, SAFETY_EVENT, SPEED_LIMIT_STOP, DISABLE_TX_UNCONFIRMED };

// Keep this fixed-size record safe to copy in the 2 kHz timer ISR.
struct Sample {
  uint32_t timeUs, feedbackSequence, feedbackAgeUs;
  uint16_t stepIndex, torqueCount;
  uint8_t phase, mode, eventCode, stepResult, motorError, feedbackFresh;
  float targetTorqueNm, feedbackTorqueNm, speedRadS, positionDeg;
  float rawPositionRad, unwrappedPositionRad;
  float meanSpeedRadS, meanFeedbackTorqueNm, speedStdRadS, torqueStdNm, accelerationRadS2, steadyWindowS;
  uint16_t validFeedbackCount, stopWindowSampleCount;
  uint8_t commandAccepted, positionRebased, disableTxCompleted;
};
static_assert(sizeof(Sample) <= 80, "sample size exceeds ring budget");
