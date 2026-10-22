#pragma once
#include <stdio.h>
#include "friction_config.h"
inline size_t makeConfig(char *configJson, size_t capacity) {
 size_t n=0;
 n+=snprintf(configJson+n,capacity-n,"{\"format\":\"FRIC001\"");
 n+=snprintf(configJson+n,capacity-n,",\"NO_MOTOR_TEST\":%.9g",double(NO_MOTOR_TEST));
 n+=snprintf(configJson+n,capacity-n,",\"REQUIRE_INITIAL_STATIONARY\":%.9g",double(REQUIRE_INITIAL_STATIONARY));
 n+=snprintf(configJson+n,capacity-n,",\"BRAKE_VERIFIED\":%.9g",double(BRAKE_VERIFIED));
 n+=snprintf(configJson+n,capacity-n,",\"VERIFIED_MOTOR_TIMEOUT_MS\":%.9g",double(VERIFIED_MOTOR_TIMEOUT_MS));
 n+=snprintf(configJson+n,capacity-n,",\"IS_60\":%.9g",double(IS_60));
 n+=snprintf(configJson+n,capacity-n,",\"IS_80A\":%.9g",double(IS_80A));
 n+=snprintf(configJson+n,capacity-n,",\"CAN_PORT_NUMBER\":%.9g",double(CAN_PORT_NUMBER));
 n+=snprintf(configJson+n,capacity-n,",\"MOTOR_ID\":%.9g",double(MOTOR_ID));
 n+=snprintf(configJson+n,capacity-n,",\"CAN_BITRATE\":%.9g",double(CAN_BITRATE));
 n+=snprintf(configJson+n,capacity-n,",\"PERIOD_US\":%.9g",double(PERIOD_US));
 n+=snprintf(configJson+n,capacity-n,",\"ISR_PRIORITY\":%.9g",double(ISR_PRIORITY));
 n+=snprintf(configJson+n,capacity-n,",\"FEEDBACK_TIMEOUT_US\":%.9g",double(FEEDBACK_TIMEOUT_US));
 n+=snprintf(configJson+n,capacity-n,",\"START_TIMEOUT_US\":%.9g",double(START_TIMEOUT_US));
 n+=snprintf(configJson+n,capacity-n,",\"MAX_CONTROL_GAP_US\":%.9g",double(MAX_CONTROL_GAP_US));
 n+=snprintf(configJson+n,capacity-n,",\"EXIT_TIMEOUT_US\":%.9g",double(EXIT_TIMEOUT_US));
 n+=snprintf(configJson+n,capacity-n,",\"POSITION_MIN_RAD\":%.9g",double(POSITION_MIN_RAD));
 n+=snprintf(configJson+n,capacity-n,",\"POSITION_MAX_RAD\":%.9g",double(POSITION_MAX_RAD));
 n+=snprintf(configJson+n,capacity-n,",\"VELOCITY_MAX_RAD_S\":%.9g",double(VELOCITY_MAX_RAD_S));
 n+=snprintf(configJson+n,capacity-n,",\"MIT_TORQUE_MAX_NM\":%.9g",double(MIT_TORQUE_MAX_NM));
 n+=snprintf(configJson+n,capacity-n,",\"AK60_POLE_PAIRS\":%.9g",double(AK60_POLE_PAIRS));
 n+=snprintf(configJson+n,capacity-n,",\"AK60_GEAR_RATIO\":%.9g",double(AK60_GEAR_RATIO));
 n+=snprintf(configJson+n,capacity-n,",\"AK60_KT_NM_PER_A\":%.9g",double(AK60_KT_NM_PER_A));
 n+=snprintf(configJson+n,capacity-n,",\"TORQUE_LIMIT_NM\":%.9g",double(TORQUE_LIMIT_NM));
 n+=snprintf(configJson+n,capacity-n,",\"TORQUE_STEP_NM\":%.9g",double(TORQUE_STEP_NM));
 n+=snprintf(configJson+n,capacity-n,",\"SPEED_LIMIT_RAD_S\":%.9g",double(SPEED_LIMIT_RAD_S));
 n+=snprintf(configJson+n,capacity-n,",\"MIN_SETTLE_US\":%.9g",double(MIN_SETTLE_US));
 n+=snprintf(configJson+n,capacity-n,",\"MAX_SETTLE_US\":%.9g",double(MAX_SETTLE_US));
 n+=snprintf(configJson+n,capacity-n,",\"WINDOW_US\":%.9g",double(WINDOW_US));
 n+=snprintf(configJson+n,capacity-n,",\"CONFIRM_US\":%.9g",double(CONFIRM_US));
 n+=snprintf(configJson+n,capacity-n,",\"ACQUIRE_US\":%.9g",double(ACQUIRE_US));
 n+=snprintf(configJson+n,capacity-n,",\"ACCEL_THRESHOLD\":%.9g",double(ACCEL_THRESHOLD));
 n+=snprintf(configJson+n,capacity-n,",\"SPEED_STD_THRESHOLD\":%.9g",double(SPEED_STD_THRESHOLD));
 n+=snprintf(configJson+n,capacity-n,",\"STATIONARY_CONFIRM_US\":%.9g",double(STATIONARY_CONFIRM_US));
 n+=snprintf(configJson+n,capacity-n,",\"STATIONARY_ACQUIRE_US\":%.9g",double(STATIONARY_ACQUIRE_US));
 n+=snprintf(configJson+n,capacity-n,",\"STATIONARY_SPEED\":%.9g",double(STATIONARY_SPEED));
 n+=snprintf(configJson+n,capacity-n,",\"STATIONARY_POSITION_RANGE\":%.9g",double(STATIONARY_POSITION_RANGE));
 n+=snprintf(configJson+n,capacity-n,",\"STOP_WINDOW_US\":%.9g",double(STOP_WINDOW_US));
 n+=snprintf(configJson+n,capacity-n,",\"STOP_WINDOW_SAMPLES\":%.9g",double(STOP_WINDOW_SAMPLES));
 n+=snprintf(configJson+n,capacity-n,",\"STOP_POSITION_RANGE_RAD\":%.9g",double(STOP_POSITION_RANGE_RAD));
 n+=snprintf(configJson+n,capacity-n,",\"STOP_MEAN_SPEED_RAD_S\":%.9g",double(STOP_MEAN_SPEED_RAD_S));
 n+=snprintf(configJson+n,capacity-n,",\"STOP_TIMEOUT_US\":%.9g",double(STOP_TIMEOUT_US));
 n+=snprintf(configJson+n,capacity-n,",\"RING_SIZE\":%.9g",double(RING_SIZE));
 n+=snprintf(configJson+n,capacity-n,",\"BATCH\":%.9g",double(BATCH));
 n+=snprintf(configJson+n,capacity-n,",\"MAX_RUN_US\":%.9g",double(MAX_RUN_US));
 n+=snprintf(configJson+n,capacity-n,",\"EXPECTED_ROWS\":%.9g",double(EXPECTED_ROWS));
 n+=snprintf(configJson+n,capacity-n,",\"WINDOW_CAPACITY\":%.9g",double(WINDOW_CAPACITY));
 n+=snprintf(configJson+n,capacity-n,"}"); return n;
}
