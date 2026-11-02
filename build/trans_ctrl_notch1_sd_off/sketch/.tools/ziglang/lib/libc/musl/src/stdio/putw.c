#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdio\\putw.c"
#define _GNU_SOURCE
#include <stdio.h>

int putw(int x, FILE *f)
{
	return (int)fwrite(&x, sizeof x, 1, f)-1;
}
