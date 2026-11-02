#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\fixed_notch.h"
#pragma once
#include <math.h>
#include "control_config.h"
namespace ControlAlgorithm {
// RBJ notch blended with the dry input: H=(1-depth)+depth*Hnotch.
// Q=f0/BW. BW is the underlying full-notch -3 dB bandwidth (Hz).
// Double states avoid cancellation at low f0/fs. No trigonometry in update().
class FixedNotch {
 public:
  FixedNotch() {
    const double w=6.2831853071795864769*ControlConfig::NOTCH_CENTER_HZ*
      ControlConfig::CONTROL_PERIOD_US*1e-6;
    const double alpha=sin(w)*ControlConfig::NOTCH_BANDWIDTH_HZ/(2*ControlConfig::NOTCH_CENTER_HZ);
    b0=1/(1+alpha); b1=-2*cos(w)/(1+alpha); b2=b0;
    a1=b1; a2=(1-alpha)/(1+alpha);
  }
  void reset() { z1=z2=0; initialized=false; }
  bool finiteState() const { return isfinite(z1)&&isfinite(z2)&&isfinite(b0)&&isfinite(b1)&&isfinite(b2)&&isfinite(a1)&&isfinite(a2); }
  float update(float input) {
    const double x=input;
    if(!initialized) { z1=(1-b0)*x; z2=(b2-a2)*x; initialized=true; }
    const double y=b0*x+z1;
    z1=b1*x-a1*y+z2; z2=b2*x-a2*y;
    return float(x+ControlConfig::NOTCH_DEPTH*(y-x));
  }
  double b0=0,b1=0,b2=0,a1=0,a2=0;
 private:
  double z1=0,z2=0;
  bool initialized=false;
};
}
