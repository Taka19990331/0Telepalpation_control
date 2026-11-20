#pragma once
#include <Arduino.h>

namespace ControlLog {
constexpr uint32_t CAPACITY=16384; // About 8.2 s at 2 kHz; oldest samples are overwritten when full.
struct Sample {
  uint32_t timestampUs=0;
  float position=0, velocity=0;
};
static Sample samples[CAPACITY];
static uint32_t head=0, count=0;

inline void reset() {
  head=0;
  count=0;
}

// Called from the control ISR once for each accepted CAN feedback frame.
inline void append(uint32_t timestampUs,float position,float velocity) {
  samples[head].timestampUs=timestampUs;
  samples[head].position=position;
  samples[head].velocity=velocity;
  head=(head+1)%CAPACITY;
  if(count<CAPACITY) ++count;
}

// Call only while the control session is stopped; serial output is blocking.
inline void dump() {
  Serial.print("LOG_BEGIN,"); Serial.println(count);
  Serial.println("timestamp_us,position_rad,velocity_raw_rad_s");
  const uint32_t oldest=(head+CAPACITY-count)%CAPACITY;
  for(uint32_t i=0;i<count;++i) {
    const Sample &s=samples[(oldest+i)%CAPACITY];
    Serial.print(s.timestampUs); Serial.print(',');
    Serial.print(s.position,7); Serial.print(',');
    Serial.println(s.velocity,7);
  }
  Serial.println("LOG_END");
}
}
