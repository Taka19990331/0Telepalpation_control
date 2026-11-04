#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch3\\tests\\notch_runtime.cpp"
#include <cstdio>
#include <cassert>
#include <chrono>
#include "../fixed_notch.h"
int main() {
  using namespace NotchDesign;
  Settings cases[]={{12,4,20,.95,500},{12,8,22,.95,500},{12,10,22,.95,500},{24,8,40,.95,250}};
  for(auto s:cases) {
    auto start=std::chrono::steady_clock::now();
    auto q=design(s);
    double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    printf("%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%d,%d,%.6f\n",
      s.center,s.low,s.high,s.depth,s.period,q.b0,q.b1,q.b2,q.a1,q.a2,q.valid,q.approximation,seconds);
  }
  assert(!design({12,12,20,.95,500}).valid);
  assert(!design({12,4,20,.1,500}).valid);
  ControlAlgorithm::FixedNotch filter;
  const auto &q=configured();
  assert(filter.finiteState()==q.valid);
  if(q.valid) {
    for(int i=0;i<10000;++i) assert(fabs(filter.update(2)-2)<1e-5);
    filter.reset();
    assert(fabs(filter.update(-3)+3)<1e-5);
  } else { filter.reset(); assert(!filter.finiteState()); }
}
