#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../control_algorithm.h"
namespace Baseline {
#include "../../trans_ctrl_test3/control_algorithm.h"
}
int main(int argc,char**argv) {
  using namespace ControlAlgorithm;
  FixedNotch n;
  printf("coeff %.17g %.17g %.17g %.17g %.17g\n",n.b0,n.b1,n.b2,n.a1,n.a2);
  for(int i=0;i<10000;++i) assert(fabs(n.update(2)-2)<1e-6);
  n.reset();
  Controller a,b; Baseline::ControlAlgorithm::Controller old;
  for(unsigned i=0;i<100000;++i) {
    float v=sin(i*.037f), t=cos(i*.017f);
    // Zero/multiple CAN frames in a tick must not change notch update count.
    for(unsigned j=0;j<i%3;++j) { unsigned us=i*500+j+1; a.feedback(v,t,us); b.feedback(v,t,us); old.feedback(v,t,us); }
    float c=a.command(true,.75f,0), d=b.command(i%2000>1000,.75f,0);
    float legacy=old.command(true,.75f,0);
    assert(a.signals.external==old.signals.external);
    assert(a.signals.friction==old.signals.friction);
    assert(a.finiteState() && b.finiteState());
    if(i%2000>1000) assert(c==d); else assert(d==0);
    if(!ControlConfig::NOTCH_ENABLED) assert(c==legacy && a.signals.torqueR==old.signals.torqueR);
  }
  a.signals.external=NAN; assert(a.command(true,.75f,0)==0 && a.faulted());
  a.reset(); assert(!a.faulted());
  if(argc>1) {
    FILE*in=fopen(argv[1],"r"); FILE*out=fopen(argv[2],"w"); assert(in&&out);
    float x; FixedNotch f;
    while(fscanf(in,"%f",&x)==1) { float y=f.update(x); assert(isfinite(y)&&f.finiteState()); fprintf(out,"%.9g\n",y); }
    fclose(in); fclose(out);
  }
  puts("PASS: finite, DC initialization, baseline observer, OFF equivalence, warm switching, numeric fault");
}

