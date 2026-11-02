#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\libc-top-half\\musl\\src\\internal\\fork_impl.h"
#include <features.h>

#ifdef __wasilibc_unmodified_upstream
extern hidden volatile int *const __at_quick_exit_lockptr;
extern hidden volatile int *const __atexit_lockptr;
extern hidden volatile int *const __gettext_lockptr;
extern hidden volatile int *const __locale_lockptr;
extern hidden volatile int *const __random_lockptr;
extern hidden volatile int *const __sem_open_lockptr;
extern hidden volatile int *const __stdio_ofl_lockptr;
extern hidden volatile int *const __syslog_lockptr;
extern hidden volatile int *const __timezone_lockptr;

extern hidden volatile int *const __bump_lockptr;

extern hidden volatile int *const __vmlock_lockptr;

hidden void __malloc_atfork(int);
hidden void __ldso_atfork(int);
hidden void __pthread_key_atfork(int);

hidden void __post_Fork(int);
#endif
