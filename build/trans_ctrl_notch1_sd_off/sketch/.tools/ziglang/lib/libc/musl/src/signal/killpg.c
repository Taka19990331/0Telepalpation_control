#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\signal\\killpg.c"
#include <signal.h>
#include <errno.h>

int killpg(pid_t pgid, int sig)
{
	if (pgid < 0) {
		errno = EINVAL;
		return -1;
	}
	return kill(-pgid, sig);
}
