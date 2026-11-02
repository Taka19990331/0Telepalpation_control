#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\llrintf.c"
#include <math.h>

/* uses LLONG_MAX > 2^24, see comments in lrint.c */

long long llrintf(float x)
{
	return rintf(x);
}
