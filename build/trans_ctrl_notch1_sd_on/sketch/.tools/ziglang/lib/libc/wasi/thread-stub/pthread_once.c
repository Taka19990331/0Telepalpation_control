#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\thread-stub\\pthread_once.c"
#include "pthread_impl.h"

int __pthread_once(pthread_once_t *control, void (*init)(void))
{
       if (!*control) {
               init();
               *control = 1;
       }
       return 0;
}

weak_alias(__pthread_once, pthread_once);
