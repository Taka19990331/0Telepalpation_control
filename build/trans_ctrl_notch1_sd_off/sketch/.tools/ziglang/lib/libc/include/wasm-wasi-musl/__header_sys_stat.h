#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\wasm-wasi-musl\\__header_sys_stat.h"
#ifndef __wasilibc___header_sys_stat_h
#define __wasilibc___header_sys_stat_h

#include <__struct_stat.h>

#define st_atime st_atim.tv_sec
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec

#include <__mode_t.h>

#define UTIME_NOW (-1)
#define UTIME_OMIT (-2)

#endif
