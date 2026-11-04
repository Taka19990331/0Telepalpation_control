#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch3\\notch_coefficients.h"
#pragma once
#include <math.h>
#include <float.h>

#include "control_config.h"
// Runtime port of verification/design_asymmetric_notch.m; no generated constants.
namespace NotchDesign {
constexpr double pi=3.14159265358979323846;
struct Settings { double center,low,high,depth,period; };
struct Coefficients {
  double b0=1,b1=0,b2=0,a1=0,a2=0;
  bool valid=false,approximation=false;
};
struct Point { double c,s; };
inline Point point(double f,double fs) { double w=2*pi*f/fs; return {cos(w),-sin(w)}; }
inline double magnitude(const Coefficients &q,double depth,Point z) {
  double b0=1-depth+depth*q.b0,b1=(1-depth)*q.a1+depth*q.b1,b2=(1-depth)*q.a2+depth*q.b2;
  double zr=z.c*z.c-z.s*z.s,zi=2*z.c*z.s;
  return hypot(b0+b1*z.c+b2*zr,b1*z.s+b2*zi)/hypot(1+q.a1*z.c+q.a2*zr,q.a1*z.s+q.a2*zi);
}
inline Coefficients poles(double x,double y,const Settings &s) {
  double fs=1e6/s.period,r=1/(1+exp(-x)),fp=(fs/2)/(1+exp(-y));
  Coefficients q; q.a1=-2*r*cos(2*pi*fp/fs); q.a2=r*r;
  double c=cos(2*pi*s.center/fs),k=(1+q.a1+q.a2)/(2-2*c);
  q.b0=q.b2=k; q.b1=-2*c*k; return q;
}
// MATLAB's objective grid, cached only during initialization (64 kB stack).
class Objective {
 public:
  Settings s; Point grid[4002],edges[2]; unsigned count=0,evaluations=0;
  explicit Objective(Settings settings):s(settings) {
    double fs=1e6/s.period,hi=fmin(fs/2,2*(s.center+s.high));
    unsigned i=0,j=0;
    while(i<=2000 || j<=2000) {
      double a=i<=2000?(fs/2)*double(i)/2000:INFINITY,b=j<=2000?hi*double(j)/2000:INFINITY;
      double f=fmin(a,b); if(a<=b) ++i; if(b<=a) ++j;
      grid[count++]=point(f,fs);
    }
    edges[0]=point(s.center-s.low,fs); edges[1]=point(s.center+s.high,fs);
  }
  double operator()(double x,double y) {
    ++evaluations; Coefficients q=poles(x,y,s); double cost=0;
    for(auto z:edges) { double e=20*log10(magnitude(q,s.depth,z))+3; cost+=e*e; }
    for(unsigned i=0;i<count;++i) {
      double h=magnitude(q,s.depth,grid[i]); if(!isfinite(h)) return DBL_MAX;
      double e=fmax(h-1,0); cost+=100*e*e;
    }
    return isfinite(cost)?cost:DBL_MAX;
  }
};
struct Vertex { double x,y,f; };
inline void sort(Vertex *v) {
  for(int i=1;i<3;++i) for(int j=i;j>0 && v[j].f<v[j-1].f;--j) {
    Vertex t=v[j]; v[j]=v[j-1]; v[j-1]=t;
  }
}
inline Coefficients fit(Settings s) {
  Objective fun(s);
  double fs=1e6/s.period,r=exp(-pi*(s.low+s.high)/fs);
  double x=log(r/(1-r)),y=log((s.center/(fs/2))/(1-s.center/(fs/2)));
  Vertex v[3]={{x,y,0},{x!=0?x*1.05:0.00025,y,0},{x,y!=0?y*1.05:0.00025,0}};
  for(auto &p:v) p.f=fun(p.x,p.y);
  sort(v);
  // Nelder-Mead: MATLAB's initial simplex, limits, tolerances and branch rules.
  for(int iteration=1;iteration<4000 && fun.evaluations<10000;++iteration) {
    double df=fmax(fabs(v[1].f-v[0].f),fabs(v[2].f-v[0].f));
    double dx=fmax(fmax(fabs(v[1].x-v[0].x),fabs(v[2].x-v[0].x)),fmax(fabs(v[1].y-v[0].y),fabs(v[2].y-v[0].y)));
    if(df<=1e-14 && dx<=1e-11) break;
    double mx=(v[0].x+v[1].x)/2,my=(v[0].y+v[1].y)/2;
    Vertex reflected={2*mx-v[2].x,2*my-v[2].y,0}; reflected.f=fun(reflected.x,reflected.y);
    bool shrink=false;
    if(reflected.f<v[0].f) {
      Vertex expanded={3*mx-2*v[2].x,3*my-2*v[2].y,0}; expanded.f=fun(expanded.x,expanded.y);
      v[2]=expanded.f<reflected.f?expanded:reflected;
    } else if(reflected.f<v[1].f) v[2]=reflected;
    else {
      bool outside=reflected.f<v[2].f;
      Vertex contracted={outside?1.5*mx-.5*v[2].x:.5*mx+.5*v[2].x,
                         outside?1.5*my-.5*v[2].y:.5*my+.5*v[2].y,0};
      contracted.f=fun(contracted.x,contracted.y);
      if(outside?contracted.f<=reflected.f:contracted.f<v[2].f) v[2]=contracted;
      else shrink=true;
    }
    if(shrink) for(int i=1;i<3;++i) {
      v[i].x=v[0].x+.5*(v[i].x-v[0].x); v[i].y=v[0].y+.5*(v[i].y-v[0].y);
      v[i].f=fun(v[i].x,v[i].y);
    }
    sort(v);
  }
  return poles(v[0].x,v[0].y,s);
}
inline bool bounded(const Coefficients &q,const Settings &s,int n) {
  double fs=1e6/s.period,hi=fmin(fs/2,2*(s.center+s.high));
  double limits[2]={fs/2,hi};
  for(int i=0;i<=n;++i) for(double limit:limits) {
    double h=magnitude(q,s.depth,point(limit*double(i)/n,fs));
    if(!isfinite(h)||h>1+1e-8) return false;
  }
  return true;
}
inline double crossing(const Coefficients &q,const Settings &s,double left,double right) {
  double fs=1e6/s.period,target=pow(10.,-3./20);
  double fl=magnitude(q,s.depth,point(left,fs))-target,fr=magnitude(q,s.depth,point(right,fs))-target;
  if(!isfinite(fl)||!isfinite(fr)||fl*fr>0) return NAN;
  for(int i=0;i<60;++i) {
    double mid=(left+right)/2,fm=magnitude(q,s.depth,point(mid,fs))-target;
    if(fl*fm<=0) right=mid; else {left=mid;fl=fm;}
  }
  return (left+right)/2;
}
inline Coefficients design(Settings s) {
  Coefficients bad; double fs=1e6/s.period;
  if(!(isfinite(fs)&&fs>0&&s.center>0&&s.low>0&&s.low<s.center&&s.high>0&&s.center+s.high<fs/2&&s.depth>1-pow(10.,-3./20)&&s.depth<=1)) return bad;
  Coefficients q=fit(s); double lo=s.center-s.low,hi=s.center+s.high;
  bool exact=true;
  double edges[2]={lo,hi};
  for(double f:edges) if(!(fabs(20*log10(magnitude(q,s.depth,point(f,fs)))+3)<=.001)) exact=false;
  if(!exact||!bounded(q,s,20000)) {
    // Preserve MATLAB's existing fallback, not a new filter policy.
    double w=2*pi*s.center/fs,alpha=sin(w)*(s.low+s.high)/(2*s.center);
    q.b0=q.b2=1/(1+alpha); q.b1=-2*cos(w)/(1+alpha);
    q.a1=q.b1; q.a2=(1-alpha)/(1+alpha); q.approximation=true;
  }
  double low=crossing(q,s,0,s.center),high=crossing(q,s,s.center,fs/2);
  bool stable=fabs(q.a2)<1 && 1+q.a1+q.a2>0 && 1-q.a1+q.a2>0;
  q.valid=stable&&bounded(q,s,200000)&&isfinite(low)&&isfinite(high)&&fabs(low-lo)/lo<=.1&&fabs(high-hi)/hi<=.1;
  return q;
}
inline const Coefficients &configured() {
  // Once, before the control timer starts. reset()/update() never invoke design().
  static const Coefficients q=design({ControlConfig::NOTCH_CENTER_HZ,ControlConfig::NOTCH_LOW_SIDE_HZ,
    ControlConfig::NOTCH_HIGH_SIDE_HZ,ControlConfig::NOTCH_DEPTH,double(ControlConfig::CONTROL_PERIOD_US)});
  return q;
}
}

