#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\wasm-wasi-musl\\__header_sys_resource.h"
#ifndef __wasilibc___header_sys_resource_h
#define __wasilibc___header_sys_resource_h

#include <__struct_rusage.h>

#define RUSAGE_SELF 1
#define RUSAGE_CHILDREN 2

#ifdef __cplusplus
extern "C" {
#endif

int getrusage(int who, struct rusage *usage);

#ifdef __cplusplus
}
#endif

#endif
