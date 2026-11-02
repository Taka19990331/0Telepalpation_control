#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\temp\\mkstemps.c"
#define _BSD_SOURCE
#include <stdlib.h>

int mkstemps(char *template, int len)
{
	return __mkostemps(template, len, 0);
}
