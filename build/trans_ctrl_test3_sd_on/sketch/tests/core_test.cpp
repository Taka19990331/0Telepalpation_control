#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_test3\\tests\\core_test.cpp"
// Host-only deterministic test; no device access.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "can_protocol.h"
#include "control_algorithm.h"
uint32_t mockUs=0;
static ControlAlgorithm::Controller revised;
static uint32_t updates=0;
static void compareFeedback(const CanProtocol::Feedback &f) {
  revised.feedback(f.velocity,f.torque,f.rxUs);
  const auto &a=revised.signals;
  assert(a.friction==ControlAlgorithm::Controller::friction(a.velocity));
  assert(a.input==f.torque-revised.torqueBias-a.friction+
      ControlConfig::J*ControlConfig::G*a.velocity);
  ++updates;
}
static CAN_message_t frame(unsigned n) {
  CAN_message_t m; m.id=2; m.len=8; m.buf[0]=2;
  const unsigned v=(n*131)%4096, t=(n*37)%4096, p=(n*139)%65536;
  m.buf[1]=p>>8; m.buf[2]=p; m.buf[3]=v>>4;
  m.buf[4]=((v&15)<<4)|(t>>8); m.buf[5]=t;
  return m;
}
int main() {
  CanProtocol::feedbackHandler=compareFeedback;
  CanDiagnostics::reset(0); CanProtocol::resetRx(true);
  // Wraparound, repeated timestamps, batches, missing feedback, ON/OFF and gain changes.
  mockUs=0xffff0000U;
  for(unsigned tick=0;tick<120000;++tick) {
    unsigned incoming=tick%29==0?3:(tick%13==0?0:1);
    for(unsigned j=0;j<incoming;++j) {
      mockUs+=j==1 && tick%7==0?0:137;
      const auto m=frame(tick*3+j); CanProtocol::receiveFeedback(m);
      assert(updates==CanDiagnostics::rxProcessed); // Callback never runs observer.
    }
    mockUs+=500;
    const uint32_t before=updates;
    for(unsigned n=0;n<ControlConfig::RX_BUDGET && CanProtocol::processFeedback(mockUs);++n) {}
    assert(updates-before<=ControlConfig::RX_BUDGET);
    const bool on=tick%4000>1000;
    const float kr=tick%2?0.5f:-0.2f, kf=tick%3?0.7f:0.0f;
    const float command=revised.command(on,kr,kf);
    if(on && revised.ready()) {
      const float unsaturated=kr*revised.signals.external+kf*revised.signals.friction;
      assert(revised.signals.unsaturated==unsaturated);
      assert(command==fmaxf(-ControlConfig::TORQUE_LIMIT,
          fminf(ControlConfig::TORQUE_LIMIT,unsaturated)));
    } else assert(command==0);
    const float saved=revised.signals.external;
    assert(!CanProtocol::processFeedback(mockUs));
    assert(saved==revised.signals.external);
  }
  // Queue full: preserve FIFO, count loss, and never process beyond a tick budget.
  CanProtocol::resetRx(true); CanProtocol::feedback=CanProtocol::Feedback{};
  const uint32_t before=updates;
  for(unsigned n=0;n<ControlConfig::RX_QUEUE_ROWS+3;++n) {
    ++mockUs; CanProtocol::receiveFeedback(frame(n));
  }
  assert(CanDiagnostics::rxOverflow==3);
  assert(CanDiagnostics::rxQueueMax==ControlConfig::RX_QUEUE_ROWS);
  for(unsigned n=0;n<ControlConfig::RX_QUEUE_ROWS;++n) {
    assert(CanProtocol::processFeedback(mockUs));
    assert(CanProtocol::feedback.sequence==n+1);
    assert(CanProtocol::feedback.rxUs==mockUs-ControlConfig::RX_QUEUE_ROWS-2+n);
  }
  assert(updates==before+ControlConfig::RX_QUEUE_ROWS);
  assert(!CanProtocol::processFeedback(mockUs));
  auto bad=frame(1); bad.len=6; CanProtocol::receiveFeedback(bad);
  assert(CanDiagnostics::rxRejected==1);
  revised.feedback(NAN,0,mockUs);
  assert(revised.faulted());
  assert(revised.command(true,1,1)==0);
  printf("PASS: 120000 ticks, %u feedback updates; LPF-based friction and FF; FIFO/budget/overflow/wrap/NaN\n",updates);
}
