#pragma once
#include <stdint.h>
#include <math.h>

// All experiment and safety settings. 0 = hardware; 1 = no-CAN dry run.
#ifndef FRICTION_NO_MOTOR
#define FRICTION_NO_MOTOR 0
#endif
constexpr bool NO_MOTOR_TEST = FRICTION_NO_MOTOR != 0;
// Require a stationary shaft before starting the initial torque sweep.
// Set false to begin the sweep as soon as fresh, valid feedback is received.
constexpr bool REQUIRE_INITIAL_STATIONARY = false;
// Documentary metadata only; these values do not configure motor protection.
constexpr bool BRAKE_VERIFIED = true;
constexpr uint32_t VERIFIED_MOTOR_TIMEOUT_MS = 0; // UNKNOWN
constexpr bool IS_60=false, IS_80A=false; // AK80 only; false selects CAN2/ID2
constexpr uint32_t CAN_PORT_NUMBER=IS_80A?1:2, MOTOR_ID=IS_80A?1:2;
constexpr uint32_t CAN_BITRATE=1000000, PERIOD_US=500, ISR_PRIORITY=192;
constexpr uint32_t FEEDBACK_TIMEOUT_US=20000, START_TIMEOUT_US=500000;
constexpr uint32_t MAX_CONTROL_GAP_US=2000, EXIT_TIMEOUT_US=100000;
constexpr float POSITION_MIN_RAD=-12.5f, POSITION_MAX_RAD=12.5f;
constexpr float VELOCITY_MAX_RAD_S=37.5f, MIT_TORQUE_MAX_NM=32;
constexpr float AK60_POLE_PAIRS=14, AK60_GEAR_RATIO=6, AK60_KT_NM_PER_A=.5994f; // unused protocol branch
constexpr float TORQUE_LIMIT_NM=1, TORQUE_STEP_NM=.02f, SPEED_LIMIT_RAD_S=20;
constexpr uint32_t MIN_SETTLE_US=100000, MAX_SETTLE_US=2000000, WINDOW_US=200000;
constexpr uint32_t CONFIRM_US=100000, ACQUIRE_US=500000;
constexpr float ACCEL_THRESHOLD=.3f, SPEED_STD_THRESHOLD=.08f;
constexpr uint32_t STATIONARY_CONFIRM_US=1000000, STATIONARY_ACQUIRE_US=100000;
constexpr float STATIONARY_SPEED=.04f, STATIONARY_POSITION_RANGE=.003f;
constexpr uint32_t STOP_WINDOW_US=1000000, STOP_TIMEOUT_US=8000000;
constexpr float STOP_POSITION_RANGE_RAD=.003f, STOP_MEAN_SPEED_RAD_S=.03f;
constexpr unsigned STOP_WINDOW_SAMPLES=STOP_WINDOW_US/PERIOD_US+1;
constexpr uint32_t RING_SIZE=4096, BATCH=128, MAX_RUN_US=900000000;
constexpr uint32_t EXPECTED_ROWS=MAX_RUN_US/PERIOD_US+2;
constexpr unsigned WINDOW_CAPACITY=WINDOW_US/PERIOD_US+4;
static_assert(TORQUE_STEP_NM>0 && TORQUE_STEP_NM<=TORQUE_LIMIT_NM && TORQUE_LIMIT_NM<=1,
              "Invalid sweep range");
static_assert(WINDOW_US>=PERIOD_US && WINDOW_US<=MAX_SETTLE_US && MIN_SETTLE_US<=MAX_SETTLE_US,
              "Invalid settling times");
static_assert(STOP_WINDOW_US%PERIOD_US==0 && STOP_WINDOW_SAMPLES==2001 && STOP_TIMEOUT_US>=STOP_WINDOW_US,
              "Invalid stop confirmation window or timeout");
static_assert(BATCH<=RING_SIZE && SPEED_LIMIT_RAD_S>0 && SPEED_LIMIT_RAD_S<VELOCITY_MAX_RAD_S,
              "Invalid buffer or speed limit");
inline constexpr uint32_t encode(float x,float lo,float hi,unsigned bits) {
  return uint32_t((fmaxf(lo,fminf(hi,x))-lo)*((1u<<bits)-1)/(hi-lo));
}
inline constexpr float decode(uint32_t x,float lo,float hi,unsigned bits) {
  return x*(hi-lo)/((1u<<bits)-1)+lo;
}
