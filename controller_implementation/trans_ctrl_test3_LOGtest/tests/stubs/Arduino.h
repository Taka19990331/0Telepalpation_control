#pragma once
#include <stdint.h>
#include <math.h>
template<class T> T min(T a,T b) { return a<b?a:b; }
template<class T> T max(T a,T b) { return a>b?a:b; }
extern uint32_t mockUs;
inline uint32_t micros() { return mockUs; }
inline void noInterrupts() {}
inline void interrupts() {}
template<class T> T constrain(T x,T lo,T hi) { return min(hi,max(lo,x)); }
#define NVIC_SET_PRIORITY(a,b) ((void)0)
