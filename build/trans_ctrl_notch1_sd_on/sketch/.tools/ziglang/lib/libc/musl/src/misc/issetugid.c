#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\misc\\issetugid.c"
#define _BSD_SOURCE
#include <unistd.h>
#include "libc.h"

int issetugid(void)
{
	return libc.secure;
}
