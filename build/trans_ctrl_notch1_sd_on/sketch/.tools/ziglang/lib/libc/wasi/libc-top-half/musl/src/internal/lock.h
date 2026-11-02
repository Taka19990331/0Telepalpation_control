#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\libc-top-half\\musl\\src\\internal\\lock.h"
#ifndef LOCK_H
#define LOCK_H

#if defined(__wasilibc_unmodified_upstream) || defined(_REENTRANT)
hidden void __lock(volatile int *);
hidden void __unlock(volatile int *);
#define LOCK(x) __lock(x)
#define UNLOCK(x) __unlock(x)
#else
// No locking needed.
#define LOCK(x) ((void)0)
#define UNLOCK(x) ((void)0)
#endif

#endif
