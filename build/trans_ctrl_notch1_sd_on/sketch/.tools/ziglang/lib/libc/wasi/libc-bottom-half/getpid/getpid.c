#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\libc-bottom-half\\getpid\\getpid.c"
#include <unistd.h>

pid_t getpid(void) {
    // Return an arbitrary value, greater than 1 which is special.
    return 42;
}
