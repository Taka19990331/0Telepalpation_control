#pragma once
#include <math.h>
#include "control_config.h"
#include "notch_coefficients.h"
namespace ControlAlgorithm {
// Asymmetric pole-shift biquad: H=(1-depth)+depth*Hnotch.
// Initialization-only fitted coefficients; two states, no added block delay.
// Double states avoid cancellation at low f0/fs. No trigonometry in update().
class FixedNotch {
 public:
  FixedNotch() {
    const auto &q=NotchDesign::configured();
    b0=q.b0; b1=q.b1; b2=q.b2; a1=q.a1; a2=q.a2;
    if(!q.valid) b0=NAN; // Existing finiteState/trip path prevents control with rejected coefficients.
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


