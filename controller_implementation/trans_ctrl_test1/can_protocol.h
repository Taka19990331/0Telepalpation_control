#pragma once

// Teensy 4.1 / FlexCAN_T4: select the wired motor at build time.
#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <IntervalTimer.h>

namespace CanProtocol {
constexpr uint32_t SAMPLE_HZ = 2000;
constexpr uint32_t PERIOD_US = 1000000UL / SAMPLE_HZ;
constexpr uint32_t CAN_BAUD = 1000000;
constexpr char MOTOR_SELECT[] = "80B"; // "80A", "80B", "60"
constexpr bool sameText(const char *a, const char *b) {
  return *a == *b && (*a == '\0' || sameText(a + 1, b + 1));
}
constexpr bool IS_80A = sameText(MOTOR_SELECT, "80A");
constexpr bool IS_80B = sameText(MOTOR_SELECT, "80B");
constexpr bool IS_60 = sameText(MOTOR_SELECT, "60");
static_assert(IS_80A || IS_80B || IS_60, "Select 80A, 80B or 60");
constexpr uint8_t MOTOR_ID = IS_60 ? 3 : (IS_80A ? 1 : 2);
constexpr float P_MIN = IS_60 ? -12.56f : -12.5f;
constexpr float P_MAX = -P_MIN;
constexpr float V_MAX = IS_60 ? 60.0f : 37.5f;
constexpr float T_MAX = IS_60 ? 12.0f : 32.0f;
constexpr float KP_MAX = 500.0f, KD_MAX = 5.0f;
constexpr float PI_F = 3.14159265358979323846f;

using CanType = FlexCAN_T4<IS_60 ? CAN3 : (IS_80A ? CAN1 : CAN2), RX_SIZE_256, TX_SIZE_16>;
static CanType can;

inline uint32_t encode(float x, float lo, float hi, uint8_t bits) {
  x = constrain(x, lo, hi);
  return uint32_t((x - lo) * float((1UL << bits) - 1) / (hi - lo));
}
inline float decode(uint32_t x, float lo, float hi, uint8_t bits) {
  return float(x) * (hi - lo) / float((1UL << bits) - 1) + lo;
}
inline bool sendSpecial(uint8_t code) {
  if (IS_60) return true; // AK60 does not use AK80 FC/FD commands.
  CAN_message_t m = {};
  m.id = MOTOR_ID; m.len = 8;
  for (uint8_t i = 0; i < 7; ++i) m.buf[i] = 0xff;
  m.buf[7] = code;
  return can.write(m);
}
inline bool enable() { return sendSpecial(0xfc); }
inline bool disable() { return sendSpecial(0xfd); }

// MIT command; AK80 standard frame and AK60 extended frame packing.
inline bool command(float p, float v, float kp, float kd, float tau) {
  const uint32_t pi = encode(p, P_MIN, P_MAX, 16);
  const uint32_t vi = encode(v, -V_MAX, V_MAX, 12);
  const uint32_t kpi = encode(kp, 0, KP_MAX, 12);
  const uint32_t kdi = encode(kd, 0, KD_MAX, 12);
  const uint32_t ti = encode(tau, -T_MAX, T_MAX, 12);
  CAN_message_t m = {};
  m.len = 8;
  if (IS_60) {
    m.id = (8UL << 8) | MOTOR_ID; m.flags.extended = 1;
    m.buf[0] = kpi >> 4; m.buf[1] = ((kpi & 15) << 4) | (kdi >> 8); m.buf[2] = kdi;
    m.buf[3] = pi >> 8; m.buf[4] = pi; m.buf[5] = vi >> 4;
    m.buf[6] = ((vi & 15) << 4) | (ti >> 8); m.buf[7] = ti;
  } else {
    m.id = MOTOR_ID;
    m.buf[0] = pi >> 8; m.buf[1] = pi; m.buf[2] = vi >> 4;
    m.buf[3] = ((vi & 15) << 4) | (kpi >> 8); m.buf[4] = kpi;
    m.buf[5] = kdi >> 4; m.buf[6] = ((kdi & 15) << 4) | (ti >> 8); m.buf[7] = ti;
  }
  return can.write(m);
}
inline void begin() {
  can.begin(); can.setBaudRate(CAN_BAUD);
  can.setMaxMB(16); can.setMB(MB0, RX, STD);
  can.setMBFilter(REJECT_ALL); can.setMBFilter(MB0, ACCEPT_ALL);
  can.enableMBInterrupt(MB0);
  NVIC_SET_PRIORITY(IS_60 ? IRQ_CAN3 : (IS_80A ? IRQ_CAN1 : IRQ_CAN2), 192);
}
} // namespace CanProtocol
