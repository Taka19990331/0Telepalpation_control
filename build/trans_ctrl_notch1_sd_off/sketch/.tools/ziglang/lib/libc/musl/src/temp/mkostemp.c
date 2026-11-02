#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\temp\\mkostemp.c"
#define _BSD_SOURCE
#include <stdlib.h>

int mkostemp(char *template, int flags)
{
	return __mkostemps(template, 0, flags);
}
