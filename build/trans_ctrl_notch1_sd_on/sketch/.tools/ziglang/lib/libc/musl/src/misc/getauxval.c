#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\misc\\getauxval.c"
#include <sys/auxv.h>
#include <errno.h>
#include "libc.h"

unsigned long __getauxval(unsigned long item)
{
	size_t *auxv = libc.auxv;
	if (item == AT_SECURE) return libc.secure;
	for (; *auxv; auxv+=2)
		if (*auxv==item) return auxv[1];
	errno = ENOENT;
	return 0;
}

weak_alias(__getauxval, getauxval);
