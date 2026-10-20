#pragma once
#include "chirp_config.h"

// Hardware-independent protocol helpers; the Arduino adapter only moves bytes.
struct MotorFrame {
  uint32_t id;
  bool extended, remote, overrun;
  uint8_t len;
  uint8_t data[8];
};
struct MotorFeedback {
  bool valid;
  float positionRad, speedRadS, torqueEstimateNm;
  uint32_t error, raw;
};
inline constexpr MotorFrame packRawCommand(uint32_t p, uint32_t v, uint32_t t) {
  MotorFrame f = {IS_60 ? ((8u<<8)|MOTOR_ID) : MOTOR_ID, IS_60, false, false, 8, {}};
  if (IS_60) {
    // Extended MIT order: kp(12), kd(12), p(16), v(12), t(12); kp=kd=0.
    f.data[0]=0; f.data[1]=0; f.data[2]=0;
    f.data[3]=p>>8; f.data[4]=p; f.data[5]=v>>4;
    f.data[6]=(v&15)<<4 | (t>>8); f.data[7]=t;
  } else {
    f.data[0]=p>>8; f.data[1]=p; f.data[2]=v>>4;
    f.data[3]=(v&15)<<4; f.data[4]=0; f.data[5]=0;
    f.data[6]=t>>8; f.data[7]=t;
  }
  return f;
}
inline constexpr MotorFrame torqueFrame(float torque, float limit = TORQUE_LIMIT_NM) {
  const uint32_t p=encode(0,POSITION_MIN_RAD,POSITION_MAX_RAD,16);
  const uint32_t v=encode(0,-VELOCITY_MAX_RAD_S,VELOCITY_MAX_RAD_S,12);
  uint32_t t=encode(torque,-MIT_TORQUE_MAX_NM,MIT_TORQUE_MAX_NM,12);
  const uint32_t low=uint32_t(ceil((MIT_TORQUE_MAX_NM-limit)*4095.0/(2*MIT_TORQUE_MAX_NM)));
  const uint32_t high=uint32_t(floor((MIT_TORQUE_MAX_NM+limit)*4095.0/(2*MIT_TORQUE_MAX_NM)));
  if (t<low) t=low;
  if (t>high) t=high;
  return packRawCommand(p,v,t);
}
inline constexpr bool isTargetFeedback(const MotorFrame &f) {
  return !f.remote && f.len==8 && (IS_60 ?
      (f.extended && f.id==((0x29u<<8)|MOTOR_ID)) :
      (!f.extended && f.id==MOTOR_ID && f.data[0]==MOTOR_ID));
}
inline constexpr int32_t signed16(uint32_t x) { return x&0x8000 ? int32_t(x)-65536 : int32_t(x); }
inline constexpr MotorFeedback decodeMotorFeedback(const MotorFrame &f) {
  if (!isTargetFeedback(f)) return {};
  MotorFeedback out={true,0,0,0,f.data[7],0};
  if (IS_60) {
    const int32_t p=signed16((uint32_t(f.data[0])<<8)|f.data[1]);
    const int32_t v=signed16((uint32_t(f.data[2])<<8)|f.data[3]);
    out.raw=(uint32_t(f.data[4])<<8)|f.data[5]; // Store uint16 bits, including negative currents.
    out.positionRad=p*0.1f*(3.14159265358979323846f/180.0f);
    out.speedRadS=v*10.0f/AK60_POLE_PAIRS/AK60_GEAR_RATIO*(2.0f*3.14159265358979323846f/60.0f);
    out.torqueEstimateNm=signed16(out.raw)*0.01f*AK60_KT_NM_PER_A;
  } else {
    out.positionRad=decode((uint32_t(f.data[1])<<8)|f.data[2],POSITION_MIN_RAD,POSITION_MAX_RAD,16);
    out.speedRadS=decode((uint32_t(f.data[3])<<4)|(f.data[4]>>4),-VELOCITY_MAX_RAD_S,VELOCITY_MAX_RAD_S,12);
    out.raw=((uint32_t(f.data[4])&15)<<8)|f.data[5];
    out.torqueEstimateNm=decode(out.raw,-MIT_TORQUE_MAX_NM,MIT_TORQUE_MAX_NM,12);
  }
  return out;
}
