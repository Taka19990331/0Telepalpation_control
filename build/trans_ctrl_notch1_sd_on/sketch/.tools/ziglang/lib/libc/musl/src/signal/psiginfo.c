#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\signal\\psiginfo.c"
#include <signal.h>

void psiginfo(const siginfo_t *si, const char *msg)
{
	psignal(si->si_signo, msg);
}
