#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\network\\getservbyport.c"
#define _GNU_SOURCE
#include <netdb.h>

struct servent *getservbyport(int port, const char *prots)
{
	static struct servent se;
	static long buf[32/sizeof(long)];
	struct servent *res;
	if (getservbyport_r(port, prots, &se, (void *)buf, sizeof buf, &res))
		return 0;
	return &se;
}
