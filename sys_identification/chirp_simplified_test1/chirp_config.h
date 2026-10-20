#pragma once
#include <stdint.h>
#include <math.h>
#include <float.h>

// Select hardware here: "80A" = CAN1/ID1, "80B" = CAN2/ID2, "60" = CAN3/ID3.
constexpr char MOTOR_SELECT[] = "80B";
constexpr bool sameText(const char *a, const char *b) {
  return *a == *b && (*a == '\0' || sameText(a+1,b+1));
}
constexpr bool IS_80A = sameText(MOTOR_SELECT,"80A");
constexpr bool IS_80B = sameText(MOTOR_SELECT,"80B");
constexpr bool IS_60 = sameText(MOTOR_SELECT,"60");
static_assert(IS_80A || IS_80B || IS_60, "MOTOR_SELECT must be 80A, 80B or 60");
constexpr uint32_t CAN_PORT_NUMBER = IS_60 ? 3 : (IS_80A ? 1 : 2);
constexpr uint32_t MOTOR_ID = IS_60 ? 3 : (IS_80A ? 1 : 2);
constexpr uint32_t MOTOR_PROTOCOL = IS_60 ? 1 : 0; // 0=AK80 standard, 1=AK60 extended.
constexpr uint32_t CAN_BITRATE = 1000000;

// Experiment conditions: edit these seven values together with the hold times.
constexpr double SAMPLING_FREQUENCY_HZ = 2000.0;
constexpr double SWEEP_DURATION_S = 20.0;
constexpr double START_FREQUENCY_HZ = 1.0;
constexpr double END_FREQUENCY_HZ = 100.0;
constexpr double START_AMPLITUDE_NM = 1.5; // Default 1.0
constexpr double END_AMPLITUDE_NM = 10.0; // Default 2.5
constexpr double AMPLITUDE_EXPONENT = 1.0;
constexpr double PRE_HOLD_DURATION_S = 2.0;
constexpr double POST_HOLD_DURATION_S = 1.0;

// P: virtual spring about the average position measured during zero-torque startup.
// 0.00278 Nm/deg gives about 1 Nm at 360 deg; this trial uses three times that.
// 0.00834 Nm/deg gives 3.0024 Nm at 360 deg, 1.5012 Nm at 180 deg,
// and about 0.478 Nm/rad. Do not mix deg and rad.
// About +/- one turn is intended; this gain does NOT enforce a rotation range.
constexpr float KP_PD = 0.00278f *8.0f; // [Nm/deg]
// D: virtual damper opposing measured rotation; fixed target => target velocity 0.
// 0.1 Nm/(rad/s): 0.1 Nm at 1 rad/s, 1 Nm at 10 rad/s,
// about 0.628 Nm at one revolution/s. Initial tuning value only:
// neither optimal tuning nor stability on the real machine is guaranteed.
constexpr float KD_PD = 0.025f; // [Nm/(rad/s)]
constexpr float TORQUE_LIMIT_NM = 15.0f; // Experiment limit; exceeding it ABORTS.

// AK80 profiles retain the requested legacy AK80-8 KV60 ranges;
// AK60 uses the existing AK80_timer_test / Mode1_Motor3 profile.
// Ranges match manual v1.0.18 p.63 and Jia motor1ak80. Boundary behavior
// and the motor-side timeout remain unverified on the physical motor.
// Never mix this +/-37.5 rad/s profile with the +/-45 rad/s implementation.
constexpr float POSITION_MIN_RAD = IS_60 ? -12.56f : -12.5f, POSITION_MAX_RAD = -POSITION_MIN_RAD;
constexpr float VELOCITY_MAX_RAD_S = IS_60 ? 60.0f : 37.5f;
constexpr float MIT_TORQUE_MAX_NM = IS_60 ? 12.0f : 32.0f; // Encoding range, NOT experiment limit.
// AK60 conversions copied from AK80_timer_test and integrated Mode1_Motor3.
// Nominal estimate only, NOT an external torque sensor measurement.
constexpr float AK60_POLE_PAIRS = 14.0f, AK60_GEAR_RATIO = 6.0f;
constexpr float AK60_KT_NM_PER_A = 0.5994f;
#ifndef CHIRP_NO_MOTOR
#define CHIRP_NO_MOTOR 0
#endif
constexpr bool NO_MOTOR_TEST = CHIRP_NO_MOTOR != 0;
// Documentary evidence, not a claim of physical-machine validation.
constexpr bool MOTOR_PROFILE_DOCUMENTED = true; // Manual/project evidence, not physical-machine validation.
// Record actual motor-side timeout after verification; 0 means UNKNOWN, not disabled.
constexpr uint32_t VERIFIED_MOTOR_TIMEOUT_MS = 0;

