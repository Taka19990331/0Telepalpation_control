#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\wasm-wasi-musl\\malloc.h"
#ifndef _MALLOC_H
#define _MALLOC_H

#ifdef __wasilibc_unmodified_upstream /* Use alternate WASI libc headers */
#else
#include <__functions_malloc.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_size_t

#include <bits/alltypes.h>

#ifdef __wasilibc_unmodified_upstream /* Use alternate WASI libc headers */
void *malloc (size_t);
void *calloc (size_t, size_t);
void *realloc (void *, size_t);
void free (void *);
#endif
#ifdef __wasilibc_unmodified_upstream /* WASI libc doesn't build the legacy functions */
void *valloc (size_t);
void *memalign(size_t, size_t);
#endif

size_t malloc_usable_size(void *);

#ifdef __cplusplus
}
#endif

#endif
