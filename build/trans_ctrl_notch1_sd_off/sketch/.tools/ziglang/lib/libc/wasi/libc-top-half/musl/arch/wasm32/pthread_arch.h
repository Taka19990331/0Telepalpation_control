#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\libc-top-half\\musl\\arch\\wasm32\\pthread_arch.h"
extern _Thread_local struct __pthread __wasilibc_pthread_self;

static inline uintptr_t __get_tp() {
  return (uintptr_t)&__wasilibc_pthread_self;
}
