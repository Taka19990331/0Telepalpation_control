#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\signal\\siginterrupt.c"
#include <signal.h>

int siginterrupt(int sig, int flag)
{
	struct sigaction sa;

	sigaction(sig, 0, &sa);
	if (flag) sa.sa_flags &= ~SA_RESTART;
	else sa.sa_flags |= SA_RESTART;

	return sigaction(sig, &sa, 0);
}
