#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\wasm-wasi-musl\\__typedef_fd_set.h"
#ifndef __wasilibc___typedef_fd_set_h
#define __wasilibc___typedef_fd_set_h

#define __need_size_t
#include <stddef.h>

#include <__macro_FD_SETSIZE.h>

typedef struct {
    size_t __nfds;
    int __fds[FD_SETSIZE];
} fd_set;

#endif
