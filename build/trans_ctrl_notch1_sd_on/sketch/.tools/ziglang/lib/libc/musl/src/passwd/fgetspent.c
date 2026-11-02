#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\passwd\\fgetspent.c"
#include "pwf.h"
#include <pthread.h>

struct spwd *fgetspent(FILE *f)
{
	static char *line;
	static struct spwd sp;
	size_t size = 0;
	struct spwd *res = 0;
	int cs;
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cs);
	if (getline(&line, &size, f) >= 0 && __parsespent(line, &sp) >= 0) res = &sp;
	pthread_setcancelstate(cs, 0);
	return res;
}
