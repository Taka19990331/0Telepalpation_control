#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdlib\\gcvt.c"
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>

char *gcvt(double x, int n, char *b)
{
	sprintf(b, "%.*g", n, x);
	return b;
}
