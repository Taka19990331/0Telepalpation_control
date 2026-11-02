#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\pthread_testcancel.c"
#include "pthread_impl.h"

static void dummy()
{
}

weak_alias(dummy, __testcancel);

void __pthread_testcancel()
{
	__testcancel();
}

weak_alias(__pthread_testcancel, pthread_testcancel);
