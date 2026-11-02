#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\signal\\sigpause.c"
#include <signal.h>

int sigpause(int sig)
{
	sigset_t mask;
	sigprocmask(0, 0, &mask);
	sigdelset(&mask, sig);
	return sigsuspend(&mask);
}
