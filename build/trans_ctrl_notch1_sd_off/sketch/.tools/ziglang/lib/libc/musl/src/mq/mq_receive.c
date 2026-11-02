#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\mq\\mq_receive.c"
#include <mqueue.h>

ssize_t mq_receive(mqd_t mqd, char *msg, size_t len, unsigned *prio)
{
	return mq_timedreceive(mqd, msg, len, prio, 0);
}
