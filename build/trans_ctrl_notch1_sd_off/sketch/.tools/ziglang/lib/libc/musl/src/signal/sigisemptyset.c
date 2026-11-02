#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\signal\\sigisemptyset.c"
#define _GNU_SOURCE
#include <signal.h>
#include <string.h>

int sigisemptyset(const sigset_t *set)
{
	for (size_t i=0; i<_NSIG/8/sizeof *set->__bits; i++)
		if (set->__bits[i]) return 0;
	return 1;
}
