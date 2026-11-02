#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\mq\\mq_getattr.c"
#include <mqueue.h>
#include "syscall.h"

int mq_getattr(mqd_t mqd, struct mq_attr *attr)
{
	return mq_setattr(mqd, 0, attr);
}
