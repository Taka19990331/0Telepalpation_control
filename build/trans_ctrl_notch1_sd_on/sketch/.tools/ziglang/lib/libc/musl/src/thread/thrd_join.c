#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\thrd_join.c"
#include <stdint.h>
#include <threads.h>
#include <pthread.h>

int thrd_join(thrd_t t, int *res)
{
        void *pthread_res;
        __pthread_join(t, &pthread_res);
        if (res) *res = (int)(intptr_t)pthread_res;
        return thrd_success;
}
