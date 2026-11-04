#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch3\\position_direction.h"
#pragma once
#include <stdint.h>
#include "control_config.h"

namespace PositionDirection {
enum Direction { UNKNOWN, POSITIVE, NEGATIVE };
class Estimator {
 public:
  void reset() { head=0; count=0; result=UNKNOWN; }
  Direction direction() const { return result; }
  void update(float position,uint32_t rxUs) {
    using namespace ControlConfig;
    if(count && rxUs==samples[(head+count-1)%CAPACITY].us) return;
    // Keep the latest received sample at or before now - 50 ms, plus newer samples.
    while(count>1 && uint32_t(rxUs-samples[(head+1)%CAPACITY].us)>=POSITION_DIRECTION_WINDOW_US) {
      head=(head+1)%CAPACITY; --count;
    }
    // Capacity exceeds the number of accepted frames possible in 50 ms at 1 Mbit/s.
    // If exhausted, discard incomplete history and require a fresh full window.
    if(count==CAPACITY) reset();
    samples[(head+count)%CAPACITY]={position,rxUs}; ++count;
    result=UNKNOWN;
    if(uint32_t(rxUs-samples[head].us)<POSITION_DIRECTION_WINDOW_US) return;
    float delta=position-samples[head].position;
    const float range=POSITION_MAX-POSITION_MIN;
    if(delta>range*0.5f) delta-=range;
    else if(delta<-range*0.5f) delta+=range;
    if(delta>POSITION_DIRECTION_THRESHOLD_RAD) result=POSITIVE;
    else if(delta<-POSITION_DIRECTION_THRESHOLD_RAD) result=NEGATIVE;
  }
 private:
  static constexpr uint32_t CAPACITY=1024;
  struct Sample { float position; uint32_t us; };
  Sample samples[CAPACITY];
  uint32_t head=0, count=0;
  Direction result=UNKNOWN;
};
}
