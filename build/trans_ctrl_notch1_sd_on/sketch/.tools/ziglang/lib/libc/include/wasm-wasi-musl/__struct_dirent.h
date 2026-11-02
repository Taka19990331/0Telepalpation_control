#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\wasm-wasi-musl\\__struct_dirent.h"
#ifndef __wasilibc___struct_dirent_h
#define __wasilibc___struct_dirent_h

#include <__typedef_ino_t.h>

#define _DIRENT_HAVE_D_TYPE

struct dirent {
    ino_t d_ino;
    unsigned char d_type;
    char d_name[];
};

#endif
