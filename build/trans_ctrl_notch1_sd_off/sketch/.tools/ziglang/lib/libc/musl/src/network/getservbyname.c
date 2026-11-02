#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\network\\getservbyname.c"
#define _GNU_SOURCE
#include <netdb.h>

struct servent *getservbyname(const char *name, const char *prots)
{
	static struct servent se;
	static char *buf[2];
	struct servent *res;
	if (getservbyname_r(name, prots, &se, (void *)buf, sizeof buf, &res))
		return 0;
	return &se;
}