// PRE_HOLD_DURATION_S is a ZERO TORQUE averaging interval, with PD disabled.
// No return-to-zero or settling requirement. Never send encoder-origin reset.
constexpr uint32_t MIN_REFERENCE_SAMPLES = 2;

// Logging/implementation settings. SI1 uses 64-byte rows.
constexpr uint32_t RING_SIZE = 4096, BATCH = 128;
constexpr uint8_t ISR_PRIORITY = 192; // CAN and timer: same nonpreempting priority.
constexpr uint32_t FEEDBACK_TIMEOUT_US = 20000, START_TIMEOUT_US = 500000;
constexpr uint32_t STOP_TIMEOUT_US = 100000;
constexpr float POSITION_JUMP_TOLERANCE_RAD = 0.05f;
constexpr float POSITION_BOUNDARY_MARGIN_RAD = 0.05f;

static_assert(SAMPLING_FREQUENCY_HZ > 0 && SAMPLING_FREQUENCY_HZ <= 1000000, "Invalid fs");
constexpr double SAMPLING_PERIOD_S = 1.0 / SAMPLING_FREQUENCY_HZ;
constexpr double TIMER_PERIOD_US = 1000000.0 / SAMPLING_FREQUENCY_HZ;
constexpr uint32_t PERIOD_US = static_cast<uint32_t>(TIMER_PERIOD_US);
static_assert(PERIOD_US == TIMER_PERIOD_US && PERIOD_US > 0, "Period must be integer us");
constexpr double SWEEP_STEPS = SWEEP_DURATION_S * SAMPLING_FREQUENCY_HZ;
constexpr double PRE_STEPS_VALUE = PRE_HOLD_DURATION_S * SAMPLING_FREQUENCY_HZ;
constexpr double POST_STEPS_VALUE = POST_HOLD_DURATION_S * SAMPLING_FREQUENCY_HZ;
static_assert(SWEEP_STEPS >= 1 && SWEEP_STEPS < UINT32_MAX &&
              PRE_STEPS_VALUE >= 0 && PRE_STEPS_VALUE < UINT32_MAX &&
              POST_STEPS_VALUE >= 0 && POST_STEPS_VALUE < UINT32_MAX, "Invalid duration");
constexpr uint32_t STEPS = static_cast<uint32_t>(SWEEP_STEPS);
constexpr uint32_t PRE_STEPS = static_cast<uint32_t>(PRE_STEPS_VALUE);
constexpr uint32_t POST_STEPS = static_cast<uint32_t>(POST_STEPS_VALUE);
static_assert(STEPS == SWEEP_STEPS && PRE_STEPS == PRE_STEPS_VALUE &&
              POST_STEPS == POST_STEPS_VALUE, "Durations must contain integer steps");
constexpr uint64_t TOTAL_STEPS_64 = uint64_t(PRE_STEPS) + STEPS + POST_STEPS;
static_assert(TOTAL_STEPS_64 * PERIOD_US < UINT32_MAX && TOTAL_STEPS_64 < UINT32_MAX,
              "Recording exceeds uint32 range");
constexpr uint32_t TOTAL_STEPS = TOTAL_STEPS_64;
constexpr uint32_t EXPERIMENT_ROWS = TOTAL_STEPS + 1;
static_assert(PRE_STEPS > 0, "Reference averaging interval must be positive");
// Allow startup timer phase jitter while waiting at least 2 seconds wall-clock.
constexpr uint32_t MAX_AVERAGE_EXTRA_STEPS = 4;
constexpr uint32_t EXPECTED_ROWS = EXPERIMENT_ROWS + MAX_AVERAGE_EXTRA_STEPS;
static_assert(uint64_t(EXPECTED_ROWS)*4*PERIOD_US < UINT32_MAX, "Timestamp overflow");
constexpr uint32_t DURATION_US = STEPS * PERIOD_US;
constexpr uint32_t MAX_CONTROL_GAP_US = 4 * PERIOD_US;
static_assert(0 < START_FREQUENCY_HZ && START_FREQUENCY_HZ < END_FREQUENCY_HZ &&
              END_FREQUENCY_HZ < SAMPLING_FREQUENCY_HZ / 2, "Invalid sweep frequencies");
static_assert(START_AMPLITUDE_NM >= 0 && START_AMPLITUDE_NM <= FLT_MAX &&
              END_AMPLITUDE_NM >= 0 && END_AMPLITUDE_NM <= FLT_MAX &&
              AMPLITUDE_EXPONENT > 0 && AMPLITUDE_EXPONENT <= FLT_MAX, "Invalid amplitude");
static_assert(TORQUE_LIMIT_NM > 0 && TORQUE_LIMIT_NM <= MIT_TORQUE_MAX_NM, "Invalid torque limit");
static_assert(KP_PD >= 0 && KP_PD <= FLT_MAX && KD_PD >= 0 && KD_PD <= FLT_MAX, "Invalid PD gains");
static_assert(BATCH > 0 && BATCH <= RING_SIZE, "Invalid ring settings");
const double CHIRP_K = log(END_FREQUENCY_HZ / START_FREQUENCY_HZ) / SWEEP_DURATION_S;

