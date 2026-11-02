#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_test2\\control_algorithm.h"
#pragma once
#include <stdint.h>
#include <math.h>
#include "control_config.h"
#include "position_direction.h"
namespace ControlAlgorithm {
struct Signals {
  float velocity=0, friction=0, input=0, state=0, external=0;
  float torqueR=0, torqueF=0, unsaturated=0, command=0;
  bool saturated=false;
};
class Controller {
 public:
  Signals signals;
  float torqueBias=0;
  // Explicit new-run initialization is the only way to clear a numeric fault.
  void reset() { signals=Signals{}; initialized=false; numericFault=false; lastUs=0; frictionFF=0; positionDirection.reset(); }
  bool ready() const { return initialized && !numericFault; }
  bool faulted() const { return numericFault; }
  void zeroOutput() { signals.torqueR=signals.torqueF=signals.unsaturated=signals.command=0; signals.saturated=false; }
  void trip() { numericFault=true; zeroOutput(); }
  static float friction(float w) {
    if(!isfinite(w)) return NAN;
    if(w==0) return 0;
    const auto &p=w>0 ? ControlConfig::POSITIVE : ControlConfig::NEGATIVE;
    const float r=w/p.vs;
    const float magnitude=p.tc+(p.ts-p.tc)*expf(-r*r);
    return (w>0 ? magnitude : -magnitude)+p.b*w;
  }
  bool finiteState() const {
    return isfinite(torqueBias) && isfinite(signals.velocity) && isfinite(signals.friction) &&
      isfinite(signals.input) && isfinite(signals.state) && isfinite(signals.external) &&
      isfinite(signals.torqueR) && isfinite(signals.torqueF) && isfinite(signals.unsaturated) && isfinite(signals.command);
  }
  // Exactly once per received frame, including while torque output is OFF.
  void feedback(float rawVelocity,float motorTorque,uint32_t rxUs) {
    using namespace ControlConfig;
    if(numericFault) return;
    if(!isfinite(rawVelocity) || !isfinite(motorTorque) || !finiteState()) { trip(); return; }
    if(initialized && uint32_t(rxUs-lastUs)==0) return; // No observer update for dt=0.
    if(!initialized) {
      signals.velocity=rawVelocity;
      signals.friction=friction(signals.velocity);
      signals.input=motorTorque-torqueBias-signals.friction+J*G*signals.velocity;
      signals.state=J*G*signals.velocity; // First external estimate is exactly zero.
      initialized=true;
    } else {
      const float dt=float(uint32_t(rxUs-lastUs))*US_TO_SECONDS;
      signals.velocity+=(1.0f-expf(-RADIANS_PER_REVOLUTION*VELOCITY_HZ*dt))*(rawVelocity-signals.velocity);
      signals.friction=friction(signals.velocity);
      signals.input=motorTorque-torqueBias-signals.friction+J*G*signals.velocity;
      signals.state+=(1.0f-expf(-G*dt))*(signals.input-signals.state);
    }
    frictionFF=signals.friction; // Legacy feedback interface retains original FF.
    lastUs=rxUs;
    signals.external=J*G*signals.velocity-signals.state;
    if(!finiteState()) trip();
  }
  // Production interface: exactly one position sample per processed CAN frame.
  void feedback(float rawVelocity,float motorTorque,uint32_t rxUs,float position) {
    if(numericFault) return;
    if(!isfinite(position)) { trip(); return; }
    positionDirection.update(position,rxUs);
    feedback(rawVelocity,motorTorque,rxUs); // Unchanged CAN LPF and RFOB.
    if(!numericFault) frictionFF=feedforwardFriction(signals.velocity,positionDirection.direction());
  }
  static float feedforwardFriction(float velocity,PositionDirection::Direction direction) {
    using namespace ControlConfig;
    const float speed=fabsf(velocity);
    if(speed>=FRICTION_BLEND_HIGH) return friction(velocity); // Exact legacy result.
    float positionFriction=0;
    if(direction!=PositionDirection::UNKNOWN)
      positionFriction=friction(direction==PositionDirection::POSITIVE ? speed : -speed);
    if(speed<=FRICTION_BLEND_LOW) return positionFriction;
    const float alpha=(speed-FRICTION_BLEND_LOW)/(FRICTION_BLEND_HIGH-FRICTION_BLEND_LOW);
    return (1.0f-alpha)*positionFriction+alpha*friction(velocity);
  }
  float command(bool enabled,float Kr,float Kf) {
    if(numericFault) { zeroOutput(); return 0; }
    if(!isfinite(Kr) || !isfinite(Kf) || !finiteState()) { trip(); return 0; }
    if(!enabled || !ready()) { zeroOutput(); return 0; }
    signals.torqueR=Kr*signals.external;
    signals.torqueF=Kf*frictionFF;
    signals.unsaturated=signals.torqueR+signals.torqueF;
    // Never send NaN/Inf through saturation: latch and require explicit reset.
    if(!finiteState()) { trip(); return 0; }
    signals.command=fmaxf(-ControlConfig::TORQUE_LIMIT,fminf(ControlConfig::TORQUE_LIMIT,signals.unsaturated));
    signals.saturated=signals.command!=signals.unsaturated;
    if(!isfinite(signals.command)) trip();
    return signals.command;
  }
 private:
  bool initialized=false, numericFault=false;
  uint32_t lastUs=0;
  float frictionFF=0;
  PositionDirection::Estimator positionDirection;
};
}
