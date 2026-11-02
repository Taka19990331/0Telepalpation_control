#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\network\\hstrerror.c"
#define _GNU_SOURCE
#include <netdb.h>
#include "locale_impl.h"

static const char msgs[] =
	"Host not found\0"
	"Try again\0"
	"Non-recoverable error\0"
	"Address not available\0"
	"\0Unknown error";

const char *hstrerror(int ecode)
{
	const char *s;
	for (s=msgs, ecode--; ecode && *s; ecode--, s++) for (; *s; s++);
	if (!*s) s++;
	return LCTRANS_CUR(s);
}
