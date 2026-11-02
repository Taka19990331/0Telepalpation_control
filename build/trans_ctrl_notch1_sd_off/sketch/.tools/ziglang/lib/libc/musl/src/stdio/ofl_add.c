#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdio\\ofl_add.c"
#include "stdio_impl.h"

FILE *__ofl_add(FILE *f)
{
	FILE **head = __ofl_lock();
	f->next = *head;
	if (*head) (*head)->prev = f;
	*head = f;
	__ofl_unlock();
	return f;
}
