#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\network\\herror.c"
#define _GNU_SOURCE
#include <stdio.h>
#include <netdb.h>

void herror(const char *msg)
{
	fprintf(stderr, "%s%s%s\n", msg?msg:"", msg?": ":"", hstrerror(h_errno));
}
