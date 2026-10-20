#pragma once
#include "chirp_config.h"

inline constexpr float homeClamp(float x, float low, float high) {
  return fmaxf(low, fminf(high, x));
}
inline constexpr float homeSlew(float target, float previous, float delta) {
  return homeClamp(target, previous-delta, previous+delta);
}
// No wrapped angles. All position/velocity calculations use rad and rad/s.
struct HomingControl {
  float velocity = 0, integral = 0, command = 0;
  float proportional = 0, raw = 0;
  bool saturated = false, slewLimited = false;
  constexpr void reset() { velocity = integral = command = proportional = raw = 0; saturated = slewLimited = false; }
  constexpr float step(float targetRad, float positionRad, float measuredVelocity, float dt) {
    const float desired = homeClamp(HOMING_POSITION_K*(targetRad-positionRad),
                                   -HOMING_MAX_VELOCITY, HOMING_MAX_VELOCITY);
    velocity = homeSlew(desired, velocity, HOMING_MAX_ACCELERATION*dt);
    const float error = velocity-measuredVelocity;
    proportional = HOMING_VELOCITY_KP*error;
    // Conditional integration against BOTH absolute and slew reachable limits.
    // Increments opposing saturation are allowed (unwinding).
    const float low = fmaxf(-HOMING_TORQUE_LIMIT_NM, command-HOMING_TORQUE_SLEW*dt);
    const float high = fminf(HOMING_TORQUE_LIMIT_NM, command+HOMING_TORQUE_SLEW*dt);
    const float increment = HOMING_VELOCITY_KI*error*dt;
    const float candidate = proportional+integral+increment;
    if (!((candidate > high && increment > 0) || (candidate < low && increment < 0)))
      integral += increment;
    raw = proportional+integral;
    saturated = fabsf(raw) > HOMING_TORQUE_LIMIT_NM;
    const float clipped = homeClamp(raw, -HOMING_TORQUE_LIMIT_NM, HOMING_TORQUE_LIMIT_NM);
    command = homeClamp(clipped, low, high);
    slewLimited = fabsf(command-clipped) > 1e-7f;
    return command;
  }
  // Freeze PI contribution at the last SENT command; no integration in handover.
  constexpr float handover(float initial, float pd, float fraction, float dt) {
    raw = (1-fraction)*initial + fraction*pd;
    saturated = fabsf(raw) > HOMING_TORQUE_LIMIT_NM;
    const float clipped = homeClamp(raw, -HOMING_TORQUE_LIMIT_NM, HOMING_TORQUE_LIMIT_NM);
    command = homeSlew(clipped, command, HOMING_TORQUE_SLEW*dt);
    slewLimited = fabsf(command-clipped) > 1e-7f;
    return command;
  }
};

// Wall-clock continuous settling; reset only at phase entry or tolerance loss.
struct ContinuousSettle {
  bool tracking = false;
  uint32_t since = 0;
  constexpr void reset() { tracking = false; since = 0; }
  constexpr bool update(bool within, uint32_t now) {
    if (!within) tracking = false;
    else if (!tracking) { tracking = true; since = now; }
    return tracking && now-since >= SETTLE_STEPS*PERIOD_US;
  }
};

// Shared non-driving-testable gates. No retry path; each timeout is terminal.
enum GateDecision { GATE_WAIT, GATE_ADVANCE, GATE_TIMEOUT };
inline constexpr GateDecision zeroingGate(ContinuousSettle &settle, bool within,
    uint32_t now, uint32_t first, uint32_t rows) {
  return settle.update(within, now) ? GATE_ADVANCE :
    (rows >= ZEROING_STEPS || now-first >= ZEROING_STEPS*PERIOD_US) ? GATE_TIMEOUT : GATE_WAIT;
}
inline constexpr GateDecision preHoldGate(ContinuousSettle &settle, bool within,
    uint32_t now, uint32_t first, uint32_t rows) {
  const bool ready = settle.update(within, now);
  return ready && now-first >= PRE_STEPS*PERIOD_US ? GATE_ADVANCE :
    (rows >= PRE_WAIT_STEPS || now-first >= PRE_WAIT_STEPS*PERIOD_US) ? GATE_TIMEOUT : GATE_WAIT;
}

inline constexpr bool handoverReady(float command, float pd) {
  return fabsf(pd) <= HOMING_TORQUE_LIMIT_NM && fabsf(command-pd) <= 1e-6f;
}
