#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\compat\\time32\\difftime32.c"
#include "time32.h"
#include <time.h>

double __difftime32(time32_t t1, time32_t t2)
{
	return difftime(t1, t2);
}
