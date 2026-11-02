#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdio\\setlinebuf.c"
#define _GNU_SOURCE
#include <stdio.h>

void setlinebuf(FILE *f)
{
	setvbuf(f, 0, _IOLBF, 0);
}
