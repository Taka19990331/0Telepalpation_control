#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\arch\\mips\\ksigaction.h"
#include <features.h>

struct k_sigaction {
	unsigned flags;
	void (*handler)(int);
	unsigned long mask[4];
	void *unused;
};

hidden void __restore(), __restore_rt();
