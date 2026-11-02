#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\signal\\sigfillset.c"
#include <signal.h>
#include <limits.h>

int sigfillset(sigset_t *set)
{
#if ULONG_MAX == 0xffffffff
	set->__bits[0] = 0x7ffffffful;
	set->__bits[1] = 0xfffffffcul;
	if (_NSIG > 65) {
		set->__bits[2] = 0xfffffffful;
		set->__bits[3] = 0xfffffffful;
	}
#else
	set->__bits[0] = 0xfffffffc7ffffffful;
	if (_NSIG > 65) set->__bits[1] = 0xfffffffffffffffful;
#endif
	return 0;
}
