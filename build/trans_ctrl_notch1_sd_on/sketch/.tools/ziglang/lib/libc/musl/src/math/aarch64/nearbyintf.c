#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\aarch64\\nearbyintf.c"
#include <math.h>

float nearbyintf(float x)
{
	__asm__ ("frinti %s0, %s1" : "=w"(x) : "w"(x));
	return x;
}
