#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\string\\strerror_r.c"
#include <string.h>
#include <errno.h>

int strerror_r(int err, char *buf, size_t buflen)
{
	char *msg = strerror(err);
	size_t l = strlen(msg);
	if (l >= buflen) {
		if (buflen) {
			memcpy(buf, msg, buflen-1);
			buf[buflen-1] = 0;
		}
		return ERANGE;
	}
	memcpy(buf, msg, l+1);
	return 0;
}

weak_alias(strerror_r, __xpg_strerror_r);
