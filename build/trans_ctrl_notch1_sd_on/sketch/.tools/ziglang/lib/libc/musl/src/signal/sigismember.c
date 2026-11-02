#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\signal\\sigismember.c"
#include <signal.h>

int sigismember(const sigset_t *set, int sig)
{
	unsigned s = sig-1;
	if (s >= _NSIG-1) return 0;
	return !!(set->__bits[s/8/sizeof *set->__bits] & 1UL<<(s&8*sizeof *set->__bits-1));
}
