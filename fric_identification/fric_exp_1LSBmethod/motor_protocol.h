#pragma once
#include "fric_config.h"

struct MotorFrame { uint32_t id; bool extended, remote, overrun; uint8_t len; uint8_t data[8]; };
struct MotorFeedback { bool valid; float positionRad, speedRadS, torqueEstimateNm; uint8_t error; uint16_t torqueRaw; };
inline uint32_t encodeValue(float x, float low, float high, uint8_t bits) {
  float c=(fmaxf(low,fminf(high,x))-low)*((1u<<bits)-1)/(high-low);
  return (uint32_t)c;
}
inline float decodeValue(uint32_t x,float low,float high,uint8_t bits) {
  return x*(high-low)/((1u<<bits)-1)+low;
}
// The MIT feedback field is decoded on [-12.5, 12.5]. Continuous-turn runs
// unwrap adjacent samples; after a gap that could hide multiple wraps, the
// firmware rebases and marks the discontinuity rather than inventing turns.
inline float wrappedPositionDelta(float current, float previous) {
  float delta=current-previous;
  if(delta>POSITION_WRAP_PERIOD_RAD*0.5f) delta-=POSITION_WRAP_PERIOD_RAD;
  else if(delta< -POSITION_WRAP_PERIOD_RAD*0.5f) delta+=POSITION_WRAP_PERIOD_RAD;
  return delta;
}
inline MotorFrame torqueFrameFromCount(uint16_t t) {
  MotorFrame f={MOTOR_ID,false,false,false,8,{}};
  const uint16_t p=encodeValue(0,POSITION_MIN_RAD,POSITION_MAX_RAD,16);
  const uint16_t v=encodeValue(0,-VELOCITY_MAX_RAD_S,VELOCITY_MAX_RAD_S,12);
  f.data[0]=p>>8; f.data[1]=p; f.data[2]=v>>4; f.data[3]=(v&15)<<4;
  f.data[4]=0; f.data[5]=0; f.data[6]=t>>8; f.data[7]=t;
  return f;
}
inline float torqueForCount(uint16_t c) { return decodeValue(c,-MIT_TORQUE_MAX_NM,MIT_TORQUE_MAX_NM,12); }
inline bool isTargetFeedback(const MotorFrame &f) {
  return !f.extended && !f.remote && f.len==8 && f.id==MOTOR_ID && f.data[0]==MOTOR_ID;
}
inline MotorFeedback decodeMotorFeedback(const MotorFrame &f) {
  if(!isTargetFeedback(f)) return {};
  const uint16_t p=(uint16_t(f.data[1])<<8)|f.data[2];
  const uint16_t v=(uint16_t(f.data[3])<<4)|(f.data[4]>>4);
  const uint16_t t=(uint16_t(f.data[4]&15)<<8)|f.data[5];
  return {true,decodeValue(p,POSITION_MIN_RAD,POSITION_MAX_RAD,16),
          decodeValue(v,-VELOCITY_MAX_RAD_S,VELOCITY_MAX_RAD_S,12),
          decodeValue(t,-MIT_TORQUE_MAX_NM,MIT_TORQUE_MAX_NM,12),f.data[7],t};
}