// Same amplitude and phase equations as the original calculateChirp().
inline float calculateChirp(double t) {
  if (t < 0 || t >= SWEEP_DURATION_S) return 0;
  const double growth = expm1(CHIRP_K * t);
  const double frequency = START_FREQUENCY_HZ * (1 + growth);
  const double fraction = fmax(0.0, fmin(1.0,
      (frequency - START_FREQUENCY_HZ) / (END_FREQUENCY_HZ - START_FREQUENCY_HZ)));
  const double amplitude = START_AMPLITUDE_NM +
      (END_AMPLITUDE_NM - START_AMPLITUDE_NM) * pow(fraction, AMPLITUDE_EXPONENT);
  const double phase = 6.2831853071795864769 * START_FREQUENCY_HZ * growth / CHIRP_K;
  return amplitude * sin(phase);
}

enum Phase : uint32_t { PRE_HOLD = 0, SWEEP = 1, POST_HOLD = 2, STOP_ROW = 3, AVERAGE_PHASE = PRE_HOLD };
enum RunState : uint32_t { IDLE, WAIT_FEEDBACK, AVERAGING, RUNNING, STOPPING, STOPPED };
enum Status : uint32_t {
  COMPLETE = 0, USER_ABORT = 1, RING_FULL = 2, SD_ERROR = 3, TIMER_ERROR = 4,
  TX_FAILED = 5, FEEDBACK_LOST = 6, MOTOR_ERROR = 7, POSITION_INVALID = 8,
  TORQUE_LIMIT = 9, CONTROL_DEADLINE = 10, CAN_ERROR = 11, START_TIMEOUT = 12,
  STOP_FAILED = 13, REFERENCE_INVALID = 14, NONFINITE_CONTROL = 15, RECORD_LIMIT = 16
};
// stopFlags: bit0 zero accepted, bit1 zero TX complete, bit2 exit accepted,
// bit3 exit TX complete; bit4 no-motor simulation. TX complete is CAN ACK,
// not motor-level acknowledgement of torque or mode.
struct Sample {
  uint32_t index, actualUs, phase;
  float positionDeg, speedRadS, feedbackTorqueEstimateNm, chirpNm, pdNm, commandNm;
  uint32_t limited, feedbackAgeUs, feedbackSequence, txAccepted, executionUs,
           motorError, feedbackRaw;
};
struct Header {
  char magic[8];
  uint32_t periodUs, sweepUs, preUs, postUs, expectedRows, rowBytes, motorId,
           canBitrate, noMotor, positionPolicy, feedbackTimeoutUs, profileDocumented,
           motorTimeoutMs;
  float f0, f1, amp0, amp1, amplitudeExponent, kpNmPerDeg, kdNmPerRadS,
        holdDeg, torqueLimitNm, positionMinRad, positionMaxRad,
        velocityMaxRadS, mitTorqueMaxNm;
  uint32_t averageUs, referenceSamples, averageRows;
  float referenceMinRad, referenceMaxRad;
  uint32_t holdPolicy; // 2: average of fresh feedback in initial zero-torque interval.
  uint32_t canPort, motorProtocol;
};
struct Footer { char magic[8]; uint32_t rows, status, stopFlags; };
static_assert(sizeof(Sample) == 64 && sizeof(Header) == 144 && sizeof(Footer) == 20,
              "Update binary converter when changing layout");
constexpr uint64_t LOG_CAPACITY_BYTES = sizeof(Header) +
    uint64_t(EXPECTED_ROWS) * sizeof(Sample) + sizeof(Footer);
static_assert(LOG_CAPACITY_BYTES <= UINT32_MAX, "Log exceeds FAT32 single-file capacity");

inline Phase phaseAt(uint32_t index) {
  return index < PRE_STEPS ? PRE_HOLD : index < PRE_STEPS + STEPS ? SWEEP :
         index < TOTAL_STEPS ? POST_HOLD : STOP_ROW;
}
inline float pdTorque(float targetDeg, float positionDeg, float speedRadS) {
  // Full multi-turn position difference, with NO +/-180 deg error wrapping.
  return KP_PD * (targetDeg - positionDeg) - KD_PD * speedRadS;
}
inline constexpr uint32_t encode(float x, float low, float high, unsigned bits) {
  return uint32_t((fmaxf(low, fminf(high, x)) - low) * ((1u << bits) - 1) / (high-low));
}
inline constexpr float decode(uint32_t x, float low, float high, unsigned bits) {
  return x * (high-low) / ((1u << bits) - 1) + low;
}

