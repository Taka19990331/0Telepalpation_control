#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\signal\\sigwait.c"
#include <signal.h>

int sigwait(const sigset_t *restrict mask, int *restrict sig)
{
	siginfo_t si;
	if (sigtimedwait(mask, &si, 0) < 0)
		return -1;
	*sig = si.si_signo;
	return 0;
}
