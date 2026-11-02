#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\wasm-wasi-musl\\__struct_timeval.h"
#ifndef __wasilibc___struct_timeval_h
#define __wasilibc___struct_timeval_h

#include <__typedef_time_t.h>
#include <__typedef_suseconds_t.h>

/* As specified in POSIX. */
struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};

#endif
