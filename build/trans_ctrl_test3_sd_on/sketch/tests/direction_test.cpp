#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_test3\\tests\\direction_test.cpp"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "control_algorithm.h"

using ControlAlgorithm::Controller;

static void frictionModelTests() {
  assert(Controller::friction(0.0f)==0.0f);
  for(int i=-1000;i<=1000;++i) {
    const float w=i*0.0001f;
    const float value=Controller::friction(w);
    assert(isfinite(value));
    assert((w>0 && value>0) || (w<0 && value<0) || (w==0 && value==0));
    if(i>-1000) assert(value>=Controller::friction((i-1)*0.0001f));
  }

  const float epsilon=0.000001f;
  assert(fabsf(Controller::friction(epsilon)-Controller::friction(-epsilon))<0.00001f);
  for(float speed : {1.0f, 5.0f, 10.0f}) {
    const float positive=ControlConfig::POSITIVE.tc+ControlConfig::POSITIVE.b*speed;
    const float negative=-ControlConfig::NEGATIVE.tc-ControlConfig::NEGATIVE.b*speed;
    assert(fabsf(Controller::friction(speed)-positive)<0.000001f);
    assert(fabsf(Controller::friction(-speed)-negative)<0.000001f);
  }
}

static void rfobAndFeedforwardTests() {
  Controller controller;
  const float rawVelocity=0.02f, motorTorque=0.3f, bias=0.04f;
  controller.torqueBias=bias;
  controller.feedback(rawVelocity,motorTorque,1000);
  const float expectedFriction=Controller::friction(rawVelocity);
  assert(controller.signals.velocity==rawVelocity);
  assert(controller.signals.friction==expectedFriction);
  assert(controller.signals.input==motorTorque-bias-expectedFriction+
      ControlConfig::J*ControlConfig::G*rawVelocity);
  controller.command(true,0.0f,0.7f);
  assert(controller.signals.torqueF==0.7f*expectedFriction);

  const float nextRawVelocity=-0.03f;
  controller.feedback(nextRawVelocity,-0.1f,1500);
  const float alpha=1.0f-expf(-ControlConfig::RADIANS_PER_REVOLUTION*
      ControlConfig::VELOCITY_HZ*0.0005f);
  const float expectedVelocity=rawVelocity+alpha*(nextRawVelocity-rawVelocity);
  const float expectedNextFriction=Controller::friction(expectedVelocity);
  assert(fabsf(controller.signals.velocity-expectedVelocity)<1.0e-7f);
  assert(controller.signals.friction==expectedNextFriction);
  assert(fabsf(controller.signals.input-(-0.1f-bias-expectedNextFriction+
      ControlConfig::J*ControlConfig::G*expectedVelocity))<1.0e-7f);
  controller.command(true,0.0f,0.7f);
  assert(controller.signals.torqueF==0.7f*controller.signals.friction);
}

int main() {
  frictionModelTests();
  rfobAndFeedforwardTests();
  puts("PASS: zero, smooth transition, positive/negative asymptotes, LPF-based RFOB and FF");
}
