#pragma once
#include <math.h>
#include "control_config.h"

namespace ControlAlgorithm {
struct Signals {
  float velocity=0.0f;
  float friction=0.0f;
  float input=0.0f;
  float state=0.0f;
  float external=0.0f;
  float torqueR=0.0f;
  float unsaturated=0.0f;
  float command=0.0f;
  bool saturated=false;
};

class Controller {
 public:
  Signals signals;
  float torqueBias=0.0f;

  Controller() : qAlpha(1.0f-expf(-ControlConfig::G*(float(ControlConfig::CONTROL_PERIOD_US)*ControlConfig::US_TO_SECONDS))) {}

  void resetObserver() {
    signals=Signals{};
    initialized=false;
    numericFault=false;
  }
  bool ready() const { return initialized && !numericFault; }
  bool faulted() const { return numericFault; }
  void zeroOutput() {
    signals.torqueR=signals.unsaturated=signals.command=0.0f;
    signals.saturated=false;
  }
  void trip() { numericFault=true; zeroOutput(); }

  static float friction(float velocity,const ControlConfig::FrictionParameters &p) {
    if(!isfinite(velocity)) return NAN;
    if(velocity==0.0f) return 0.0f;
    const float ratio=velocity/p.vs;
    const float magnitude=p.tc+(p.ts-p.tc)*expf(-ratio*ratio);
    return (velocity>0.0f ? magnitude : -magnitude)+p.b*velocity;
  }

  // Called once per 500-us control tick, only when a new feedback frame arrived.
  // The raw Jia velocity is used directly: no averaging or additional filters.
  void update(float velocity,float motorTorque) {
    using namespace ControlConfig;
    if(numericFault) return;
    if(!isfinite(velocity)||!isfinite(motorTorque)||!isfinite(torqueBias)||!finiteState()) {
      trip(); return;
    }
    signals.velocity=velocity;
    const FrictionParameters &p=velocity>0.0f?POSITIVE:NEGATIVE;
    signals.friction=friction(velocity,p);
    signals.input=motorTorque-torqueBias-signals.friction+J*G*signals.velocity;
    if(!initialized) {
      signals.state=J*G*signals.velocity;
      initialized=true;
    } else {
      // Exact zero-order-hold discretization of the original first-order Q filter.
      signals.state+=qAlpha*(signals.input-signals.state);
    }
    signals.external=J*G*signals.velocity-signals.state;
    if(!finiteState()) trip();
  }

  // The RFOB command is limited here. Standalone tanh friction FF is added by the sketch.
  float command(bool enabled,float Kr) {
    using namespace ControlConfig;
    if(numericFault) { zeroOutput(); return 0.0f; }
    if(!isfinite(Kr)||!finiteState()) { trip(); return 0.0f; }
    if(!enabled||!ready()) { zeroOutput(); return 0.0f; }
    signals.torqueR=Kr*signals.external;
    signals.unsaturated=signals.torqueR;
    if(!finiteState()) { trip(); return 0.0f; }
    signals.command=fmaxf(-TORQUE_LIMIT,fminf(TORQUE_LIMIT,signals.unsaturated));
    signals.saturated=(signals.command!=signals.unsaturated);
    if(!isfinite(signals.command)) trip();
    return signals.command;
  }

 private:
  bool initialized=false;
  bool numericFault=false;
  const float qAlpha;
  bool finiteState() const {
    return isfinite(qAlpha)&&isfinite(torqueBias)&&isfinite(signals.velocity)&&isfinite(signals.friction)&&
      isfinite(signals.input)&&isfinite(signals.state)&&isfinite(signals.external)&&
      isfinite(signals.torqueR)&&isfinite(signals.unsaturated)&&isfinite(signals.command);
  }
};
}
