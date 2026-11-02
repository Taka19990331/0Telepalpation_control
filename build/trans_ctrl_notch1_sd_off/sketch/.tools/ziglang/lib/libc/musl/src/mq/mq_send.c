#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\mq\\mq_send.c"
#include <mqueue.h>

int mq_send(mqd_t mqd, const char *msg, size_t len, unsigned prio)
{
	return mq_timedsend(mqd, msg, len, prio, 0);
}
