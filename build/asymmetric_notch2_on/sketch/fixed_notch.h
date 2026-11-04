#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch2\\fixed_notch.h"
#pragma once
#include <math.h>
#include "control_config.h"
#include "notch_coefficients.h"
namespace ControlAlgorithm {
// Asymmetric pole-shift biquad: H=(1-depth)+depth*Hnotch.
// Offline fitted coefficients; two states, no added block delay.
// Double states avoid cancellation at low f0/fs. No trigonometry in update().
class FixedNotch {
 public:
  FixedNotch() {
    b0=NotchCoefficients::COEF_B0; b1=NotchCoefficients::COEF_B1; b2=NotchCoefficients::COEF_B2;
    a1=NotchCoefficients::COEF_A1; a2=NotchCoefficients::COEF_A2;
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


