#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\env\\__reset_tls.c"
#include <string.h>
#include "pthread_impl.h"
#include "libc.h"

void __reset_tls()
{
	pthread_t self = __pthread_self();
	struct tls_module *p;
	size_t i, n = self->dtv[0];
	if (n) for (p=libc.tls_head, i=1; i<=n; i++, p=p->next) {
		char *mem = (char *)(self->dtv[i] - DTP_OFFSET);
		memcpy(mem, p->image, p->len);
		memset(mem+p->len, 0, p->size - p->len);
	}
}
