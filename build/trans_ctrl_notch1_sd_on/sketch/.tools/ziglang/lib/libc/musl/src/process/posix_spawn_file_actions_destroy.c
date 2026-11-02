#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\process\\posix_spawn_file_actions_destroy.c"
#include <spawn.h>
#include <stdlib.h>
#include "fdop.h"

int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *fa)
{
	struct fdop *op = fa->__actions, *next;
	while (op) {
		next = op->next;
		free(op);
		op = next;
	}
	return 0;
}
