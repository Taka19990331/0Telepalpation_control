#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\personality.c"
#include <sys/personality.h>
#include "syscall.h"
#ifdef SYS_personality
int personality(unsigned long persona)
{
	return syscall(SYS_personality, persona);
}
#endif
